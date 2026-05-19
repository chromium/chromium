// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_param_handler.h"

#include <memory>

#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/oscillator_node.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

TEST(AudioParamHandlerTest, UAFOnGCDerivedFromSummingBus) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();

  DummyExceptionStateForTesting exception_state;
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 2, 1, 48000, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  OscillatorNode* osc1 = context->createOscillator(exception_state);
  ASSERT_FALSE(exception_state.HadException());
  OscillatorNode* osc2 = context->createOscillator(exception_state);
  ASSERT_FALSE(exception_state.HadException());

  // Connect osc1 to osc2's frequency AudioParam.
  osc1->connect(osc2->frequency(), 0, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  // Keep the AudioParam alive so it outlives the OscillatorNode during GC.
  Persistent<AudioParam> frequency_param = osc2->frequency();

  // Create a background thread to simulate the audio thread.
  std::unique_ptr<NonMainThread> audio_thread = NonMainThread::CreateThread(
      ThreadCreationParams(ThreadType::kRealtimeAudioWorkletThread));

  base::WaitableEvent event;

  PostCrossThreadTask(
      *audio_thread->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(
          [](OfflineAudioContext* context, AudioParam* param,
             base::WaitableEvent* event) {
            context->GetDeferredTaskHandler()
                .SetAudioThreadToCurrentThread();
            {
              DeferredTaskHandler::GraphAutoLocker locker(
                  context->GetDeferredTaskHandler());
              context->GetDeferredTaskHandler().HandleDeferredTasks();
            }
            param->Handler().FinalValue();
            event->Signal();
          },
          WrapCrossThreadPersistent(context),
          WrapCrossThreadPersistent(frequency_param.Get()),
          CrossThreadUnretained(&event)));

  event.Wait();

  // Drop references to nodes.
  osc1 = nullptr;
  osc2 = nullptr;

  // Force GC. Without the fix, frequency_param's summing_bus_ might hold a
  // dangling pointer.
  ThreadState::Current()->CollectAllGarbageForTesting();

  // Clear the AudioParam and GC again. This destroys the AudioParamHandler.
  frequency_param = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
}

TEST(AudioParamHandlerTest, TimelinePruningOnDisconnectedNode) {
  test::TaskEnvironment task_environment;
  auto page = std::make_unique<DummyPageHolder>();

  DummyExceptionStateForTesting exception_state;
  OfflineAudioContext* context = OfflineAudioContext::Create(
      page->GetFrame().DomWindow(), 1, 128, 48000, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  OscillatorNode* osc = context->createOscillator(exception_state);
  ASSERT_FALSE(exception_state.HadException());

  AudioParamHandler& param_handler = osc->frequency()->Handler();

  // We need to acquire the lock to inspect and mutate the timeline events
  // directly.
  {
    base::AutoLock locker(param_handler.events_lock_);
    EXPECT_EQ(param_handler.events_.size(), 0u);
    EXPECT_EQ(param_handler.new_events_.size(), 0u);

    // Insert E0 (past)
    param_handler.InsertEvent(
        AudioParamHandler::ParamEvent::CreateSetValueEvent(1.0, -2.0),
        exception_state);
    ASSERT_FALSE(exception_state.HadException());
    EXPECT_EQ(param_handler.events_.size(), 1u);
    EXPECT_EQ(param_handler.new_events_.size(), 1u);

    // Insert E1 (past)
    param_handler.InsertEvent(
        AudioParamHandler::ParamEvent::CreateSetValueEvent(2.0, -1.0),
        exception_state);
    ASSERT_FALSE(exception_state.HadException());
    EXPECT_EQ(param_handler.events_.size(), 2u);
    EXPECT_EQ(param_handler.new_events_.size(), 2u);

    // Insert E2 (future) -> triggers pruning of E0, keeps E1 (preceding) and
    // E2.
    param_handler.InsertEvent(
        AudioParamHandler::ParamEvent::CreateSetValueEvent(3.0, 1.0),
        exception_state);
    ASSERT_FALSE(exception_state.HadException());

    // E0 should be pruned.
    EXPECT_EQ(param_handler.events_.size(), 2u);
    EXPECT_EQ(param_handler.new_events_.size(), 2u);
  }
}

}  // namespace blink
