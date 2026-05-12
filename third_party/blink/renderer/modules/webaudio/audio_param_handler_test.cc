// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_param_handler.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/webaudio/audio_param.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/oscillator_node.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "base/synchronization/waitable_event.h"

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

}  // namespace blink
