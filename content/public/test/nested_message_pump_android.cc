// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/nested_message_pump_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "content/public/test/android/test_support_content_jni_headers/NestedSystemMessageHandler_jni.h"

namespace {

base::LazyInstance<base::android::ScopedJavaGlobalRef<jobject>>::
    DestructorAtExit g_message_handler_obj = LAZY_INSTANCE_INITIALIZER;

}  // namespace


namespace content {

struct NestedMessagePumpAndroid::RunState {
  RunState(base::MessagePump::Delegate* delegate, int run_depth)
      : delegate(delegate),
        run_depth(run_depth),
        should_quit(false),
        waitable_event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                       base::WaitableEvent::InitialState::NOT_SIGNALED) {
    waitable_event.declare_only_used_while_idle();
  }

  base::MessagePump::Delegate* delegate;

  // Used to count how many Run() invocations are on the stack.
  int run_depth;

  // Used to flag that the current Run() invocation should return ASAP.
  bool should_quit;

  // Used to sleep until there is more work to do.
  base::WaitableEvent waitable_event;

  // The time at which we should call DoDelayedWork.
  base::TimeTicks delayed_work_time;
};

NestedMessagePumpAndroid::NestedMessagePumpAndroid()
    : state_(NULL) {
}

NestedMessagePumpAndroid::~NestedMessagePumpAndroid() {
}

void NestedMessagePumpAndroid::Run(Delegate* delegate) {
  RunState state(delegate, state_ ? state_->run_depth + 1 : 1);
  RunState* previous_state = state_;
  state_ = &state;

  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  // Need to cap the wait time to allow task processing on the java
  // side. Otherwise, a long wait time on the native will starve java
  // tasks.
  base::TimeDelta max_delay = base::TimeDelta::FromMilliseconds(100);

  for (;;) {
    if (state_->should_quit)
      break;

    bool did_work = state_->delegate->DoWork();
    if (state_->should_quit)
      break;

    did_work |= state_->delegate->DoDelayedWork(&state_->delayed_work_time);
    if (state_->should_quit)
      break;

    if (did_work) {
      continue;
    }

    did_work = state_->delegate->DoIdleWork();
    if (state_->should_quit)
      break;

    if (did_work)
      continue;

    // No native tasks to process right now. Process tasks from the Java
    // System message handler. This will return when the java message queue
    // is idle.
    bool ret = Java_NestedSystemMessageHandler_runNestedLoopTillIdle(
        env, g_message_handler_obj.Get());
    CHECK(ret) << "Error running java message loop, tests will likely fail.";

    if (state_->delayed_work_time.is_null()) {
      state_->waitable_event.TimedWait(max_delay);
    } else {
      base::TimeDelta delay =
          state_->delayed_work_time - base::TimeTicks::Now();
      if (delay > max_delay)
        delay = max_delay;
      if (delay > base::TimeDelta()) {
        state_->waitable_event.TimedWait(delay);
      } else {
        // It looks like delayed_work_time indicates a time in the past, so we
        // need to call DoDelayedWork now.
        state_->delayed_work_time = base::TimeTicks();
      }
    }
  }

  state_ = previous_state;
}

void NestedMessagePumpAndroid::Attach(
    base::MessagePump::Delegate* delegate) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);
  g_message_handler_obj.Get().Reset(
      Java_NestedSystemMessageHandler_create(env));
}

void NestedMessagePumpAndroid::Quit() {
  if (state_) {
    state_->should_quit = true;
    state_->waitable_event.Signal();
    return;
  }
}

void NestedMessagePumpAndroid::ScheduleWork() {
  if (state_) {
    state_->waitable_event.Signal();
    return;
  }
}

void NestedMessagePumpAndroid::ScheduleDelayedWork(
    const base::TimeTicks& delayed_work_time) {
  if (state_) {
    // We know that we can't be blocked on Wait right now since this method can
    // only be called on the same thread as Run, so we only need to update our
    // record of how long to sleep when we do sleep.
    state_->delayed_work_time = delayed_work_time;
    return;
  }
}

}  // namespace content
