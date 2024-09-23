// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/nested_message_pump_android.h"

#include "base/android/jni_android.h"
#include "base/auto_reset.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/test/android/test_support_content_jni_headers/NestedSystemMessageHandler_jni.h"

namespace content {

NestedMessagePumpAndroid::NestedMessagePumpAndroid() = default;
NestedMessagePumpAndroid::~NestedMessagePumpAndroid() = default;

// We need to override Run() instead of using the implementation from
// base::MessagePumpAndroid because one of the side-effects of
// dispatchOneMessage() is calling Looper::pollOnce(). If that happens while we
// are inside Alooper_pollOnce(), we get a crash because Android Looper isn't
// re-entrant safe. Instead, we keep the entire run loop in Java (in
// MessageQueue.next()).
void NestedMessagePumpAndroid::Run(base::MessagePump::Delegate* delegate) {
  // Preserve delegate and quit state of the current run loop so it can be
  // restored after the loop below has completed.
  auto* old_delegate = SetDelegate(delegate);
  bool old_quit = SetQuit(false);

  ScheduleWork();
  while (!ShouldQuit()) {
    RunJavaSystemMessageHandler();

    // Handle deferred work if necessary.
    // Note: |deferred_work_type_| and |deferred_do_idle_work_| must be reset
    // here because another Run() loop can be triggered when we dispatch work.
    CHECK(!ShouldDeferWork());
    auto work_type = std::exchange(deferred_work_type_, kNone);
    auto do_idle_work = std::exchange(deferred_do_idle_work_, true);
    switch (work_type) {
      case kNone:
        // Do nothing.
        break;
      case kDelayed:
        base::MessagePumpAndroid::DoDelayedLooperWork();
        break;
      case kNonDelayed:
        base::MessagePumpAndroid::DoNonDelayedLooperWork(do_idle_work);
        break;
    }
  }

  // Restore old run loop state.
  SetDelegate(old_delegate);
  SetQuit(old_quit);
}

void NestedMessagePumpAndroid::Quit() {
  QuitJavaSystemMessageHandler();
  SetQuit(true);
}

// Since this pump allows Run() to be called on the UI thread, there's no need
// to also attach the pump on that thread. Making attach a no-op also prevents
// the top level run loop from being considered a nested one.
void NestedMessagePumpAndroid::Attach(Delegate*) {}

void NestedMessagePumpAndroid::DoDelayedLooperWork() {
  if (ShouldDeferWork()) {
    switch (deferred_work_type_) {
      case kNone:
        deferred_work_type_ = kDelayed;
        break;
      case kDelayed:
        // Do nothing. We are already going to process the next delayed work.
        break;
      case kNonDelayed:
        // Do nothing. The immediate work will be processed and then a
        // request to process immediate and idle work will be made. This
        // will unconditionally process the next work item which should be
        // the delayed item.
        break;
    }
    QuitJavaSystemMessageHandler();
    return;
  }

  base::MessagePumpAndroid::DoDelayedLooperWork();
}

void NestedMessagePumpAndroid::DoNonDelayedLooperWork(bool do_idle_work) {
  if (ShouldDeferWork()) {
    deferred_work_type_ = kNonDelayed;

    // Only do idle work if we are consistently asked to do it. If there
    // is a request to defer the idle work, then we will honor that over
    // any previous request to do idle work. Once the non-delayed work is
    // dispatched, another request to do the idle work will be made.
    deferred_do_idle_work_ &= do_idle_work;

    QuitJavaSystemMessageHandler();
    return;
  }

  base::MessagePumpAndroid::DoNonDelayedLooperWork(do_idle_work);
}

void NestedMessagePumpAndroid::RunJavaSystemMessageHandler() {
  auto* env = base::android::AttachCurrentThread();
  CHECK(!quit_message_handler_);
  CHECK(!inside_run_message_handler_);
  base::AutoReset<bool> auto_reset(&inside_run_message_handler_, true);
  // Dispatch the first available Java message and one or more C++ messages as
  // a side effect. If there are no Java messages available, this call will
  // block until one becomes available (while continuing to process C++ work).
  bool ret = Java_NestedSystemMessageHandler_dispatchOneMessage(env);
  CHECK(ret) << "Error running java message loop, tests will likely fail.";
  quit_message_handler_ = false;
}

void NestedMessagePumpAndroid::QuitJavaSystemMessageHandler() {
  if (!inside_run_message_handler_ || quit_message_handler_)
    return;

  quit_message_handler_ = true;

  // Wake up the Java message dispatcher to exit dispatchOneMessage().
  auto* env = base::android::AttachCurrentThread();
  Java_NestedSystemMessageHandler_postQuitMessage(env);
}

}  // namespace content
