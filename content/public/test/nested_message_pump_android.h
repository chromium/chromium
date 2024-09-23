// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_NESTED_MESSAGE_PUMP_ANDROID_H_
#define CONTENT_PUBLIC_TEST_NESTED_MESSAGE_PUMP_ANDROID_H_

#include "base/message_loop/message_pump_android.h"

namespace content {

// A nested message pump to be used for content browsertests and web tests
// on Android. It overrides the default UI message pump to allow nested loops.
class NestedMessagePumpAndroid : public base::MessagePumpAndroid {
 public:
  NestedMessagePumpAndroid();

  NestedMessagePumpAndroid(const NestedMessagePumpAndroid&) = delete;
  NestedMessagePumpAndroid& operator=(const NestedMessagePumpAndroid&) = delete;

  ~NestedMessagePumpAndroid() override;

  void Run(Delegate*) override;
  void Quit() override;
  void Attach(Delegate*) override;
  void DoDelayedLooperWork() override;
  void DoNonDelayedLooperWork(bool do_idle_work) override;

 private:
  // Returns true if the work should be deferred.
  bool ShouldDeferWork() const { return inside_run_message_handler_; }

  // Calls Java_NestedSystemMessageHandler_dispatchOneMessage() to service the
  // native looper, dispatch a pending Java message, or wait until a Java
  // message or looper event arrives.
  void RunJavaSystemMessageHandler();

  // Sends a signal that causes the current RunMessageHandler() call to return
  // as soon as possible.
  void QuitJavaSystemMessageHandler();

  // Tracks whether a RunMessageHandler() call is currently on the stack.
  bool inside_run_message_handler_ = false;

  // Tracks whether a signal has been sent to trigger the current
  // RunMessageHandler() call to return.
  bool quit_message_handler_ = false;

  // Keeps track of the work that needs to be dispatched after
  // RunJavaSystemMessageHandler() returns.
  enum DeferredWorkType {
    kNone,
    kDelayed,
    kNonDelayed,
  };
  DeferredWorkType deferred_work_type_ = kNone;
  bool deferred_do_idle_work_ = true;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_NESTED_MESSAGE_PUMP_ANDROID_H_
