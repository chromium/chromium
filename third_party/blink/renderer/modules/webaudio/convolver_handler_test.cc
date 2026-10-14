// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/convolver_handler.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/convolver_node.h"
#include "third_party/blink/renderer/modules/webaudio/gain_node.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/testing/fake_audio_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class ConvolverHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    page_ = std::make_unique<DummyPageHolder>();
    context_ = OfflineAudioContext::Create(
        page_->GetFrame().DomWindow(), 2, 1, 48000, ASSERT_NO_EXCEPTION);

    GainNode* source = context_->createGain(ASSERT_NO_EXCEPTION);
    source->setChannelCount(2, ASSERT_NO_EXCEPTION);
    convolver_ = context_->createConvolver(ASSERT_NO_EXCEPTION);
    source->connect(convolver_, 0, 0, ASSERT_NO_EXCEPTION);
  }

  ConvolverHandler* GetHandler() const {
    return &static_cast<ConvolverHandler&>(convolver_->Handler());
  }

  void CheckNumberOfChannelsForInputInternalOnAudioThread() {
    audio_thread_.RunOnAudioThreadWithContext(
        context_.Get(),
        CrossThreadBindOnce(
            [](OfflineAudioContext* context, ConvolverHandler* handler) {
              DeferredTaskHandler::GraphAutoLocker locker(
                  context->GetDeferredTaskHandler());
              handler->CheckNumberOfChannelsForInput(&handler->Input(0));
            },
            WrapCrossThreadPersistent(context_.Get()),
            CrossThreadUnretained(GetHandler())));
  }

  void SetOutputNumberOfChannelsOnAudioThread(unsigned number_of_channels) {
    audio_thread_.RunOnAudioThreadWithContext(
        context_.Get(),
        CrossThreadBindOnce(
            [](OfflineAudioContext* context, ConvolverHandler* handler,
               unsigned channels) {
              DeferredTaskHandler::GraphAutoLocker locker(
                  context->GetDeferredTaskHandler());
              handler->Output(0).SetNumberOfChannels(channels);
            },
            WrapCrossThreadPersistent(context_.Get()),
            CrossThreadUnretained(GetHandler()), number_of_channels));
  }

  void HandleDeferredTasksOnAudioThread() {
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

  void RunProcessOnAudioThread() {
    audio_thread_.RunOnAudioThreadWithContext(
        context_.Get(),
        CrossThreadBindOnce(
            [](ConvolverHandler* handler) { handler->Process(128); },
            CrossThreadUnretained(GetHandler())));
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> page_;
  Persistent<OfflineAudioContext> context_;
  FakeAudioThread audio_thread_;
  Persistent<ConvolverNode> convolver_;
};

TEST_F(ConvolverHandlerTest, CheckNumberOfChannelsForInputWithLockContention) {
  // 1. Initial setup.
  AudioBuffer* stereo_buffer = AudioBuffer::Create(2, 1, 48000);
  convolver_->setBuffer(stereo_buffer, ASSERT_NO_EXCEPTION);

  CheckNumberOfChannelsForInputInternalOnAudioThread();

  EXPECT_EQ(GetHandler()->Output(0).NumberOfChannels(), 2u);

  // 2. Simulate the bug state: Force Output(0) back to 1 channel manually
  // on audio thread.
  SetOutputNumberOfChannelsOnAudioThread(1);

  EXPECT_EQ(GetHandler()->Output(0).NumberOfChannels(), 1u);

  // 3. Start contention on the main thread.
  {
    base::AutoLock contention_lock(GetHandler()->process_lock_);

    // 4. Audio thread tries to update. It will take graph lock but safely
    // bail out on process_lock_ via AutoTryLock.
    audio_thread_.RunOnAudioThreadWithContext(
        context_.Get(),
        CrossThreadBindOnce(
            [](OfflineAudioContext* context, ConvolverHandler* handler) {
              DeferredTaskHandler::GraphAutoLocker locker(
                  context->GetDeferredTaskHandler());
              handler->CheckNumberOfChannelsForInput(&handler->Input(0));
            },
            WrapCrossThreadPersistent(context_.Get()),
            CrossThreadUnretained(GetHandler())));
  }

  // 5. Verify the state. Output(0) should STILL be 1 because of early return.
  EXPECT_EQ(GetHandler()->Output(0).NumberOfChannels(), 1u);

  // 6. Call it again on the audio thread when the lock is free.
  CheckNumberOfChannelsForInputInternalOnAudioThread();
  EXPECT_EQ(GetHandler()->Output(0).NumberOfChannels(), 2u);

  // 7. Run Process() on the audio thread.
  RunProcessOnAudioThread();
}

TEST_F(ConvolverHandlerTest,
       ChannelCountUpdatesAcrossBufferAndModeTransitions) {
  AudioBuffer* mono_buffer = AudioBuffer::Create(1, 128, 48000);
  AudioBuffer* stereo_buffer = AudioBuffer::Create(2, 128, 48000);

  convolver_->setChannelCount(1, ASSERT_NO_EXCEPTION);
  convolver_->setChannelCountMode(
      V8ChannelCountMode(V8ChannelCountMode::Enum::kExplicit),
      ASSERT_NO_EXCEPTION);
  HandleDeferredTasksOnAudioThread();

  // 1. Set a mono buffer while the main thread sees a 2-channel explicit input
  // (queuing desired_number_of_channels_ = 2 on Output(0) while
  // Output(0).NumberOfChannels() is still 1), then switch back to 1-channel
  // input before HandleDeferredTasks() runs on the audio thread.
  convolver_->setChannelCount(2, ASSERT_NO_EXCEPTION);
  convolver_->setBuffer(mono_buffer, ASSERT_NO_EXCEPTION);
  convolver_->setChannelCount(1, ASSERT_NO_EXCEPTION);

  HandleDeferredTasksOnAudioThread();

  // Input is 1 channel and buffer is 1 channel, so output must be 1 channel
  // (stale desired_number_of_channels_ = 2 from setBuffer must not win).
  EXPECT_EQ(GetHandler()->Input(0).NumberOfChannels(), 1u);
  EXPECT_EQ(GetHandler()->Output(0).NumberOfChannels(), 1u);
  RunProcessOnAudioThread();

  // 2. Switch to a stereo buffer with 1-channel input -> output becomes 2ch.
  convolver_->setBuffer(stereo_buffer, ASSERT_NO_EXCEPTION);
  HandleDeferredTasksOnAudioThread();
  EXPECT_EQ(GetHandler()->Output(0).NumberOfChannels(), 2u);
  RunProcessOnAudioThread();

  // 3. Clear buffer (setBuffer(nullptr)) with 1-channel input -> output resets
  // to 1ch.
  convolver_->setBuffer(nullptr, ASSERT_NO_EXCEPTION);
  HandleDeferredTasksOnAudioThread();
  EXPECT_EQ(GetHandler()->Output(0).NumberOfChannels(), 1u);
  RunProcessOnAudioThread();
}

}  // namespace blink
