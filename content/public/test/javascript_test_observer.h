// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_JAVASCRIPT_TEST_OBSERVER_H_
#define CONTENT_PUBLIC_TEST_JAVASCRIPT_TEST_OBSERVER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {
class WebContents;

// Base class for handling a stream of automation messages produced by a
// JavascriptTestObserver.
class TestMessageHandler {
 public:
  enum MessageResponse {
    // Reset the timeout and keep running.
    CONTINUE,
    // Stop running.
    DONE
  };

  TestMessageHandler();
  virtual ~TestMessageHandler();

  // Called when a message is received from the DOM automation controller.
  virtual MessageResponse HandleMessage(const std::string& json) = 0;

  void SetError(const std::string& message);

  bool ok() const {
    return ok_;
  }

  const std::string& error_message() const {
    return error_message_;
  }

  // Prepare the handler to be used or reused.
  virtual void Reset();

 private:
  bool ok_;
  std::string error_message_;
};

// This class captures a stream of automation messages coming from a Javascript
// test and dispatches them to a message handler.
class JavascriptTestObserver : public NotificationObserver {
 public:
  // The observer does not own any arguments passed to it.  It is assumed that
  // the arguments will outlive all uses of the observer.
  JavascriptTestObserver(WebContents* web_contents,
                         TestMessageHandler* handler);

  ~JavascriptTestObserver() override;

  // Pump the message loop until the message handler indicates the Javascript
  // test is done running.  Return true if the test jig functioned correctly and
  // nothing timed out.
  bool Run();

  // Prepare the observer to be used again.  This method should NOT be called
  // while Run() is pumping the message loop.
  void Reset();

  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

 private:
  // This message did not signal the end of a test, keep going.
  void Continue();

  // This was the last message we care about, stop listening for more messages.
  void EndTest();

  TestMessageHandler* handler_;
  bool running_;
  bool finished_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptTestObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_JAVASCRIPT_TEST_OBSERVER_H_
