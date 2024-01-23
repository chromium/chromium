// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/wpt/cwt_request_handler.h"

#import <XCTest/XCTest.h>

#import <optional>
#import <string>

#import "base/debug/stack_trace.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/json/json_reader.h"
#import "base/json/json_writer.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/uuid.h"
#import "base/values.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/test/wpt/cwt_constants.h"
#import "ios/chrome/test/wpt/cwt_webdriver_app_interface.h"
#import "ios/third_party/edo/src/Service/Sources/EDOClientService.h"
#import "net/http/http_status_code.h"

EDO_STUB_CLASS(CWTWebDriverAppInterface, kCwtEdoPortNumber)

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace {

constexpr base::TimeDelta kDefaultScriptTimeout = base::Seconds(30);
constexpr base::TimeDelta kDefaultPageLoadTimeout = base::Seconds(300);

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
const char kWebDriverActionsCommand[] = "actions";

// Non-standard commands used only for testing Chrome.
// This command is similar to the standard "url" command. It loads the URL
// specified by the kWebDriverURLRequestField argument, waits up till the
// currently-set page load time (see the standard "timeouts" command) for the
// page to finish loading, but then waits an additional amount of time
// (specified in seconds by the kChromeCrashWaitTime argument) for the page to
// crash. It then returns the stderr produced by the app during this time, in
// the kChromeStderrValueField response field. If the given URL is a file URL,
// the given file is copied and served from a local EmbeddedTestServer.
const char kChromeCrashTestCommand[] = "chrome_crashtest";

// This returns the Chrome version (e.g., "92.0.4483.0") along with the
// revision number (e.g, "872495") used for the current build.
const char kChromeVersionInfoCommand[] = "chrome_versionInfo";

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

// Non-standard error messages, used only for testing Chrome.
const char kChromeInvalidExtraWaitMessage[] =
    "Extra wait must be a non-negative integer";
const char kChromeInvalidUrlMessage[] = "The provided URL is not valid";
const char kChromeFileCannotBeCopiedMessage[] =
    "The provided input file cannot be copied";

// WebDriver request field names. These are fields that are contained within
// the body of a POST request.
const char kWebDriverURLRequestField[] = "url";
const char kWebDriverScriptTimeoutRequestField[] = "script";
const char kWebDriverPageLoadTimeoutRequestField[] = "pageLoad";
const char kWebDriverWindowHandleRequestField[] = "handle";
const char kWebDriverScriptRequestField[] = "script";
const char kWebDriverArgsRequestField[] = "args";

// Non-standard request field names, used only for testing Chrome.
// The additional time (in seconds) to wait for a crash after a successful page
// load.
const char kChromeCrashWaitTime[] = "chrome_crashWaitTime";

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

// Non-standard value field names, used only when testing Chrome.
// Stderr output from the app.
const char kChromeStderrValueField[] = "chrome_stderr";
// The revision number used for the current build.
const char kChromeRevisionNumberField[] = "chrome_revisionNumber";

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
  return base::Value(base::Value::Dict()
                         .Set(kWebDriverErrorCodeValueField, error)
                         .Set(kWebDriverErrorMessageValueField, message)
                         .Set(kWebDriverStackTraceValueField,
                              base::debug::StackTrace().ToString()));
}

bool IsErrorValue(const base::Value& value) {
  return value.is_dict() &&
         value.GetDict().contains(kWebDriverErrorCodeValueField);
}

}  // namespace

CWTRequestHandler::CWTRequestHandler(ProceduralBlock session_completion_handler)
    : session_completion_handler_(session_completion_handler),
      script_timeout_(kDefaultScriptTimeout),
      page_load_timeout_(kDefaultPageLoadTimeout) {
  application_ = [[XCUIApplication alloc] init];
  [application_ launch];
  base::CreateNewTempDirectory(base::FilePath::StringType(),
                               &test_case_directory_);
  test_case_server_.ServeFilesFromDirectory(test_case_directory_);
  if (!test_case_server_.Start()) {
    XCTFail("Unable to start test case server.");
  }
}

CWTRequestHandler::~CWTRequestHandler() = default;

std::optional<base::Value> CWTRequestHandler::ProcessCommand(
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

    if (command == kChromeVersionInfoCommand)
      return GetVersionInfo();

    return std::nullopt;
  }

  if (http_method == net::test_server::METHOD_POST) {
    std::optional<base::Value> content =
        base::JSONReader::Read(request_content);
    if (!content || !content->is_dict()) {
      return CreateErrorValue(kWebDriverInvalidArgumentError,
                              kWebDriverMissingRequestMessage);
    }
    const base::Value::Dict& content_dict = content->GetDict();

    if (command == kWebDriverSessionCommand)
      return InitializeSession();

    if (session_id_.empty()) {
      return CreateErrorValue(kWebDriverInvalidSessionError,
                              kWebDriverNoActiveSessionMessage);
    }

    if (command == kChromeCrashTestCommand)
      return NavigateToUrlForCrashTest(*content);

    if (command == kWebDriverNavigationCommand)
      return NavigateToUrl(content_dict.FindString(kWebDriverURLRequestField));

    if (command == kWebDriverTimeoutsCommand)
      return SetTimeouts(*content);

    if (command == kWebDriverWindowCommand) {
      return SwitchToTabWithId(
          content_dict.FindString(kWebDriverWindowHandleRequestField));
    }

    if (command == kWebDriverSyncScriptCommand) {
      return ExecuteScript(
          content_dict.FindString(kWebDriverScriptRequestField),
          /*is_async_function=*/false,
          content_dict.FindList(kWebDriverArgsRequestField));
    }

    if (command == kWebDriverAsyncScriptCommand) {
      return ExecuteScript(
          content_dict.FindString(kWebDriverScriptRequestField),
          /*is_async_function=*/true,
          content_dict.FindList(kWebDriverArgsRequestField));
    }

    if (command == kWebDriverWindowRectCommand)
      return SetWindowRect(*content);

    return std::nullopt;
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

    if (command == kWebDriverActionsCommand)
      return ReleaseActions();

    return std::nullopt;
  }

  return std::nullopt;
}

std::unique_ptr<net::test_server::HttpResponse>
CWTRequestHandler::HandleRequest(const net::test_server::HttpRequest& request) {
  std::string command = request.GetURL().ExtractFileName();
  std::optional<base::Value> result =
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

  auto response_content =
      base::Value::Dict().Set(kWebDriverValueResponseField, std::move(*result));
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

  base::Value::Dict result;
  session_id_ = base::Uuid::GenerateRandomV4().AsLowercaseString();
  result.Set(kWebDriverSessionIdValueField, session_id_);

  base::Value::Dict capabilities;
  capabilities.Set(kCapabilitiesBrowserNameField, kCapabilitiesBrowserName);
  capabilities.Set(kCapabilitiesBrowserVersionField,
                   version_info::GetVersionNumber());
  capabilities.Set(kCapabilitiesPlatformNameField, kCapabilitiesPlatformName);
  capabilities.Set(kCapabilitiesPageLoadStrategyField,
                   kCapabilitiesPageLoadStrategy);
  capabilities.Set(kCapabilitiesProxyField,
                   base::Value(base::Value::Type::DICT));
  capabilities.SetByDottedPath(
      kCapabilitiesScriptTimeoutField,
      static_cast<int>(script_timeout_.InMilliseconds()));
  capabilities.SetByDottedPath(
      kCapabilitiesPageLoadTimeoutField,
      static_cast<int>(page_load_timeout_.InMilliseconds()));
  capabilities.SetByDottedPath(kCapabilitiesImplicitTimeoutField, 0);
  capabilities.Set(kCapabilitiesCanResizeWindowsField, base::Value(false));

  result.Set(kWebDriverCapabilitiesValueField, std::move(capabilities));
  return base::Value(std::move(result));
}

base::Value CWTRequestHandler::CloseSession() {
  session_id_.clear();
  session_completion_handler_();
  return base::Value(base::Value::Type::NONE);
}

base::Value CWTRequestHandler::ReleaseActions() {
  return base::Value(base::Value::Type::NONE);
}

base::Value CWTRequestHandler::NavigateToUrl(const std::string* url) {
  if (!url) {
    return CreateErrorValue(kWebDriverInvalidArgumentError,
                            kWebDriverMissingURLMessage);
  }

  NSError* error =
      [CWTWebDriverAppInterface loadURL:base::SysUTF8ToNSString(*url)
                                  inTab:base::SysUTF8ToNSString(target_tab_id_)
                                timeout:page_load_timeout_];
  if (!error)
    return base::Value(base::Value::Type::NONE);

  return CreateErrorValue(kWebDriverTimeoutError,
                          kWebDriverPageLoadTimeoutMessage);
}

base::Value CWTRequestHandler::NavigateToUrlForCrashTest(
    const base::Value& input) {
  const base::Value::Dict& input_dict = input.GetDict();
  const std::string* url_str = input_dict.FindString(kWebDriverURLRequestField);
  if (!url_str) {
    return CreateErrorValue(kWebDriverInvalidArgumentError,
                            kWebDriverMissingURLMessage);
  }

  GURL url(*url_str);
  if (!url.is_valid()) {
    return CreateErrorValue(kWebDriverInvalidArgumentError,
                            kChromeInvalidUrlMessage);
  }

  if (url.SchemeIsFile()) {
    // Copy the file to the directory being served by the test server.
    base::FilePath test_input_file(url.GetContent());
    base::FilePath test_destination_file =
        test_case_directory_.Append(test_input_file.BaseName());
    bool copied_file = base::CopyFile(test_input_file, test_destination_file);
    if (!copied_file) {
      return CreateErrorValue(kWebDriverInvalidArgumentError,
                              kChromeFileCannotBeCopiedMessage);
    }
    url = test_case_server_.GetURL("/" + test_input_file.BaseName().value());
  }

  base::FilePath log_file;
  base::CreateTemporaryFile(&log_file);
  [CWTWebDriverAppInterface
      logStderrToFilePath:base::SysUTF8ToNSString(log_file.value())];
  [CWTWebDriverAppInterface installCleanExitHandlerForAbortSignal];

  // Once the test page is loaded, the app might crash at any time until the
  // tab is closed. Re-launch the app if it crashes.
  @try {
    NSError* error = [CWTWebDriverAppInterface
        loadURL:base::SysUTF8ToNSString(url.spec())
          inTab:base::SysUTF8ToNSString(target_tab_id_)
        timeout:page_load_timeout_];

    if (!error) {
      const std::optional<int> extra_wait =
          input_dict.FindInt(kChromeCrashWaitTime);
      if (extra_wait) {
        if (!extra_wait || extra_wait.value() < 0) {
          return CreateErrorValue(kWebDriverInvalidArgumentError,
                                  kChromeInvalidExtraWaitMessage);
        }
        base::test::ios::SpinRunLoopWithMinDelay(
            base::Seconds(extra_wait.value()));
      }
    }

    [CWTWebDriverAppInterface openNewTab];
    [CWTWebDriverAppInterface
        closeTabWithID:base::SysUTF8ToNSString(target_tab_id_)];
    target_tab_id_ =
        base::SysNSStringToUTF8([CWTWebDriverAppInterface currentTabID]);

    [CWTWebDriverAppInterface stopLoggingStderr];
  } @catch (NSException* exception) {
    dispatch_sync(dispatch_get_main_queue(), ^{
      [application_ launch];
    });
    target_tab_id_ =
        base::SysNSStringToUTF8([CWTWebDriverAppInterface currentTabID]);
  }

  std::string stderr_contents;
  base::ReadFileToString(log_file, &stderr_contents);

  return base::Value(
      base::Value::Dict().Set(kChromeStderrValueField, stderr_contents));
}

base::Value CWTRequestHandler::SetTimeouts(const base::Value& timeouts) {
  for (const auto pair : timeouts.GetDict()) {
    if (!pair.second.is_int() || pair.second.GetInt() < 0) {
      return CreateErrorValue(kWebDriverInvalidArgumentError,
                              kWebDriverInvalidTimeoutMessage);
    }

    const base::TimeDelta timeout = base::Milliseconds(pair.second.GetInt());

    // Only script and page load timeouts are supported in CWTChromeDriver.
    // Other values are ignored.
    if (pair.first == kWebDriverScriptTimeoutRequestField)
      script_timeout_ = timeout;
    else if (pair.first == kWebDriverPageLoadTimeoutRequestField)
      page_load_timeout_ = timeout;
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
  base::Value::List id_list;
  NSArray* tab_ids = [CWTWebDriverAppInterface tabIDs];
  for (NSString* tab_id in tab_ids) {
    id_list.Append(base::Value(base::SysNSStringToUTF8(tab_id)));
  }
  return base::Value(std::move(id_list));
}

base::Value CWTRequestHandler::SwitchToTabWithId(const std::string* tab_id) {
  if (!tab_id) {
    return CreateErrorValue(kWebDriverInvalidArgumentError,
                            kWebDriverMissingWindowHandleMessage);
  }

  NSError* error = [CWTWebDriverAppInterface
      switchToTabWithID:base::SysUTF8ToNSString(*tab_id)];

  if (!error) {
    target_tab_id_ = *tab_id;
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

base::Value CWTRequestHandler::ExecuteScript(const std::string* script,
                                             bool is_async_function,
                                             const base::Value::List* args) {
  if (!script) {
    return CreateErrorValue(kWebDriverInvalidArgumentError,
                            kWebDriverMissingScriptMessage);
  }

  NSString* function_to_execute;
  if (is_async_function) {
    // The provided `script` is a function body that already calls its last
    // argument with the result of its computation.
    NSString* updated_script = base::SysUTF8ToNSString(*script);
    // Update the url if exists in the args
    if (args && args->size() > 0) {
      NSString* script_url = [NSString
          stringWithFormat:@"\"%s\"", args->front().GetString().c_str()];
      updated_script =
          [updated_script stringByReplacingOccurrencesOfString:@"arguments[0]"
                                                    withString:script_url];
    }
    function_to_execute =
        [NSString stringWithFormat:@"function f(completionHandler) { %@ }",
                                   updated_script];
  } else {
    // The provided `script` directly computes a result. Convert to a function
    // that calls a completion handler with the result of its computation.
    NSString* input_function =
        [NSString stringWithFormat:@"() => { %s }", script->c_str()];
    function_to_execute =
        [NSString stringWithFormat:@"function f(completionHandler) { "
                                   @"  completionHandler((%@).call()); "
                                   @"} ",
                                   input_function];
  }

  NSString* result_as_json = [CWTWebDriverAppInterface
      executeAsyncJavaScriptFunction:function_to_execute
                               inTab:base::SysUTF8ToNSString(target_tab_id_)
                             timeout:script_timeout_];

  if (!result_as_json) {
    return CreateErrorValue(kWebDriverScriptTimeoutError,
                            kWebDriverScriptTimeoutMessage);
  }

  std::optional<base::Value> result =
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

base::Value CWTRequestHandler::GetVersionInfo() {
  auto result = base::Value::Dict().Set(kCapabilitiesBrowserVersionField,
                                        version_info::GetVersionNumber());

  // The full revision starts with a git hash and ends with the revision
  // number in the following format: @{#123456}
  std::string full_revision(version_info::GetLastChange());
  size_t start_position = full_revision.rfind("#") + 1;

  if (start_position == std::string::npos) {
    result.Set(kChromeRevisionNumberField, "0");
  } else {
    size_t length = full_revision.size() - start_position - 1;
    result.Set(kChromeRevisionNumberField,
               full_revision.substr(start_position, length));
  }
  return base::Value(std::move(result));
}
