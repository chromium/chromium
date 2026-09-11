// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_buffer_source_handler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <tuple>

#include "base/containers/span.h"
#include "base/memory/stack_allocated.h"
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

class AudioBufferSourceHandlerTestBase : public testing::Test {
 protected:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<NonMainThread> audio_thread_;

  void SetUp() override {
    audio_thread_ = NonMainThread::CreateThread(
        ThreadCreationParams(ThreadType::kTestThread));
  }

  void TearDown() override { audio_thread_.reset(); }

  std::array<float, kRenderQuantumFrames> ProcessOnAudioThread(
      OfflineAudioContext* context,
      AudioBufferSourceHandler& handler) {
    std::array<float, kRenderQuantumFrames> destination{};
    scoped_refptr<DeferredTaskHandler> task_handler =
        &context->GetDeferredTaskHandler();
    scoped_refptr<AudioBufferSourceHandler> handler_ref = &handler;

    RunOnAudioThread(
        audio_thread_.get(),
        CrossThreadBindOnce(
            [](scoped_refptr<DeferredTaskHandler> task_handler,
               scoped_refptr<AudioBufferSourceHandler> handler,
               base::span<float> dest_data) {
              task_handler->SetAudioThreadToCurrentThread();
              DeferredTaskHandler::GraphAutoLocker locker(*task_handler);

              handler->Process(kRenderQuantumFrames);

              AudioBus* output_bus = handler->Output(0).Bus();
              ASSERT_NE(output_bus, nullptr);
              ASSERT_GT(output_bus->NumberOfChannels(), 0u);
              base::span<const float> channel_span =
                  output_bus->Channel(0)->Span();
              ASSERT_GE(channel_span.size(), kRenderQuantumFrames);
              dest_data.copy_from(channel_span.first(kRenderQuantumFrames));
            },
            std::move(task_handler), std::move(handler_ref),
            base::span(destination)));

    return destination;
  }

  template <typename Generator>
  static AudioBuffer* CreateAudioBuffer(uint32_t length,
                                        float sample_rate,
                                        Generator generator) {
    AudioBuffer* audio_buffer =
        AudioBuffer::Create(1, length, sample_rate, ASSERT_NO_EXCEPTION);
    DOMFloat32Array* channel_data =
        audio_buffer->getChannelData(0, ASSERT_NO_EXCEPTION).Get();
    base::span<float> span = channel_data->AsSpan();
    for (uint32_t i = 0; i < length; ++i) {
      span[i] = generator(i);
    }
    return audio_buffer;
  }

  static AudioBuffer* CreateRampBuffer(uint32_t length,
                                       float sample_rate,
                                       float start = 1.0f) {
    return CreateAudioBuffer(length, sample_rate, [start](uint32_t i) {
      return start + static_cast<float>(i);
    });
  }

  static AudioBuffer* CreateConstantBuffer(uint32_t length,
                                           float sample_rate,
                                           float val) {
    return CreateAudioBuffer(length, sample_rate,
                             [val](uint32_t) { return val; });
  }

  struct TestEnvironment {
    STACK_ALLOCATED();

   public:
    OfflineAudioContext* context;
    AudioBufferSourceNode* node;
    AudioBufferSourceHandler& handler;
  };

  TestEnvironment CreateEnvironmentWithBuffer(V8TestingScope& scope,
                                              AudioBuffer* buffer,
                                              bool loop = true) {
    OfflineAudioContext* context =
        OfflineAudioContext::Create(&scope.GetWindow(), 1, kRenderQuantumFrames,
                                    buffer->sampleRate(), ASSERT_NO_EXCEPTION);
    AudioBufferSourceNode* node =
        AudioBufferSourceNode::Create(*context, ASSERT_NO_EXCEPTION);
    AudioBufferSourceHandler& handler = node->GetAudioBufferSourceHandler();
    handler.SetBuffer(buffer, ASSERT_NO_EXCEPTION);
    handler.SetLoop(loop);
    return TestEnvironment{context, node, handler};
  }
};

class AudioBufferSourceHandlerParamTest
    : public AudioBufferSourceHandlerTestBase,
      public testing::WithParamInterface<
          std::tuple<AudioBufferSourceTestParams, double>> {};

TEST_P(AudioBufferSourceHandlerParamTest,
       ProcessInterpolatedPathBoundaryMatrix) {
  V8TestingScope scope;

  const AudioBufferSourceTestParams& params = std::get<0>(GetParam());
  const double rate_multiplier = std::get<1>(GetParam());
  const double final_playback_rate = params.playback_rate * rate_multiplier;

  auto env = CreateEnvironmentWithBuffer(
      scope,
      CreateConstantBuffer(kTestBufferLengthFrames, params.sample_rate, 1.0f),
      params.should_loop);
  OfflineAudioContext* context = env.context;
  AudioBufferSourceNode* node = env.node;
  AudioBufferSourceHandler& handler = env.handler;

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
      audio_thread_.get(),
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
            double read_index = handler->VirtualReadIndexForTesting();
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
            // Non-looping play-through reaching buffer end boundary
            // (read_index2 >= buffer_length)
            AudioBufferSourceTestParams{44100.0f, 1.5, 0.0, 0.0, 0.0, false},
            // Start out of bounds non-looping at 96 kHz
            AudioBufferSourceTestParams{96000.0f, 1.5, 0.0, 0.0, 2.0, false},
            // Zero-length grain start at 48 kHz
            AudioBufferSourceTestParams{48000.0f, 1.0, 0.0, 0.0, 0.0, false}),
        testing::Values(1.0, -1.0)));

// For unaligned buffer lengths (e.g., 65201 frames at 48 kHz), loop boundaries
// must be exact integer frames to enter ProcessFastPath and avoid frame
// duplication.
TEST_F(AudioBufferSourceHandlerTestBase, LoopingUnalignedBufferPrecision) {
  V8TestingScope scope;

  constexpr uint32_t kUnalignedBufferLength = 65201;
  constexpr float kSampleRate = 48000.0f;

  auto env = CreateEnvironmentWithBuffer(
      scope, CreateRampBuffer(kUnalignedBufferLength, kSampleRate));

  // Start playback at frame 65200 (1 frame before loop boundary).
  const double grain_offset = 65200.0 / kSampleRate;
  env.handler.Start(0, grain_offset, ASSERT_NO_EXCEPTION);

  const auto destination = ProcessOnAudioThread(env.context, env.handler);

  // Exact integer frames preserved, entering ProcessFastPath.
  EXPECT_TRUE(env.handler.IsUsingFastPathForTesting());
  // Destination sample 0 corresponds to frame 65200 (value 65201.0f).
  EXPECT_FLOAT_EQ(destination[0], 65201.0f);
  // Destination sample 1 must wrap to frame 0 (value 1.0f).
  // Precomputed integer loop boundaries eliminate IEEE-754 residue,
  // ensuring sample-accurate loop wrapping to frame 0.
  EXPECT_FLOAT_EQ(destination[1], 1.0f);
  EXPECT_FLOAT_EQ(destination[2], 2.0f);
}

// Explicitly setting loopEnd to buffer->duration() must also resolve to exact
// integer frames, preserving fast-path execution and exact loop wrapping.
TEST_F(AudioBufferSourceHandlerTestBase,
       LoopingUnalignedBufferExplicitLoopEnd) {
  V8TestingScope scope;

  constexpr uint32_t kUnalignedBufferLength = 65201;
  constexpr float kSampleRate = 48000.0f;

  AudioBuffer* buffer = CreateRampBuffer(kUnalignedBufferLength, kSampleRate);
  auto env = CreateEnvironmentWithBuffer(scope, buffer);

  // Explicitly set loopEnd to buffer duration (in seconds), matching the common
  // web pattern: `source.loopEnd = buffer.duration`.
  env.handler.SetLoopEnd(buffer->duration());

  // Start playback at frame 65200 (1 frame before loop boundary).
  const double grain_offset = 65200.0 / kSampleRate;
  env.handler.Start(0, grain_offset, ASSERT_NO_EXCEPTION);

  const auto destination = ProcessOnAudioThread(env.context, env.handler);

  // Exact integer frames preserved, entering ProcessFastPath.
  EXPECT_TRUE(env.handler.IsUsingFastPathForTesting());
  EXPECT_FLOAT_EQ(destination[0], 65201.0f);
  EXPECT_FLOAT_EQ(destination[1], 1.0f);
  EXPECT_FLOAT_EQ(destination[2], 2.0f);
}

TEST_F(AudioBufferSourceHandlerTestBase,
       LoopingUnalignedBufferFractionalLoopEndInterpolation) {
  V8TestingScope scope;

  constexpr uint32_t kUnalignedBufferLength = 65201;
  constexpr float kSampleRate = 48000.0f;

  auto env = CreateEnvironmentWithBuffer(
      scope, CreateRampBuffer(kUnalignedBufferLength, kSampleRate));

  // Set a deliberate fractional loopEnd at frame 65200.5 (non-zero mantissa).
  env.handler.SetLoopEnd(65200.5 / kSampleRate);

  // Start playback at frame 65200.
  const double grain_offset = 65200.0 / kSampleRate;
  env.handler.Start(0, grain_offset, ASSERT_NO_EXCEPTION);

  const auto destination = ProcessOnAudioThread(env.context, env.handler);

  // Non-zero mantissa forces ProcessInterpolatedPath.
  EXPECT_FALSE(env.handler.IsUsingFastPathForTesting());

  // Frame 65200 is 65201.0f. At frame 65200.0, linear interpolation between
  // frame 65200 and wrapped frame 0 (1.0f) yields:
  // (1 - 0) * 65201.0f + 0 * 1.0f = 65201.0f.
  EXPECT_FLOAT_EQ(destination[0], 65201.0f);
  // At sample 1, playhead wraps to 0.5 (interpolating between 1.0f and 2.0f).
  EXPECT_FLOAT_EQ(destination[1], 1.5f);
}

TEST_F(AudioBufferSourceHandlerTestBase, LoopOffsetBeyondLoopEndPositiveRate) {
  V8TestingScope scope;

  constexpr float kSampleRate = 48000.0f;

  auto env =
      CreateEnvironmentWithBuffer(scope, CreateRampBuffer(8, kSampleRate));
  env.handler.SetLoopStart(2.0 / kSampleRate);
  env.handler.SetLoopEnd(5.0 / kSampleRate);

  // Start with offset = 6 (beyond loopEnd = 5). For positive playback rate,
  // it must clamp/reset immediately to loopStart (frame 2).
  const double grain_offset = 6.0 / kSampleRate;
  env.handler.Start(0, grain_offset, ASSERT_NO_EXCEPTION);

  const auto destination = ProcessOnAudioThread(env.context, env.handler);

  // Starts at loopStart (frame 2 -> value 3.0f).
  EXPECT_FLOAT_EQ(destination[0], 3.0f);
  EXPECT_FLOAT_EQ(destination[1], 4.0f);
  EXPECT_FLOAT_EQ(destination[2], 5.0f);
  // Wraps back to loopStart (frame 2 -> value 3.0f).
  EXPECT_FLOAT_EQ(destination[3], 3.0f);
  EXPECT_FLOAT_EQ(destination[4], 4.0f);
  EXPECT_FLOAT_EQ(destination[5], 5.0f);
}

TEST_F(AudioBufferSourceHandlerTestBase, LoopOffsetBelowLoopStartPositiveRate) {
  V8TestingScope scope;

  constexpr float kSampleRate = 48000.0f;

  auto env =
      CreateEnvironmentWithBuffer(scope, CreateRampBuffer(8, kSampleRate));
  env.handler.SetLoopStart(3.0 / kSampleRate);
  env.handler.SetLoopEnd(6.0 / kSampleRate);

  // Start with offset = 1 (below loopStart = 3). Plays intro frames 1 and 2,
  // enters loop at frame 3, plays to frame 5, then loops from frame 3.
  const double grain_offset = 1.0 / kSampleRate;
  env.handler.Start(0, grain_offset, ASSERT_NO_EXCEPTION);

  const auto destination = ProcessOnAudioThread(env.context, env.handler);

  // Intro: frames 1 and 2 (values 2.0f and 3.0f)
  EXPECT_FLOAT_EQ(destination[0], 2.0f);
  EXPECT_FLOAT_EQ(destination[1], 3.0f);
  // Loop section: frames 3, 4, 5 (values 4.0f, 5.0f, 6.0f)
  EXPECT_FLOAT_EQ(destination[2], 4.0f);
  EXPECT_FLOAT_EQ(destination[3], 5.0f);
  EXPECT_FLOAT_EQ(destination[4], 6.0f);
  // Loop wrap: wraps back to loopStart (frame 3 -> value 4.0f)
  EXPECT_FLOAT_EQ(destination[5], 4.0f);
  EXPECT_FLOAT_EQ(destination[6], 5.0f);
  EXPECT_FLOAT_EQ(destination[7], 6.0f);
}

struct GranularityTestParam {
  uint32_t buffer_length;
  float sample_rate;
};

class AudioBufferSourceGranularityTest
    : public AudioBufferSourceHandlerTestBase,
      public testing::WithParamInterface<GranularityTestParam> {};

TEST_P(AudioBufferSourceGranularityTest, LoopingGranularityWrapPrecision) {
  V8TestingScope scope;

  const GranularityTestParam& param = GetParam();
  const uint32_t buffer_length = param.buffer_length;
  const float sample_rate = param.sample_rate;

  auto env = CreateEnvironmentWithBuffer(
      scope, CreateRampBuffer(buffer_length, sample_rate));

  // Start 1 frame before loop end.
  const double grain_offset =
      static_cast<double>(buffer_length - 1) / sample_rate;
  env.handler.Start(0, grain_offset, ASSERT_NO_EXCEPTION);

  const auto destination = ProcessOnAudioThread(env.context, env.handler);

  // Whole buffer loops at rate 1.0 use ProcessFastPath.
  EXPECT_TRUE(env.handler.IsUsingFastPathForTesting());

  for (uint32_t k = 0; k < kRenderQuantumFrames; ++k) {
    const float expected =
        static_cast<float>((buffer_length - 1 + k) % buffer_length + 1);
    EXPECT_FLOAT_EQ(destination[k], expected)
        << "Mismatch at sample " << k << " for buffer_length=" << buffer_length;
  }
}

INSTANTIATE_TEST_SUITE_P(VaryingGranularities,
                         AudioBufferSourceGranularityTest,
                         testing::Values(
                             // Sub-quantum prime & odd granularities
                             GranularityTestParam{7, 48000.0f},
                             GranularityTestParam{31, 44100.0f},
                             GranularityTestParam{67, 48000.0f},
                             GranularityTestParam{127, 48000.0f},
                             // Quantum boundary & adjacent offsets
                             GranularityTestParam{128, 48000.0f},
                             GranularityTestParam{129, 48000.0f},
                             GranularityTestParam{255, 44100.0f},
                             GranularityTestParam{257, 48000.0f},
                             // Codec frame boundaries
                             GranularityTestParam{1152, 48000.0f},
                             GranularityTestParam{1153, 48000.0f},
                             GranularityTestParam{1486, 48000.0f},
                             GranularityTestParam{2049, 96000.0f},
                             // Large unaligned & prime buffers
                             GranularityTestParam{65201, 44100.0f},
                             GranularityTestParam{131071, 48000.0f}));

TEST_F(AudioBufferSourceHandlerTestBase,
       VaryingRatesInterpolatedWrapPrecision) {
  V8TestingScope scope;

  // LAME padding case from crbug.com/553218226 Comment #7
  constexpr uint32_t kBufferLength = 1486;
  constexpr float kSampleRate = 48000.0f;

  auto env = CreateEnvironmentWithBuffer(
      scope, CreateRampBuffer(kBufferLength, kSampleRate));

  // Playback rate 0.5: playhead advances 0.5 frames per sample.
  env.node->playbackRate()->setValue(0.5);

  // Start 1 frame before loop end.
  const double grain_offset =
      static_cast<double>(kBufferLength - 1) / kSampleRate;
  env.handler.Start(0, grain_offset, ASSERT_NO_EXCEPTION);

  const auto destination = ProcessOnAudioThread(env.context, env.handler);

  // Non-integer playback rate forces ProcessInterpolatedPath.
  EXPECT_FALSE(env.handler.IsUsingFastPathForTesting());

  // Sample 0: exact frame 1485 (value 1486.0f)
  EXPECT_FLOAT_EQ(destination[0], 1486.0f);

  // Sample 1: playhead at 1485.5, linear interpolation between
  // frame 1485 (1486.0f) and wrapped frame 0 (1.0f):
  // (1486.0 + 1.0) / 2 = 743.5f
  EXPECT_FLOAT_EQ(destination[1], 743.5f);

  // Sample 2: playhead at wrapped frame 0 (value 1.0f)
  EXPECT_FLOAT_EQ(destination[2], 1.0f);

  // Sample 3: playhead at 0.5 (linear interpolation between 1.0f
  // and 2.0f = 1.5f)
  EXPECT_FLOAT_EQ(destination[3], 1.5f);
}

TEST_F(AudioBufferSourceHandlerTestBase, LoopSingleSampleBuffer) {
  V8TestingScope scope;

  auto env = CreateEnvironmentWithBuffer(
      scope, CreateConstantBuffer(1, 48000.0f, 0.42f));
  env.handler.Start(0, 0, ASSERT_NO_EXCEPTION);

  const auto destination = ProcessOnAudioThread(env.context, env.handler);

  for (uint32_t i = 0; i < kRenderQuantumFrames; ++i) {
    EXPECT_FLOAT_EQ(destination[i], 0.42f);
  }
}

TEST_F(AudioBufferSourceHandlerTestBase,
       LoopInvertedBoundsFallbackToWholeBuffer) {
  V8TestingScope scope;

  constexpr float kSampleRate = 48000.0f;

  auto env =
      CreateEnvironmentWithBuffer(scope, CreateRampBuffer(8, kSampleRate));
  env.handler.SetLoopStart(5.0 / kSampleRate);
  env.handler.SetLoopEnd(2.0 / kSampleRate);
  env.handler.Start(0, 0, ASSERT_NO_EXCEPTION);

  const auto destination = ProcessOnAudioThread(env.context, env.handler);

  for (uint32_t i = 0; i < 8; ++i) {
    EXPECT_FLOAT_EQ(destination[i], static_cast<float>(i + 1));
  }
  EXPECT_FLOAT_EQ(destination[8], 1.0f);
}

TEST_F(AudioBufferSourceHandlerTestBase, LoopNegativeRateBackwardWrap) {
  V8TestingScope scope;

  constexpr float kSampleRate = 48000.0f;

  auto env = CreateEnvironmentWithBuffer(
      scope, CreateRampBuffer(10, kSampleRate, /*start=*/0.0f));
  env.handler.SetLoopStart(2.0 / kSampleRate);
  env.handler.SetLoopEnd(7.0 / kSampleRate);
  env.node->playbackRate()->setValue(-1.0f);
  env.handler.Start(0, 6.0 / kSampleRate, ASSERT_NO_EXCEPTION);

  const auto destination = ProcessOnAudioThread(env.context, env.handler);

  EXPECT_FLOAT_EQ(destination[0], 6.0f);
  EXPECT_FLOAT_EQ(destination[1], 5.0f);
  EXPECT_FLOAT_EQ(destination[2], 4.0f);
  EXPECT_FLOAT_EQ(destination[3], 3.0f);
  EXPECT_FLOAT_EQ(destination[4], 2.0f);
  // Wraps back to loopEnd (frame 7) minus 1 = frame 6.
  EXPECT_FLOAT_EQ(destination[5], 6.0f);
  EXPECT_FLOAT_EQ(destination[6], 5.0f);
}

// Sub-ranges specified in seconds where frame conversion introduces IEEE-754
// roundoff must snap to exact integer frames.
TEST_F(AudioBufferSourceHandlerTestBase, LoopingExplicitSubRangeIntegerWrap) {
  V8TestingScope scope;
  constexpr float kSampleRate = 48000.0f;

  // 1. Explicit loopEnd with positive residue: (7/48000)*48000 > 7.
  {
    auto env =
        CreateEnvironmentWithBuffer(scope, CreateRampBuffer(100, kSampleRate));
    env.handler.SetLoopStart(0.0);
    env.handler.SetLoopEnd(7.0 / kSampleRate);
    env.handler.Start(0, 0.0, ASSERT_NO_EXCEPTION);

    const auto destination = ProcessOnAudioThread(env.context, env.handler);

    EXPECT_TRUE(env.handler.IsUsingFastPathForTesting());
    for (uint32_t i = 0; i < 7; ++i) {
      EXPECT_FLOAT_EQ(destination[i], static_cast<float>(i + 1));
    }
    EXPECT_FLOAT_EQ(destination[7], 1.0f);
    EXPECT_FLOAT_EQ(destination[8], 2.0f);
  }

  // 2. Explicit loopEnd with negative residue: (27/48000)*48000 < 27.
  {
    auto env =
        CreateEnvironmentWithBuffer(scope, CreateRampBuffer(100, kSampleRate));
    env.handler.SetLoopStart(0.0);
    env.handler.SetLoopEnd(27.0 / kSampleRate);
    env.handler.Start(0, 0.0, ASSERT_NO_EXCEPTION);

    const auto destination = ProcessOnAudioThread(env.context, env.handler);

    EXPECT_TRUE(env.handler.IsUsingFastPathForTesting());
    EXPECT_FLOAT_EQ(destination[26], 27.0f);
    EXPECT_FLOAT_EQ(destination[27], 1.0f);
    EXPECT_FLOAT_EQ(destination[28], 2.0f);
  }
}

TEST_F(AudioBufferSourceHandlerTestBase,
       LoopCoincidingSnappedBoundsFallbackToWholeBuffer) {
  V8TestingScope scope;
  constexpr float kSampleRate = 48000.0f;

  auto env =
      CreateEnvironmentWithBuffer(scope, CreateRampBuffer(8, kSampleRate));

  // Choose loopStart and loopEnd such that loopStart < loopEnd in seconds,
  // but both snap to the same integer frame (frame 3) within machine
  // tolerance.
  const double start_time = 3.0 / kSampleRate;
  const double end_time = std::nextafter(start_time, 1.0);
  ASSERT_LT(start_time, end_time);

  env.handler.SetLoopStart(start_time);
  env.handler.SetLoopEnd(end_time);
  env.handler.Start(0, 0, ASSERT_NO_EXCEPTION);

  const auto destination = ProcessOnAudioThread(env.context, env.handler);

  // Snapped start == end falls back to looping the entire buffer [0, 8].
  for (uint32_t i = 0; i < 8; ++i) {
    EXPECT_FLOAT_EQ(destination[i], static_cast<float>(i + 1));
  }
  EXPECT_TRUE(env.handler.IsUsingFastPathForTesting());
  EXPECT_FLOAT_EQ(destination[8], 1.0f);
}

}  // namespace blink
