// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/delay_handler.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/delay_node.h"
#include "third_party/blink/renderer/modules/webaudio/gain_node.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/testing/fake_audio_thread.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class DelayHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    page_ = std::make_unique<DummyPageHolder>();
    context_ = OfflineAudioContext::Create(
        page_->GetFrame().DomWindow(), 2, 1, 48000, ASSERT_NO_EXCEPTION);

    source_ = context_->createGain(ASSERT_NO_EXCEPTION);
    delay_node_ = context_->createDelay(ASSERT_NO_EXCEPTION);
    source_->connect(delay_node_, 0, 0, ASSERT_NO_EXCEPTION);
  }

  DelayHandler* GetHandler() const {
    return &static_cast<DelayHandler&>(delay_node_->Handler());
  }

  Vector<Delay*> GetKernels() const {
    return GetHandler()->KernelsForTesting();
  }

  void SetDelayTime(float seconds) {
    GetHandler()->delay_time_->SetValue(seconds);
  }

  void SetSourceChannelCount(unsigned channels) {
    DummyExceptionStateForTesting exception_state;
    source_->Handler().SetChannelCount(channels, exception_state);
    {
      DeferredTaskHandler::GraphAutoLocker locker(
          context_->GetDeferredTaskHandler());
      source_->Handler().Output(0).SetNumberOfChannels(channels);
    }
  }

  void ApplyDeferredTasks() {
    audio_thread_.RunOnAudioThreadWithContext(
        context_.Get(),
        CrossThreadBindOnce(
            [](OfflineAudioContext* context) {
              DeferredTaskHandler::GraphAutoLocker locker(
                  context->GetDeferredTaskHandler());
              context->GetDeferredTaskHandler().HandleDeferredTasks();
            },
            WrapCrossThreadPersistent(context_.Get())));
  }

  void SetSourceAudioValues(Vector<Vector<float>> values) {
    audio_thread_.RunOnAudioThreadWithContext(
        context_.Get(),
        CrossThreadBindOnce(
            [](OfflineAudioContext* context, AudioNode* source,
               Vector<Vector<float>> values) {
              DeferredTaskHandler::GraphAutoLocker locker(
                  context->GetDeferredTaskHandler());
              AudioBus* bus = source->Handler().Output(0).Bus();
              for (wtf_size_t c = 0; c < values.size(); ++c) {
                base::span<float> channel_span =
                    bus->Channel(c)->MutableSpan();
                for (wtf_size_t i = 0; i < values[c].size(); ++i) {
                  channel_span[i] = values[c][i];
                }
              }
            },
            WrapCrossThreadPersistent(context_.Get()),
            WrapCrossThreadPersistent(source_.Get()), std::move(values)));
  }

  Vector<float> GetOutputChannelData(unsigned channel_index,
                                     unsigned frames = 128) {
    Vector<float> result(frames);
    audio_thread_.RunOnAudioThreadWithContext(
        context_.Get(),
        CrossThreadBindOnce(
            [](OfflineAudioContext* context, DelayHandler* handler,
               unsigned channel_index, base::span<float> dest) {
              DeferredTaskHandler::GraphAutoLocker locker(
                  context->GetDeferredTaskHandler());
              AudioBus* bus = handler->Output(0).Bus();
              base::span<const float> src =
                  bus->Channel(channel_index)->Span().first(dest.size());
              dest.copy_from(src);
            },
            WrapCrossThreadPersistent(context_.Get()),
            CrossThreadUnretained(GetHandler()), channel_index,
            base::span(result)));
    return result;
  }

  void RunProcessOnAudioThread(uint32_t frames = 128) {
    audio_thread_.RunOnAudioThreadWithContext(
        context_.Get(),
        CrossThreadBindOnce(
            [](OfflineAudioContext* context, DelayHandler* handler,
               uint32_t frames) {
              DeferredTaskHandler::GraphAutoLocker locker(
                  context->GetDeferredTaskHandler());
              handler->Process(frames);
            },
            WrapCrossThreadPersistent(context_.Get()),
            CrossThreadUnretained(GetHandler()), frames));
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> page_;
  Persistent<OfflineAudioContext> context_;
  FakeAudioThread audio_thread_;
  Persistent<AudioNode> source_;
  Persistent<AudioNode> delay_node_;
};

TEST_F(DelayHandlerTest, ChannelMigrationPreservesKernels) {
  // 1. Set source to 2 channels (stereo).
  SetSourceChannelCount(2);
  ApplyDeferredTasks();

  // Verify we have 2 kernels.
  Vector<Delay*> kernels_stereo = GetKernels();
  EXPECT_EQ(kernels_stereo.size(), 2u);
  EXPECT_NE(kernels_stereo[0], nullptr);
  EXPECT_NE(kernels_stereo[1], nullptr);

  // 2. Change source to 1 channel (mono).
  SetSourceChannelCount(1);
  ApplyDeferredTasks();

  // Verify we now have 1 kernel.
  Vector<Delay*> kernels_mono = GetKernels();
  EXPECT_EQ(kernels_mono.size(), 1u);
  EXPECT_NE(kernels_mono[0], nullptr);

  // 3. Change source back to 2 channels (stereo).
  SetSourceChannelCount(2);
  ApplyDeferredTasks();

  // Verify we have 2 kernels again.
  Vector<Delay*> kernels_stereo_again = GetKernels();
  EXPECT_EQ(kernels_stereo_again.size(), 2u);
  EXPECT_NE(kernels_stereo_again[0], nullptr);
  EXPECT_NE(kernels_stereo_again[1], nullptr);
}

TEST_F(DelayHandlerTest, ChannelMigrationPreservesAudioContent) {
  // Set delay time to 128 frames (1 render quantum = 128 / 48000 seconds).
  SetDelayTime(128.0f / 48000.0f);

  // 1. Configure stereo source with known sample values:
  // L = 0.5, R = 0.75.
  SetSourceChannelCount(2);
  ApplyDeferredTasks();

  Vector<float> channel0(128, 0.5f);
  Vector<float> channel1(128, 0.75f);
  Vector<Vector<float>> stereo_data;
  stereo_data.push_back(channel0);
  stereo_data.push_back(channel1);
  SetSourceAudioValues(std::move(stereo_data));

  // Process quantum 0. Initial output is silence; samples are buffered in
  // kernels.
  RunProcessOnAudioThread(128);

  // 2. Switch source to mono (stereo -> mono downmixing).
  SetSourceChannelCount(1);
  ApplyDeferredTasks();

  Vector<float> mono_channel(128, 0.9f);
  Vector<Vector<float>> mono_data;
  mono_data.push_back(mono_channel);
  SetSourceAudioValues(std::move(mono_data));

  // Process quantum 1.
  RunProcessOnAudioThread(128);

  // Verify that the output of channel 0 is downmixed:
  // 0.5 * (0.5 + 0.75) = 0.625.
  Vector<float> output_mono = GetOutputChannelData(0, 128);
  for (unsigned i = 0; i < 128; ++i) {
    EXPECT_NEAR(output_mono[i], 0.625f, 1e-5f);
  }

  // 3. Switch source back to stereo (mono -> stereo upmixing).
  SetSourceChannelCount(2);
  ApplyDeferredTasks();

  Vector<float> ch0_new(128, 0.1f);
  Vector<float> ch1_new(128, 0.2f);
  Vector<Vector<float>> stereo_data2;
  stereo_data2.push_back(ch0_new);
  stereo_data2.push_back(ch1_new);
  SetSourceAudioValues(std::move(stereo_data2));

  // Process quantum 2.
  RunProcessOnAudioThread(128);

  // Verify that both channels output the upmixed mono sample (0.9f) from
  // quantum 1.
  Vector<float> output_stereo0 = GetOutputChannelData(0, 128);
  Vector<float> output_stereo1 = GetOutputChannelData(1, 128);
  for (unsigned i = 0; i < 128; ++i) {
    EXPECT_NEAR(output_stereo0[i], 0.9f, 1e-5f);
    EXPECT_NEAR(output_stereo1[i], 0.9f, 1e-5f);
  }
}

}  // namespace blink
