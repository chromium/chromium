// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/wpt/cwt_request_handler.h"

#include "base/debug/stack_trace.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/sys_string_conversions.h"
#include "components/version_info/version_info.h"
#import "ios/chrome/test/wpt/cwt_webdriver_app_interface.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/http/http_status_code.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

GREY_STUB_CLASS_IN_APP_BACKGROUND_QUEUE(CWTWebDriverAppInterface)

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace {

const NSTimeInterval kDefaultScriptTimeout = 30;
const NSTimeInterval kDefaultPageLoadTimeout = 300;

// WebDriver commands.
const char kWebDriverSessionCommand[] = "session";
const char kWebDriverNavigationCommand[] = "url";
const char kWebDriverTimeoutsCommand[] = "timeouts";
const char kWebDriverWindowCommand[] = "window";
const char kWebDriverWindowHandlesCommand[] = "handles";
const char kWebDriverSyncScriptCommand[] = "sync";
const char kWebDriverAsyncScriptCommand[] = "async";
const char kWebDriverScreenshotCommand[] = "screenshot";
const char kWebDriverWindowRectCommand[] = "rect";

// WebDriver error codes.
const char kWebDriverInvalidArgumentError[] = "invalid argument";
const char kWebDriverInvalidSessionError[] = "invalid session id";
const char kWebDriverSessionCreationError[] = "session not created";
const char kWebDriverTimeoutError[] = "timeout";
const char kWebDriverScriptTimeoutError[] = "script timeout";
const char kWebDriverNoSuchWindowError[] = "no such window";
const char kWebDriverUnknownCommandError[] = "unknown command";

// WebDriver error messages. The content of each message is implementation-
// defined, not prescribed the by the spec.
const char kWebDriverMissingRequestMessage[] = "Missing request body";
const char kWebDriverMissingURLMessage[] = "No url argument";
const char kWebDriverNoActiveSessionMessage[] = "No currently active session";
const char kWebDriverPageLoadTimeoutMessage[] = "Page load timed out";
const char kWebDriverSessionAlreadyExistsMessage[] = "A session already exists";
const char kWebDriverUnknownCommandMessage[] = "No such command";
const char kWebDriverInvalidTimeoutMessage[] =
    "Timeouts must be non-negative integers";
const char kWebDriverNoTargetWindowMessage[] = "Target window has been closed";
const char kWebDriverMissingWindowHandleMessage[] = "No handle argument";
const char kWebDriverNoMatchingWindowMessage[] =
    "No window with the given handle";
const char kWebDriverMissingScriptMessage[] = "No script argument";
const char kWebDriverScriptTimeoutMessage[] = "Script execution timed out";

// WebDriver request field names. These are fields that are contained within
// the body of a POST request.
const char kWebDriverURLRequestField[] = "url";
const char kWebDriverScriptTimeoutRequestField[] = "script";
const char kWebDriverPageLoadTimeoutRequestField[] = "pageLoad";
const char kWebDriverWindowHandleRequestField[] = "handle";
const char kWebDriverScriptRequestField[] = "script";

// WebDriver response field name. This is the top-level field in the JSON object
// contained in a response.
const char kWebDriverValueResponseField[] = "value";

// WebDriver value field names. These fields are contained within the 'value'
// field in a WebDriver reponse. Each response value has zero or more of these
// fields.
const char kWebDriverCapabilitiesValueField[] = "capabilities";
const char kWebDriverErrorCodeValueField[] = "error";
const char kWebDriverErrorMessageValueField[] = "message";
const char kWebDriverSessionIdValueField[] = "sessionId";
const char kWebDriverStackTraceValueField[] = "stacktrace";

// Field names for the "capabilities" struct that's included in the response
// when creating a session.
const char kCapabilitiesBrowserNameField[] = "browserName";
const char kCapabilitiesBrowserVersionField[] = "browserVersion";
const char kCapabilitiesPlatformNameField[] = "platformName";
const char kCapabilitiesPageLoadStrategyField[] = "pageLoadStrategy";
const char kCapabilitiesProxyField[] = "proxy";
const char kCapabilitiesScriptTimeoutField[] = "timeouts.script";
const char kCapabilitiesPageLoadTimeoutField[] = "timeouts.pageLoad";
const char kCapabilitiesImplicitTimeoutField[] = "timeouts.implicit";
const char kCapabilitiesCanResizeWindowsField[] = "setWindowRect";

// Field values for the "capabilities" struct that's included in the response
// when creating a session.
const char kCapabilitiesBrowserName[] = "chrome_ios";
const char kCapabilitiesPlatformName[] = "iOS";
const char kCapabilitiesPageLoadStrategy[] = "normal";

base::Value CreateErrorValue(const std::string& error,
                             const std::string& message) {
  base::Value error_value(base::Value::Type::DICTIONARY);
  error_value.SetStringKey(kWebDriverErrorCodeValueField, error);
  error_value.SetStringKey(kWebDriverErrorMessageValueField, message);
  error_value.SetStringKey(kWebDriverStackTraceValueField,
                           base::debug::StackTrace().ToString());
  return error_value;
}

bool IsErrorValue(const base::Value& value) {
  return value.is_dict() && value.FindKey(kWebDriverErrorCodeValueField);
}

}  // namespace

CWTRequestHandler::CWTRequestHandler(ProceduralBlock session_completion_handler)
    : session_completion_handler_(session_completion_handler),
      script_timeout_(kDefaultScriptTimeout),
      page_load_timeout_(kDefaultPageLoadTimeout) {}

base::Optional<base::Value> CWTRequestHandler::ProcessCommand(
    const std::string& command,
    net::test_server::HttpMethod http_method,
    const std::string& request_content) {
  if (http_method == net::test_server::METHOD_GET) {
    if (session_id_.empty()) {
      return CreateErrorValue(kWebDriverInvalidSessionError,
                              kWebDriverNoActiveSessionMessage);
    }

    if (command == kWebDriverWindowCommand)
      return GetTargetTabId();

    if (command == kWebDriverWindowHandlesCommand)
      return GetAllTabIds();

    if (command == kWebDriverScreenshotCommand)
      return GetSnapshot();

    return base::nullopt;
  }

  if (http_method == net::test_server::METHOD_POST) {
    base::Optional<base::Value> content =
        base::JSONReader::Read(request_content);
    if (!content || !content->is_dict()) {
      return CreateErrorValue(kWebDriverInvalidArgumentError,
                              kWebDriverMissingRequestMessage);
    }

    if (command == kWebDriverSessionCommand)
      return InitializeSession();

    if (session_id_.empty()) {
      return CreateErrorValue(kWebDriverInvalidSessionError,
                              kWebDriverNoActiveSessionMessage);
    }

    if (command == kWebDriverNavigationCommand)
      return NavigateToUrl(content->FindKey(kWebDriverURLRequestField));

    if (command == kWebDriverTimeoutsCommand)
      return SetTimeouts(*content);

    if (command == kWebDriverWindowCommand) {
      return SwitchToTabWithId(
          content->FindKey(kWebDriverWindowHandleRequestField));
    }

    if (command == kWebDriverSyncScriptCommand) {
      return ExecuteScript(content->FindKey(kWebDriverScriptRequestField),
                           /*is_async_function=*/false);
    }

    if (command == kWebDriverAsyncScriptCommand) {
      return ExecuteScript(content->FindKey(kWebDriverScriptRequestField),
                           /*is_async_function=*/true);
    }

    if (command == kWebDriverWindowRectCommand)
      return SetWindowRect(*content);

    return base::nullopt;
  }

  if (http_method == net::test_server::METHOD_DELETE) {
    if (session_id_.empty()) {
      return CreateErrorValue(kWebDriverInvalidSessionError,
                              kWebDriverNoActiveSessionMessage);
    }

    if (command == session_id_)
      return CloseSession();

    if (command == kWebDriverWindowCommand)
      return CloseTargetTab();

    return base::nullopt;
  }

  return base::nullopt;
}

std::unique_ptr<net::test_server::HttpResponse>
CWTRequestHandler::HandleRequest(const net::test_server::HttpRequest& request) {
  std::string command = request.GetURL().ExtractFileName();
  base::Optional<base::Value> result =
      ProcessCommand(command, request.method, request.content);

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("application/json; charset=utf-8");
  response->AddCustomHeader("Cache-Control", "no-cache");
  if (!result) {
    response->set_code(net::HTTP_NOT_FOUND);
    result = CreateErrorValue(kWebDriverUnknownCommandError,
                              kWebDriverUnknownCommandMessage);
  } else if (IsErrorValue(*result)) {
    response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
  } else {
    response->set_code(net::HTTP_OK);
  }

  base::Value response_content(base::Value::Type::DICTIONARY);
  response_content.SetKey(kWebDriverValueResponseField, std::move(*result));
  std::string response_content_string;
  base::JSONWriter::Write(response_content, &response_content_string);
  response->set_content(response_content_string);

  return std::move(response);
}

base::Value CWTRequestHandler::InitializeSession() {
  if (!session_id_.empty()) {
    return CreateErrorValue(kWebDriverSessionCreationError,
                            kWebDriverSessionAlreadyExistsMessage);
  }

  [CWTWebDriverAppInterface enablePopups];
  target_tab_id_ =
      base::SysNSStringToUTF8([CWTWebDriverAppInterface currentTabID]);

  base::Value result(base::Value::Type::DICTIONARY);
  session_id_ = base::GenerateGUID();
  result.SetStringKey(kWebDriverSessionIdValueField, session_id_);

  base::Value capabilities(base::Value::Type::DICTIONARY);
  capabilities.SetStringKey(kCapabilitiesBrowserNameField,
                            kCapabilitiesBrowserName);
  capabilities.SetStringKey(kCapabilitiesBrowserVersionField,
                            version_info::GetVersionNumber());
  capabilities.SetStringKey(kCapabilitiesPlatformNameField,
                            kCapabilitiesPlatformName);
  capabilities.SetStringKey(kCapabilitiesPageLoadStrategyField,
                            kCapabilitiesPageLoadStrategy);
  capabilities.SetKey(kCapabilitiesProxyField,
                      base::Value(base::Value::Type::DICTIONARY));
  capabilities.SetIntPath(kCapabilitiesScriptTimeoutField,
                          script_timeout_ * 1000);
  capabilities.SetIntPath(kCapabilitiesPageLoadTimeoutField,
                          page_load_timeout_ * 1000);
  capabilities.SetIntPath(kCapabilitiesImplicitTimeoutField, 0);
  capabilities.SetKey(kCapabilitiesCanResizeWindowsField, base::Value(false));

  result.SetKey(kWebDriverCapabilitiesValueField, std::move(capabilities));
  return result;
}

base::Value CWTRequestHandler::CloseSession() {
  session_id_.clear();
  session_completion_handler_();
  return base::Value(base::Value::Type::NONE);
}

base::Value CWTRequestHandler::NavigateToUrl(const base::Value* url) {
  if (!url || !url->is_string()) {
    return CreateErrorValue(kWebDriverInvalidArgumentError,
                            kWebDriverMissingURLMessage);
  }

  NSError* error = [CWTWebDriverAppInterface
               loadURL:base::SysUTF8ToNSString(url->GetString())
                 inTab:base::SysUTF8ToNSString(target_tab_id_)
      timeoutInSeconds:page_load_timeout_];
  if (!error)
    return base::Value(base::Value::Type::NONE);

  return CreateErrorValue(kWebDriverTimeoutError,
                          kWebDriverPageLoadTimeoutMessage);
}

base::Value CWTRequestHandler::SetTimeouts(const base::Value& timeouts) {
  for (const auto& timeout : timeouts.DictItems()) {
    if (!timeout.second.is_int() || timeout.second.GetInt() < 0) {
      return CreateErrorValue(kWebDriverInvalidArgumentError,
                              kWebDriverInvalidTimeoutMessage);
    }

    int timeout_in_milliseconds = timeout.second.GetInt();
    NSTimeInterval timeout_in_seconds =
        static_cast<double>(timeout_in_milliseconds) / 1000;

    // Only script and page load timeouts are supported in CWTChromeDriver.
    // Other values are ignored.
    if (timeout.first == kWebDriverScriptTimeoutRequestField)
      script_timeout_ = timeout_in_seconds;
    else if (timeout.first == kWebDriverPageLoadTimeoutRequestField)
      page_load_timeout_ = timeout_in_seconds;
  }
  return base::Value(base::Value::Type::NONE);
}

base::Value CWTRequestHandler::GetTargetTabId() {
  NSArray* tab_ids = [CWTWebDriverAppInterface tabIDs];
  if ([tab_ids indexOfObject:base::SysUTF8ToNSString(target_tab_id_)] ==
      NSNotFound) {
    return CreateErrorValue(kWebDriverNoSuchWindowError,
                            kWebDriverNoTargetWindowMessage);
  }

  return base::Value(target_tab_id_);
}

base::Value CWTRequestHandler::GetAllTabIds() {
  base::Value id_list(base::Value::Type::LIST);
  NSArray* tab_ids = [CWTWebDriverAppInterface tabIDs];
  for (NSString* tab_id in tab_ids) {
    id_list.Append(base::Value(base::SysNSStringToUTF8(tab_id)));
  }
  return id_list;
}

base::Value CWTRequestHandler::SwitchToTabWithId(const base::Value* tab_id) {
  if (!tab_id || !tab_id->is_string()) {
    return CreateErrorValue(kWebDriverInvalidArgumentError,
                            kWebDriverMissingWindowHandleMessage);
  }

  NSError* error = [CWTWebDriverAppInterface
      switchToTabWithID:base::SysUTF8ToNSString(tab_id->GetString())];

  if (!error) {
    target_tab_id_ = tab_id->GetString();
    return base::Value(base::Value::Type::NONE);
  }

  return CreateErrorValue(kWebDriverNoSuchWindowError,
                          kWebDriverNoMatchingWindowMessage);
}

base::Value CWTRequestHandler::CloseTargetTab() {
  NSError* error = [CWTWebDriverAppInterface
      closeTabWithID:base::SysUTF8ToNSString(target_tab_id_)];
  target_tab_id_.clear();

  if (error) {
    return CreateErrorValue(kWebDriverNoSuchWindowError,
                            kWebDriverNoTargetWindowMessage);
  }

  return GetAllTabIds();
}

base::Value CWTRequestHandler::ExecuteScript(const base::Value* script,
                                             bool is_async_function) {
  if (!script || !script->is_string()) {
    return CreateErrorValue(kWebDriverInvalidArgumentError,
                            kWebDriverMissingScriptMessage);
  }

  NSString* function_to_execute;
  if (is_async_function) {
    // The provided |script| is a function body that already calls its last
    // argument with the result of its computation.
    function_to_execute =
        [NSString stringWithFormat:@"function f(completionHandler) { %s }",
                                   script->GetString().c_str()];
  } else {
    // The provided |script| directly computes a result. Convert to a function
    // that calls a completion handler with the result of its computation.
    NSString* input_function = [NSString
        stringWithFormat:@"() => { %s }", script->GetString().c_str()];
    function_to_execute =
        [NSString stringWithFormat:@"function f(completionHandler) { "
                                   @"  completionHandler((%@).call()); "
                                   @"} ",
                                   input_function];
  }

  NSString* result_as_json = [CWTWebDriverAppInterface
      executeAsyncJavaScriptFunction:function_to_execute
                               inTab:base::SysUTF8ToNSString(target_tab_id_)
                    timeoutInSeconds:script_timeout_];

  if (!result_as_json) {
    return CreateErrorValue(kWebDriverScriptTimeoutError,
                            kWebDriverScriptTimeoutMessage);
  }

  base::Optional<base::Value> result =
      base::JSONReader::Read(base::SysNSStringToUTF8(result_as_json));
  DCHECK(result);
  return std::move(*result);
}

base::Value CWTRequestHandler::GetSnapshot() {
  NSString* snapshot_image = [CWTWebDriverAppInterface
      takeSnapshotOfTabWithID:base::SysUTF8ToNSString(target_tab_id_)];
  if (!snapshot_image) {
    return CreateErrorValue(kWebDriverNoSuchWindowError,
                            kWebDriverNoMatchingWindowMessage);
  }

  return base::Value(base::SysNSStringToUTF8(snapshot_image));
}

base::Value CWTRequestHandler::SetWindowRect(const base::Value& rect) {
  return base::Value();
}
