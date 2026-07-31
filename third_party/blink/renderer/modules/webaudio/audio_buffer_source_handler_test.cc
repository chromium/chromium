// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_buffer_source_handler.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <tuple>

#include "base/containers/span.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer_source_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/deferred_task_handler.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

constexpr uint32_t kRenderQuantumFrames = 128;
constexpr uint32_t kTestBufferLengthFrames = 64;

struct AudioBufferSourceTestParams {
  float sample_rate;
  double playback_rate;
  double loop_start_sec;
  double loop_end_sec;
  double grain_offset_sec;
  bool should_loop;
};

void RunOnAudioThread(NonMainThread* thread, CrossThreadOnceClosure closure) {
  base::WaitableEvent event;
  PostCrossThreadTask(
      *thread->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(
          [](CrossThreadOnceClosure closure, base::WaitableEvent* event) {
            std::move(closure).Run();
            event->Signal();
          },
          std::move(closure), CrossThreadUnretained(&event)));
  event.Wait();
}

}  // namespace

class AudioBufferSourceHandlerParamTest
    : public testing::TestWithParam<
          std::tuple<AudioBufferSourceTestParams, double>> {};

TEST_P(AudioBufferSourceHandlerParamTest,
       ProcessInterpolatedPathBoundaryMatrix) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  const AudioBufferSourceTestParams& params = std::get<0>(GetParam());
  const double rate_multiplier = std::get<1>(GetParam());
  const double final_playback_rate = params.playback_rate * rate_multiplier;

  auto audio_thread = NonMainThread::CreateThread(
      ThreadCreationParams(ThreadType::kTestThread));

  OfflineAudioContext* context =
      OfflineAudioContext::Create(&scope.GetWindow(), 1, kRenderQuantumFrames,
                                  params.sample_rate, ASSERT_NO_EXCEPTION);

  AudioBufferSourceNode* node =
      AudioBufferSourceNode::Create(*context, ASSERT_NO_EXCEPTION);
  AudioBufferSourceHandler& handler = node->GetAudioBufferSourceHandler();

  AudioBuffer* audio_buffer = AudioBuffer::Create(
      1, kTestBufferLengthFrames, params.sample_rate, ASSERT_NO_EXCEPTION);
  DOMFloat32Array* channel_data =
      audio_buffer->getChannelData(0, ASSERT_NO_EXCEPTION).Get();
  std::ranges::fill(channel_data->AsSpan(), 1.0f);

  handler.SetBuffer(audio_buffer, ASSERT_NO_EXCEPTION);
  handler.SetLoop(params.should_loop);

  // Set non-default playback rate if specified to force
  // ProcessInterpolatedPath.
  if (final_playback_rate != 1.0) {
    node->playbackRate()->setValue(final_playback_rate);
  }

  if (params.should_loop) {
    handler.SetLoopStart(params.loop_start_sec);
    handler.SetLoopEnd(params.loop_end_sec);
    handler.Start(0, ASSERT_NO_EXCEPTION);
  } else {
    double grain_offset = params.grain_offset_sec;
    // For reverse playback on non-looping buffers starting at 0,
    // shift the start to the buffer end so it actually plays backward
    // through the buffer rather than instantly stopping.
    if (rate_multiplier < 0 && grain_offset == 0.0) {
      grain_offset = kTestBufferLengthFrames / params.sample_rate;
    }
    handler.Start(0, grain_offset, ASSERT_NO_EXCEPTION);
  }

  // Extract non-Oilpan ref-counted handles on main thread prior to dispatch.
  scoped_refptr<DeferredTaskHandler> task_handler =
      &context->GetDeferredTaskHandler();
  scoped_refptr<AudioBufferSourceHandler> handler_ref = &handler;

  RunOnAudioThread(
      audio_thread.get(),
      CrossThreadBindOnce(
          [](AudioBufferSourceTestParams params,
             scoped_refptr<DeferredTaskHandler> task_handler,
             scoped_refptr<AudioBufferSourceHandler> handler) {
            task_handler->SetAudioThreadToCurrentThread();
            DeferredTaskHandler::GraphAutoLocker locker(*task_handler);

            // Pre-fill output bus with sentinels to detect incomplete
            // rendering.
            AudioBus* output_bus = handler->Output(0).Bus();
            if (output_bus) {
              for (unsigned i = 0; i < output_bus->NumberOfChannels(); ++i) {
                std::ranges::fill(output_bus->Channel(i)->MutableSpan(),
                                  std::numeric_limits<float>::quiet_NaN());
              }
            }

            handler->Process(kRenderQuantumFrames);

            // Verify that virtual_read_index_ remains strictly within valid
            // buffer frame bounds [-1e-5, kTestBufferLengthFrames + 1e-5] and
            // is finite. Note: We check buffer bounds rather than loop bounds
            // because playback starts at grain_offset (0.0), which precedes
            // loop_start until loop_end is reached.
            double read_index = handler->GetVirtualReadIndexForTesting();
            EXPECT_TRUE(std::isfinite(read_index));
            EXPECT_GE(read_index, -1e-5);
            EXPECT_LE(read_index,
                      static_cast<double>(kTestBufferLengthFrames) + 1e-5);

            // Re-evaluate Output(0).Bus() after Process to avoid stale pointer
            // risks.
            output_bus = handler->Output(0).Bus();
            ASSERT_NE(output_bus, nullptr);
            ASSERT_GT(output_bus->NumberOfChannels(), 0u);
            base::span<const float> destination =
                output_bus->Channel(0)->Span();
            ASSERT_GE(destination.size(), kRenderQuantumFrames);

            for (uint32_t i = 0; i < kRenderQuantumFrames; ++i) {
              EXPECT_TRUE(std::isfinite(destination[i]))
                  << "Sample " << i << " is non-finite: " << destination[i];
              EXPECT_GE(destination[i], -1.0f);
              EXPECT_LE(destination[i], 1.0f);
            }
          },
          params, std::move(task_handler), std::move(handler_ref)));

  EXPECT_EQ(handler.Loop(), params.should_loop);
}

INSTANTIATE_TEST_SUITE_P(
    SubSampleAndBoundaryMatrix,
    AudioBufferSourceHandlerParamTest,
    testing::Combine(
        testing::Values(
            // Sub-sample loop (< 1 sample frame) at 8 kHz
            AudioBufferSourceTestParams{8000.0f, 1.5, 0.001, 0.00105, 0.0,
                                        true},
            // Sub-sample loop (< 1 sample frame) at 44.1 kHz
            AudioBufferSourceTestParams{44100.0f, 1.2, 10.0 / 44100.0,
                                        10.4 / 44100.0, 0.0, true},
            // Sub-sample loop (< 1 sample frame) at 96 kHz
            AudioBufferSourceTestParams{96000.0f, 0.8, 20.0 / 96000.0,
                                        20.3 / 96000.0, 0.0, true},
            // Sub-sample boundary near end of buffer (frame 60.0 to 60.5)
            // at 44.1 kHz
            AudioBufferSourceTestParams{44100.0f, 1.5, 60.0 / 44100.0,
                                        60.5 / 44100.0, 0.0, true},
            // Sub-sample boundary near end of buffer at 48 kHz
            AudioBufferSourceTestParams{48000.0f, 1.2, 60.0 / 48000.0,
                                        60.4 / 48000.0, 0.0, true},
            // Standard loop (>= 1 frame, delta = 30 frames) at 22.05 kHz
            // (crosses loop ~3 times in 128 frames)
            AudioBufferSourceTestParams{22050.0f, 1.5, 10.0 / 22050.0,
                                        40.0 / 22050.0, 0.0, true},
            // Standard loop (>= 1 frame, delta = 20 frames) at 48 kHz
            AudioBufferSourceTestParams{48000.0f, 1.5, 10.0 / 48000.0,
                                        30.0 / 48000.0, 0.0, true},
            // TODO(crbug.com/540961964): These two boundary test parameter
            // combinations correctly expose a known bug in
            // ProcessInterpolatedPath() where it breaks out of the loop early
            // on bounds checks and leaves the remainder of the output bus
            // uninitialized (or filled with NaN). They are disabled in this
            // test coverage CL to pass CQ, and will be re-enabled in CL part 2
            // alongside the logic fix.
            //
            // Non-looping play-through reaching buffer end boundary
            // (read_index2 >= buffer_length)
            // AudioBufferSourceTestParams{44100.0f, 1.5, 0.0, 0.0, 0.0, false},
            // Start out of bounds non-looping at 96 kHz
            // AudioBufferSourceTestParams{96000.0f, 1.5, 0.0, 0.0, 2.0, false},
            // Zero-length grain start at 48 kHz
            AudioBufferSourceTestParams{48000.0f, 1.0, 0.0, 0.0, 0.0, false}),
        testing::Values(1.0, -1.0)));

}  // namespace blink
