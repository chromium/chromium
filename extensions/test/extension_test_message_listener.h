// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_EXTENSION_TEST_MESSAGE_LISTENER_H_
#define EXTENSIONS_TEST_EXTENSION_TEST_MESSAGE_LISTENER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "extensions/browser/api/test/test_api_observer.h"
#include "extensions/browser/api/test/test_api_observer_registry.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class TestSendMessageFunction;
}

// This class helps us wait for incoming messages sent from javascript via
// chrome.test.sendMessage(). A sample usage would be:
//
//   ExtensionTestMessageListener listener("foo");
//   ... do some work
//   ASSERT_TRUE(listener.WaitUntilSatisfied());
//
// It is also possible to have the extension wait for our reply. This is
// useful for coordinating multiple pages/processes and having them wait on
// each other. Example:
//
//   ExtensionTestMessageListener listener1("foo1", ReplyBehavior::kWillReply);
//   ExtensionTestMessageListener listener2("foo2", ReplyBehavior::kWillReply);
//   ASSERT_TRUE(listener1.WaitUntilSatisfied());
//   ASSERT_TRUE(listener2.WaitUntilSatisfied());
//   ... do some work
//   listener1.Reply("foo2 is ready");
//   listener2.Reply("foo1 is ready");
//
// Further, we can use this to listen for a success and failure message:
//
//   ExtensionTestMessageListener listener("success", will_reply);
//   listener.set_failure_message("failure");
//   ASSERT_TRUE(listener.WaitUntilSatisfied());
//   if (listener.message() == "success") {
//     HandleSuccess();
//   } else {
//     ASSERT_EQ("failure", listener.message());
//     HandleFailure();
//   }
//
// Or, use it to listen to any arbitrary message:
//
//   ExtensionTestMessageListener listener(will_reply);
//   ASSERT_TRUE(listener.WaitUntilSatisfied());
//   if (listener.message() == "foo")
//     HandleFoo();
//   else if (listener.message() == "bar")
//     HandleBar();
//   else if (listener.message() == "baz")
//     HandleBaz();
//   else
//     NOTREACHED_IN_MIGRATION();
//
// You can also use the class to listen for messages from a specified extension:
//
//   ExtensionTestMessageListener listener(will_reply);
//   listener.set_extension_id(extension->id());
//   ASSERT_TRUE(listener.WaitUntilSatisfied());
//   ... do some work.
//
// A callback can be set to react to a message from an extension, instead of
// manually waiting.
//
//   ExtensionTestMessageListener listener("do_something");
//   listener.SetOnSatisfied(base::BindOnce(&DoSomething));
//   ... run test
//   // SetOnRepeatedlySatisfied could be used if the message is expected
//   // multiple times.
//
// Finally, you can reset the listener to reuse it.
//
//   ExtensionTestMessageListener listener(ReplyBehavior::kWillReply);
//   ASSERT_TRUE(listener.WaitUntilSatisfied());
//   while (listener.message() != "end") {
//     Handle(listener.message());
//     listener.Reply("bar");
//     listener.Reset();
//     ASSERT_TRUE(listener.WaitUntilSatisfied());
//   }
//
// Note that when using it in browser tests, you need to make sure it gets
// destructed *before* the browser gets torn down. Two common patterns are to
// either make it a local variable inside your test body, or if it's a member
// variable of a ExtensionBrowserTest subclass, override the
// BrowserTestBase::TearDownOnMainThread() method and clean it up there.

// The behavior specifying whether the listener will reply to the
// incoming message. This is defined outside the class simply to save authors
// from typing out ReplyBehavior::kWillReply.
enum class ReplyBehavior {
  // The listener will reply. The extension API callback will not be
  // triggered until `ExtensionTestMessageListener::Reply()` is called.
  kWillReply,
  // The listener won't reply with a custom message. The extension API
  // callback is triggered automatically.
  kWontReply,
};

class ExtensionTestMessageListener : public extensions::TestApiObserver {
 public:
  // Listen for the `expected_message` with the specified `reply_behavior`.
  // TODO(devlin): Possibly update this to just take a string_view, once the
  // enum conversions highlighted below are done?
  explicit ExtensionTestMessageListener(
      const std::string& expected_message,
      ReplyBehavior reply_behavior = ReplyBehavior::kWontReply);
  // Construct a message listener which will listen for any message with
  // the specified `reply_behavior`.
  explicit ExtensionTestMessageListener(
      ReplyBehavior reply_behavior = ReplyBehavior::kWontReply);

  ~ExtensionTestMessageListener() override;

  // This returns true immediately if we've already gotten the expected
  // message, or waits until it arrives. Once this returns true, message() and
  // extension_id_for_message() accessors can be used.
  // Returns false if the wait is interrupted and we still haven't gotten the
  // message, or if the message was equal to |failure_message_|.
  [[nodiscard]] bool WaitUntilSatisfied();

  // Send the given message as a reply. It is only valid to call this after
  // WaitUntilSatisfied has returned true, and if will_reply is true.
  void Reply(const std::string& message);

  // Convenience method that formats int as a string and sends it.
  void Reply(int message);

  void ReplyWithError(const std::string& error);

  // Reset the listener to listen again. No settings (such as messages to
  // listen for) are modified.
  void Reset();

  // Getters and setters.

  bool was_satisfied() const { return satisfied_; }

  void set_failure_message(const std::string& failure_message) {
    failure_message_ = failure_message;
  }

  using OnSatisfiedSignature = void(const std::string&);
  void SetOnSatisfied(base::OnceCallback<OnSatisfiedSignature> on_satisfied) {
    on_satisfied_ = std::move(on_satisfied);
  }
  void SetOnRepeatedlySatisfied(
      base::RepeatingCallback<OnSatisfiedSignature> on_repeatedly_satisfied) {
    on_repeatedly_satisfied_ = on_repeatedly_satisfied;
  }

  void set_extension_id(const extensions::ExtensionId& extension_id) {
    extension_id_ = extension_id;
  }

  void set_browser_context(const content::BrowserContext* browser_context) {
    browser_context_ = browser_context;
  }

  const std::string& message() const { return message_; }

  const extensions::ExtensionId& extension_id_for_message() const {
    return extension_id_for_message_;
  }

  bool had_user_gesture() const { return had_user_gesture_; }

 private:
  // extensions::TestApiObserver:
  bool OnTestMessage(extensions::TestSendMessageFunction* function,
                     const std::string& message) override;

  // The message we're expecting. If empty, we will wait for any message,
  // regardless of contents.
  const std::optional<std::string> expected_message_;

  // The last message we received.
  std::string message_;

  // Whether we've seen expected_message_ yet.
  bool satisfied_ = false;

  // Holds the quit Closure for the RunLoop during WaitUntilSatisfied().
  base::OnceClosure quit_wait_closure_;

  // Notifies when the expected message is received.
  base::OnceCallback<OnSatisfiedSignature> on_satisfied_;
  base::RepeatingCallback<OnSatisfiedSignature> on_repeatedly_satisfied_;

  // Whether the listener will send an explicit reply via `Reply()`.
  const ReplyBehavior reply_behavior_;

  // The extension id that we listen for, or empty.
  extensions::ExtensionId extension_id_;

  // If non-null, we listen to messages only from this BrowserContext.
  raw_ptr<const content::BrowserContext> browser_context_ = nullptr;

  // The message that signals failure.
  std::optional<std::string> failure_message_;

  // If we received a message that was the failure message.
  bool failed_ = false;

  // The extension id from which |message_| was received.
  extensions::ExtensionId extension_id_for_message_;

  // Whether the ExtensionFunction handling the message had an active user
  // gesture.
  bool had_user_gesture_ = false;

  // The function we need to reply to.
  scoped_refptr<extensions::TestSendMessageFunction> function_;

  base::ScopedObservation<extensions::TestApiObserverRegistry,
                          extensions::TestApiObserver>
      test_api_observation_{this};
};

#endif  // EXTENSIONS_TEST_EXTENSION_TEST_MESSAGE_LISTENER_H_
