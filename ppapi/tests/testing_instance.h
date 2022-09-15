// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TESTING_INSTANCE_H_
#define PPAPI_TESTS_TESTING_INSTANCE_H_

#include <stdint.h>

#include <string>

#include "ppapi/utility/completion_callback_factory.h"

#if defined(__native_client__)
#include "ppapi/cpp/instance.h"
#else
#include "ppapi/cpp/private/instance_private.h"
#endif

// Windows defines 'PostMessage', so we have to undef it.
#ifdef PostMessage
#undef PostMessage
#endif

class TestCase;

// How signaling works:
//
// We want to signal to the Chrome browser test harness
// (chrome/test/ppapi/ppapi_browsertest.cc) that we're making progress and when
// we're done. This is done using the DOM controlller. The browser test waits
// for a message from it. We don't want to have a big wait for all tests in a
// TestCase since they can take a while and it might timeout.  So we send it
// pings between each test to tell it that we're still running tests and aren't
// stuck.
//
// If the value of the message is "..." then that tells the test runner that
// the test is progressing. It then waits for the next message until it either
// times out or the value is something other than "...". In this case, the value
// will be either "PASS" or "FAIL [optional message]" corresponding to the
// outcome of the entire test case. Timeout will be treated just like a failure
// of the entire test case and the test will be terminated.
//
// In trusted builds, we use InstancePrivate and allow tests that use
// synchronous scripting. NaCl does not support synchronous scripting.
class TestingInstance : public
#if defined(__native_client__)
pp::Instance {
#else
pp::InstancePrivate {
#endif
 public:
  explicit TestingInstance(PP_Instance instance);
  virtual ~TestingInstance();

  // pp::Instance override.
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);
  virtual void DidChangeView(const pp::View& view);
  virtual bool HandleInputEvent(const pp::InputEvent& event);

#if !(defined __native_client__)
  virtual pp::Var GetInstanceObject();
#endif

  // Outputs the information from one test run, using the format
  //   <test_name> [PASS|FAIL <error_message>]
  //
  // You should generally use one of the RUN_TEST* macros in test_case.h
  // instead.
  //
  // If error_message is empty, we say the test passed and emit PASS. If
  // error_message is nonempty, the test failed with that message as the error
  // string.
  //
  // Intended usage:
  //   PP_TimeTicks start_time(core.GetTimeTicks());
  //   LogTest("Foo", FooTest(), start_time);
  //
  // Where FooTest is defined as:
  //   std::string FooTest() {
  //     if (something_horrible_happened)
  //       return "Something horrible happened";
  //     return "";
  //   }
  //
  // NOTE: It's important to get the start time in the previous line, rather
  //       than calling GetTimeTicks in the LogTestLine. There's no guarantee
  //       that GetTimeTicks will be evaluated before FooTest().
  void LogTest(const std::string& test_name,
               const std::string& error_message,
               PP_TimeTicks start_time);
  const std::string& current_test_name() { return current_test_name_; }

  // Appends an error message to the log.
  void AppendError(const std::string& message);

  // Passes the message_data through to the HandleMessage method on the
  // TestClass object that's associated with this instance.
  virtual void HandleMessage(const pp::Var& message_data);

  const std::string& protocol() {
    return protocol_;
  }

  int ssl_server_port() { return ssl_server_port_; }

  const std::string& websocket_host() { return websocket_host_; }
  int websocket_port() { return websocket_port_; }

  // Posts a message to the test page to eval() the script.
  void EvalScript(const std::string& script);

  // Sets the given cookie in the current document.
  void SetCookie(const std::string& name, const std::string& value);

  void ReportProgress(const std::string& progress_value);

  // Logs the amount of time that a given test took to run. This is to help
  // debug test timeouts that occur in automated testing.
  void LogTestTime(const std::string& test_time);

  // Add a post-condition to the JavaScript on the test_case.html page. This
  // JavaScript code will be run after the instance is shut down and must
  // evaluate to |true| or the test will fail.
  void AddPostCondition(const std::string& script);

  // See doc for |remove_plugin_|.
  void set_remove_plugin(bool remove) { remove_plugin_ = remove; }

 private:
  void ExecuteTests(int32_t unused);

  // Creates a new TestCase for the give test name, or NULL if there is no such
  // test. Ownership is passed to the caller. The given string is split by '_'.
  // The test case name is the first part.
  TestCase* CaseForTestName(const std::string& name);

  // Sends a test command to the page using PostMessage.
  void SendTestCommand(const std::string& command);
  void SendTestCommand(const std::string& command, const std::string& params);

  // Appends a list of available tests to the console in the document.
  void LogAvailableTests();

  // Appends the given error test to the console in the document.
  void LogError(const std::string& text);

  // Appends the given HTML string to the console in the document.
  void LogHTML(const std::string& html);

  pp::CompletionCallbackFactory<TestingInstance> callback_factory_;

  // Owning pointer to the current test case. Valid after Init has been called.
  TestCase* current_case_;

  std::string current_test_name_;

  // A filter to use when running tests. This is passed to 'RunTests', which
  // runs only tests whose name contains test_filter_ as a substring.
  std::string test_filter_;

  // Set once the tests are run so we know not to re-run when the view is sized.
  bool executed_tests_;

  // The number of tests executed so far.
  int32_t number_tests_executed_;

  // Collects all errors to send the the browser. Empty indicates no error yet.
  std::string errors_;

  // True if running in Native Client.
  bool nacl_mode_;

  // String representing the protocol.  Used for detecting whether we're running
  // with http.
  std::string protocol_;

  // SSL server port.
  int ssl_server_port_;

  // WebSocket host.
  std::string websocket_host_;

  // WebSocket port.
  int websocket_port_;

  // At the end of each set of tests, the plugin is removed from the web-page.
  // However, for some tests, it is desirable to not remove the plguin from the
  // page.
  bool remove_plugin_;
};

#endif  // PPAPI_TESTS_TESTING_INSTANCE_H_
