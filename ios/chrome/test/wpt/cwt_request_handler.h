// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_WPT_CWT_REQUEST_HANDLER_H_
#define IOS_CHROME_TEST_WPT_CWT_REQUEST_HANDLER_H_

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/ios/block_types.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

// Implements a subset of the WebDriver protocol, for running Web Platform
// Tests. This not intended to be a general-purpose WebDriver implementation.
// Each CWTRequestHandler supports only a single session. Additional
// requests to create a session while one is already active will return an
// error response.
//
// See https://w3c.github.io/webdriver/ for the complete WebDriver protocol
// specification. In addition to only implementing a subset of this protocol,
// CWTRequestHandler only performs minimal error-checking for the sake
// of making clients easier to debug, and otherwise assumes that the client
// is submitting well-formed requests, and that all requests are coming from
// a single client.
//
// For example, to create a session, load a URL, and then close the session, the
// following sequence of requests can be used:
// 1) POST /session
// 2) POST /url (with a content body that is a JSON dictionary whose 'url'
//    property's value is the URL to navigate to)
// 3) DELETE /session/{session id}
class CWTRequestHandler {
 public:
  // `session_completion_handler` is a block to be called when a session is
  // closed.
  CWTRequestHandler(ProceduralBlock sesssion_completion_handler);

  CWTRequestHandler(const CWTRequestHandler&) = delete;
  CWTRequestHandler& operator=(const CWTRequestHandler&) = delete;

  ~CWTRequestHandler();

  // Creates responses for HTTP requests according to the WebDriver protocol.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

 private:
  // Creates a new session, if no session has already been created. Otherwise,
  // return an error. Sets the target tab to the current tab.
  base::Value InitializeSession();

  // Terminates the current session.
  base::Value CloseSession();

  // Navigates the target tab to the given URL, and waits for the page load to
  // complete.
  base::Value NavigateToUrl(const std::string* url);

  // Navigates the target tab to the URL given in `input`, waits for the page
  // load to complete, and then waits for the additional time specified in
  // `input`. Returns the stderr output produced by the app during page load.
  base::Value NavigateToUrlForCrashTest(const base::Value& input);

  // Sets timeouts used when performing browser operations.
  base::Value SetTimeouts(const base::Value& timeouts);

  // Gets the id of the target tab. Returns an error if the target tab has been
  // closed.
  base::Value GetTargetTabId();

  // Gets the ids of all open tabs.
  base::Value GetAllTabIds();

  // Switches to the tab with the given id and makes this the target tab.
  // Returns an error value if no such tab exists.
  base::Value SwitchToTabWithId(const std::string* tab_id);

  // Closes the target tab. Returns an error value if no tab is open.
  // Otherwise, returns the ids of the remaining tabs.
  base::Value CloseTargetTab();

  // Releases keys and buttons that are currently pressed as a result of
  // performed actions. This is currently a no-op since performing actions is
  // not supported.
  base::Value ReleaseActions();

  // Executes the given script in the target tab. Returns an error if script
  // execution times out. Otherwise, returns the result of script execution.
  // When `is_async_function` is true, the given script must be the body of a
  // function that uses its last argument (that is, the argument at
  // "arguments[arguments.length - 1]") as a completion handler that it calls
  // (possibly asynchronously) with the result to be returned. When
  // `is_async_function` is false, the given script must be the body of a
  // function whose return value is the result to be returned.
  // `args` provides the window url used by async functions.
  //
  // Examples:
  // 1) `script` is "arguments[arguments.length - 1].call(7)" and
  //    `is_async_function` is true. In this case, the return value is |7`.
  // 2) `script` is "return 'hello';" and `is_async_function` is false. In this
  //    case, the return value is |'hello'|.
  // 3) `script` is "document.title = 'hello world';" and `is_async_function` is
  //    false. In this case, the script's return value is "undefined" so the
  //    value returned by this method is a default-constructed base::Value.
  base::Value ExecuteScript(const std::string* script,
                            bool is_async_function,
                            const base::Value::List* args);

  // Takes a snapshot of the target tab. Returns an error value if the target
  // tab is no longer open. Otherwise, returns the snapshot as a base64-encoded
  // image.
  base::Value GetSnapshot();

  // Returns the Chrome version and revision number for the current build.
  base::Value GetVersionInfo();

  // Set the target tab's position and size. This is currently a no-op since
  // tabs cannot be arbitrarily sized or positioned on iOS. It may make sense
  // to implement this in the future on iPad-only, once multiwindow support on
  // iPad is more fully fleshed out.
  base::Value SetWindowRect(const base::Value& rect);

  // Processes the given command, HTTP method, and request content. Returns the
  // result of processing the command, or nullopt_t if the command is unknown.
  std::optional<base::Value> ProcessCommand(
      const std::string& command,
      net::test_server::HttpMethod http_method,
      const std::string& request_content);

  // Block that gets called when a session is terminated.
  ProceduralBlock session_completion_handler_;

  // A randomly-generated identifier created by InitializeSession().
  std::string session_id_;

  // The tab that's the target of WebDriver actions.
  std::string target_tab_id_;

  // Timeouts used when performing browser operations.
  base::TimeDelta script_timeout_;
  base::TimeDelta page_load_timeout_;

  // A server for test files used in crash tests.
  net::EmbeddedTestServer test_case_server_;

  // The directory used for test files for crash tests.
  base::FilePath test_case_directory_;

  // The instance of Chrome that's being tested.
  XCUIApplication* application_;
};

#endif  // IOS_CHROME_TEST_WPT_CWT_REQUEST_HANDLER_H_
