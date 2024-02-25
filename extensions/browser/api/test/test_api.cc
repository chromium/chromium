// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/test/test_api.h"

#include <string>

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/test/test_api_observer_registry.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_system.h"
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

TestExtensionFunction::~TestExtensionFunction() = default;

bool TestExtensionFunction::PreRunValidation(std::string* error) {
  if (!ExtensionFunction::PreRunValidation(error))
    return false;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    *error = kNotTestProcessError;
    return false;
  }
  return true;
}

TestNotifyPassFunction::~TestNotifyPassFunction() = default;

ExtensionFunction::ResponseAction TestNotifyPassFunction::Run() {
  TestApiObserverRegistry::GetInstance()->NotifyTestPassed(browser_context());
  return RespondNow(NoArguments());
}

TestNotifyFailFunction::~TestNotifyFailFunction() = default;

ExtensionFunction::ResponseAction TestNotifyFailFunction::Run() {
  std::optional<NotifyFail::Params> params = NotifyFail::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  TestApiObserverRegistry::GetInstance()->NotifyTestFailed(
      browser_context(), params->message);
  return RespondNow(NoArguments());
}

TestLogFunction::~TestLogFunction() = default;

ExtensionFunction::ResponseAction TestLogFunction::Run() {
  std::optional<Log::Params> params = Log::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  VLOG(1) << params->message;
  return RespondNow(NoArguments());
}

TestOpenFileUrlFunction::~TestOpenFileUrlFunction() = default;

ExtensionFunction::ResponseAction TestOpenFileUrlFunction::Run() {
  std::optional<api::test::OpenFileUrl::Params> params =
      api::test::OpenFileUrl::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  GURL file_url(params->url);
  EXTENSION_FUNCTION_VALIDATE(file_url.is_valid());
  EXTENSION_FUNCTION_VALIDATE(file_url.SchemeIsFile());

  ExtensionsAPIClient::Get()->OpenFileUrl(file_url, browser_context());
  return RespondNow(NoArguments());
}

TestSendMessageFunction::TestSendMessageFunction() = default;

ExtensionFunction::ResponseAction TestSendMessageFunction::Run() {
  std::optional<PassMessage::Params> params =
      PassMessage::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  bool listener_will_respond =
      TestApiObserverRegistry::GetInstance()->NotifyTestMessage(
          this, params->message);
  // If none of the listeners intend to respond, or one has already responded,
  // finish the function. We always reply to the message, even if it's just an
  // empty string.
  if (!listener_will_respond || response_) {
    if (!response_) {
      response_.emplace(WithArguments(std::string()));
    }
    return RespondNow(std::move(*response_));
  }
  // Otherwise, wait for a reply.
  waiting_ = true;
  return RespondLater();
}

TestSendMessageFunction::~TestSendMessageFunction() = default;

void TestSendMessageFunction::Reply(const std::string& message) {
  DCHECK(!response_);
  response_.emplace(WithArguments(message));
  if (waiting_)
    Respond(std::move(*response_));
}

void TestSendMessageFunction::ReplyWithError(const std::string& error) {
  DCHECK(!response_);
  response_.emplace(Error(error));
  if (waiting_)
    Respond(std::move(*response_));
}

TestSendScriptResultFunction::TestSendScriptResultFunction() = default;
TestSendScriptResultFunction::~TestSendScriptResultFunction() = default;

ExtensionFunction::ResponseAction TestSendScriptResultFunction::Run() {
  std::optional<api::test::SendScriptResult::Params> params =
      api::test::SendScriptResult::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  TestApiObserverRegistry::GetInstance()->NotifyScriptResult(params->result);
  return RespondNow(NoArguments());
}

// static
void TestGetConfigFunction::set_test_config_state(base::Value::Dict* value) {
  TestConfigState* test_config_state = TestConfigState::GetInstance();
  test_config_state->set_config_state(value);
}

TestGetConfigFunction::TestConfigState::TestConfigState()
    : config_state_(nullptr) {}

// static
TestGetConfigFunction::TestConfigState*
TestGetConfigFunction::TestConfigState::GetInstance() {
  return base::Singleton<TestConfigState>::get();
}

TestGetConfigFunction::~TestGetConfigFunction() = default;

ExtensionFunction::ResponseAction TestGetConfigFunction::Run() {
  TestConfigState* test_config_state = TestConfigState::GetInstance();
  if (!test_config_state->config_state())
    return RespondNow(Error(kNoTestConfigDataError));
  return RespondNow(WithArguments(test_config_state->config_state()->Clone()));
}

TestWaitForRoundTripFunction::~TestWaitForRoundTripFunction() = default;

ExtensionFunction::ResponseAction TestWaitForRoundTripFunction::Run() {
  std::optional<WaitForRoundTrip::Params> params =
      WaitForRoundTrip::Params::Create(args());
  return RespondNow(WithArguments(params->message));
}

}  // namespace extensions
