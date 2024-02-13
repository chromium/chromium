// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/extension_test_message_listener.h"

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/common/extension_id.h"

ExtensionTestMessageListener::ExtensionTestMessageListener(
    const std::string& expected_message,
    ReplyBehavior reply_behavior)
    : expected_message_(expected_message), reply_behavior_(reply_behavior) {
  test_api_observation_.Observe(
      extensions::TestApiObserverRegistry::GetInstance());
}

ExtensionTestMessageListener::ExtensionTestMessageListener(
    ReplyBehavior reply_behavior)
    : reply_behavior_(reply_behavior) {
  test_api_observation_.Observe(
      extensions::TestApiObserverRegistry::GetInstance());
}

ExtensionTestMessageListener::~ExtensionTestMessageListener() {
  DCHECK(!function_) << "MessageListener did not reply, but signaled it would.";
}

bool ExtensionTestMessageListener::WaitUntilSatisfied() {
  if (satisfied_)
    return !failed_;
  base::RunLoop run_loop;
  quit_wait_closure_ = run_loop.QuitWhenIdleClosure();
  run_loop.Run();
  return !failed_;
}

void ExtensionTestMessageListener::Reply(const std::string& message) {
  CHECK(satisfied_);
  CHECK(function_);

  function_->Reply(message);
  function_.reset();
}

void ExtensionTestMessageListener::Reply(int message) {
  Reply(base::NumberToString(message));
}

void ExtensionTestMessageListener::ReplyWithError(const std::string& error) {
  CHECK(satisfied_);
  CHECK(function_);

  function_->ReplyWithError(error);
  function_.reset();
}

void ExtensionTestMessageListener::Reset() {
  DCHECK(!function_) << "MessageListener did not reply, but signaled it would.";
  satisfied_ = false;
  failed_ = false;
  message_.clear();
  had_user_gesture_ = false;
  extension_id_for_message_.clear();
}

bool ExtensionTestMessageListener::OnTestMessage(
    extensions::TestSendMessageFunction* function,
    const std::string& message) {
  // Return immediately if we're already satisfied or it's not the right
  // extension.
  extensions::ExtensionId sender_extension_id;
  if (function->extension())
    sender_extension_id = function->extension_id();

  if (satisfied_ ||
      (!extension_id_.empty() && sender_extension_id != extension_id_) ||
      (browser_context_ && function->browser_context() != browser_context_)) {
    return false;
  }

  // We should have an empty message if we're not already satisfied.
  CHECK(message_.empty());
  CHECK(extension_id_for_message_.empty());

  bool listener_will_respond = false;

  const bool wait_for_any_message = !expected_message_;
  const bool is_expected_message =
      expected_message_ && message == *expected_message_;
  const bool is_failure_message =
      failure_message_ && message == *failure_message_;

  if (is_expected_message || wait_for_any_message || is_failure_message) {
    message_ = message;
    extension_id_for_message_ = sender_extension_id;
    satisfied_ = true;
    failed_ = is_failure_message;
    had_user_gesture_ = function->user_gesture();

    if (reply_behavior_ == ReplyBehavior::kWillReply) {
      listener_will_respond = true;
      function_ = function;
    }

    if (quit_wait_closure_)
      std::move(quit_wait_closure_).Run();

    if (on_satisfied_)
      std::move(on_satisfied_).Run(message);
    if (on_repeatedly_satisfied_)
      on_repeatedly_satisfied_.Run(message);
  }

  return listener_will_respond;
}
