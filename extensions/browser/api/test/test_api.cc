// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/test/test_api.h"

#include <string>

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/api/test.h"

namespace {

// If you see this error in your test, you need to set the config state
// to be returned by chrome.test.getConfig().  Do this by calling
// TestGetConfigFunction::set_test_config_state(Value* state)
// in test set up.
const char kNoTestConfigDataError[] = "Test configuration was not set.";

const char kNotTestProcessError[] =
    "The chrome.test namespace is only available in tests.";

}  // namespace

namespace extensions {

namespace Log = api::test::Log;
namespace NotifyFail = api::test::NotifyFail;
namespace PassMessage = api::test::PassMessage;
namespace WaitForRoundTrip = api::test::WaitForRoundTrip;

TestExtensionFunction::~TestExtensionFunction() {}

bool TestExtensionFunction::PreRunValidation(std::string* error) {
  if (!ExtensionFunction::PreRunValidation(error))
    return false;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    *error = kNotTestProcessError;
    return false;
  }
  return true;
}

TestNotifyPassFunction::~TestNotifyPassFunction() {}

ExtensionFunction::ResponseAction TestNotifyPassFunction::Run() {
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_TEST_PASSED,
      content::Source<content::BrowserContext>(dispatcher()->browser_context()),
      content::NotificationService::NoDetails());
  return RespondNow(NoArguments());
}

TestNotifyFailFunction::~TestNotifyFailFunction() {}

ExtensionFunction::ResponseAction TestNotifyFailFunction::Run() {
  std::unique_ptr<NotifyFail::Params> params(
      NotifyFail::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_TEST_FAILED,
      content::Source<content::BrowserContext>(dispatcher()->browser_context()),
      content::Details<std::string>(&params->message));
  return RespondNow(NoArguments());
}

TestLogFunction::~TestLogFunction() {}

ExtensionFunction::ResponseAction TestLogFunction::Run() {
  std::unique_ptr<Log::Params> params(Log::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  VLOG(1) << params->message;
  return RespondNow(NoArguments());
}

TestSendMessageFunction::TestSendMessageFunction() : waiting_(false) {}

ExtensionFunction::ResponseAction TestSendMessageFunction::Run() {
  std::unique_ptr<PassMessage::Params> params(
      PassMessage::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  bool listener_will_respond = false;
  std::pair<std::string, bool*> details(params->message,
                                        &listener_will_respond);
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE,
      content::Source<TestSendMessageFunction>(this),
      content::Details<std::pair<std::string, bool*>>(&details));
  // If the listener is not intending to respond, or has already responded,
  // finish the function.
  if (!listener_will_respond || response_.get()) {
    if (!response_) {
      response_ = OneArgument(std::make_unique<base::Value>(std::string()));
    }
    return RespondNow(std::move(response_));
  }
  // Otherwise, wait for a reply.
  waiting_ = true;
  return RespondLater();
}

TestSendMessageFunction::~TestSendMessageFunction() {}

void TestSendMessageFunction::Reply(const std::string& message) {
  DCHECK(!response_);
  response_ = OneArgument(std::make_unique<base::Value>(message));
  if (waiting_)
    Respond(std::move(response_));
}

void TestSendMessageFunction::ReplyWithError(const std::string& error) {
  DCHECK(!response_);
  response_ = Error(error);
  if (waiting_)
    Respond(std::move(response_));
}

// static
void TestGetConfigFunction::set_test_config_state(
    base::DictionaryValue* value) {
  TestConfigState* test_config_state = TestConfigState::GetInstance();
  test_config_state->set_config_state(value);
}

TestGetConfigFunction::TestConfigState::TestConfigState()
    : config_state_(NULL) {}

// static
TestGetConfigFunction::TestConfigState*
TestGetConfigFunction::TestConfigState::GetInstance() {
  return base::Singleton<TestConfigState>::get();
}

TestGetConfigFunction::~TestGetConfigFunction() {}

ExtensionFunction::ResponseAction TestGetConfigFunction::Run() {
  TestConfigState* test_config_state = TestConfigState::GetInstance();
  if (!test_config_state->config_state())
    return RespondNow(Error(kNoTestConfigDataError));
  return RespondNow(
      OneArgument(test_config_state->config_state()->CreateDeepCopy()));
}

TestWaitForRoundTripFunction::~TestWaitForRoundTripFunction() {}

ExtensionFunction::ResponseAction TestWaitForRoundTripFunction::Run() {
  std::unique_ptr<WaitForRoundTrip::Params> params(
      WaitForRoundTrip::Params::Create(*args_));
  return RespondNow(
      OneArgument(std::make_unique<base::Value>(params->message)));
}

}  // namespace extensions
