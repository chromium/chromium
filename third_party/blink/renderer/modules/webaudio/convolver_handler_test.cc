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

    // Create a background thread to simulate the audio thread.
    audio_thread_ = NonMainThread::CreateThread(
        ThreadCreationParams(ThreadType::kTestThread));

    GainNode* source = context_->createGain(ASSERT_NO_EXCEPTION);
    source->setChannelCount(2, ASSERT_NO_EXCEPTION);
    convolver_ = context_->createConvolver(ASSERT_NO_EXCEPTION);
    source->connect(convolver_, 0, 0, ASSERT_NO_EXCEPTION);
  }

  void TearDown() override { audio_thread_.reset(); }

  ConvolverHandler* GetHandler() const {
    return &static_cast<ConvolverHandler&>(convolver_->Handler());
  }

  void RunOnAudioThread(CrossThreadOnceClosure closure) {
    base::WaitableEvent event;
    PostCrossThreadTask(
        *audio_thread_->GetTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(
            [](CrossThreadOnceClosure closure, base::WaitableEvent* event) {
              std::move(closure).Run();
              event->Signal();
            },
            std::move(closure), CrossThreadUnretained(&event)));
    event.Wait();
  }

  void CheckNumberOfChannelsForInputInternalOnAudioThread() {
    RunOnAudioThread(CrossThreadBindOnce(
        [](OfflineAudioContext* context, ConvolverHandler* handler) {
          // In a test environment, we must manually register this background
          // thread as the audio thread so that `IsAudioThread()` checks and
          // `DCHECK`s pass correctly.
          context->GetDeferredTaskHandler().SetAudioThreadToCurrentThread();
          DeferredTaskHandler::GraphAutoLocker locker(
              context->GetDeferredTaskHandler());
          handler->CheckNumberOfChannelsForInput(&handler->Input(0));
        },
        WrapCrossThreadPersistent(context_.Get()),
        CrossThreadUnretained(GetHandler())));
  }

  void SetOutputNumberOfChannelsOnAudioThread(unsigned number_of_channels) {
    RunOnAudioThread(CrossThreadBindOnce(
        [](OfflineAudioContext* context, ConvolverHandler* handler,
           unsigned channels) {
          DeferredTaskHandler::GraphAutoLocker locker(
              context->GetDeferredTaskHandler());
          handler->Output(0).SetNumberOfChannels(channels);
        },
        WrapCrossThreadPersistent(context_.Get()),
        CrossThreadUnretained(GetHandler()), number_of_channels));
  }

  void RunProcessOnAudioThread() {
    RunOnAudioThread(CrossThreadBindOnce(
        [](ConvolverHandler* handler) { handler->Process(128); },
        CrossThreadUnretained(GetHandler())));
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> page_;
  Persistent<OfflineAudioContext> context_;
  std::unique_ptr<NonMainThread> audio_thread_;
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
    base::WaitableEvent task_finished_event;

    {
      base::AutoLock contention_lock(GetHandler()->process_lock_);

      // 4. Audio thread tries to update. It will take graph lock but safely
      // bail out on process_lock_ via AutoTryLock.
      PostCrossThreadTask(
          *audio_thread_->GetTaskRunner(), FROM_HERE,
          CrossThreadBindOnce(
              [](OfflineAudioContext* context, ConvolverHandler* handler,
                 base::WaitableEvent* finished) {
                context->GetDeferredTaskHandler()
                    .SetAudioThreadToCurrentThread();
                DeferredTaskHandler::GraphAutoLocker locker(
                    context->GetDeferredTaskHandler());
                handler->CheckNumberOfChannelsForInput(&handler->Input(0));
                finished->Signal();
              },
              WrapCrossThreadPersistent(context_.Get()),
              CrossThreadUnretained(GetHandler()),
              CrossThreadUnretained(&task_finished_event)));

      // Because AutoTryLock is non-blocking, we can just wait for the task
      // to finish while holding contention_lock to securely guarantee the
      // race condition occurs.
      task_finished_event.Wait();
    }
  }

  // 5. Verify the state. Output(0) should STILL be 1 because of early return.
  EXPECT_EQ(GetHandler()->Output(0).NumberOfChannels(), 1u);

  // 6. Call it again on the audio thread when the lock is free.
  CheckNumberOfChannelsForInputInternalOnAudioThread();
  EXPECT_EQ(GetHandler()->Output(0).NumberOfChannels(), 2u);

  // 7. Run Process() on the audio thread.
  RunProcessOnAudioThread();
}

}  // namespace blink
