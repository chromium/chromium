// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/testing_instance.h"

#include <stddef.h>

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

#include "ppapi/cpp/core.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/view.h"
#include "ppapi/tests/test_case.h"

TestCaseFactory* TestCaseFactory::head_ = NULL;

// Cookie value we use to signal "we're still working." See the comment above
// the class declaration for how this works.
static const char kProgressSignal[] = "...";

// Returns a new heap-allocated test case for the given test, or NULL on
// failure.
TestingInstance::TestingInstance(PP_Instance instance)
#if (defined __native_client__)
    : pp::Instance(instance),
#else
    : pp::InstancePrivate(instance),
#endif
      current_case_(NULL),
      executed_tests_(false),
      number_tests_executed_(0),
      nacl_mode_(false),
      ssl_server_port_(-1),
      websocket_port_(-1),
      remove_plugin_(true) {
  callback_factory_.Initialize(this);
}

TestingInstance::~TestingInstance() {
  if (current_case_)
    delete current_case_;
}

bool TestingInstance::Init(uint32_t argc,
                           const char* argn[],
                           const char* argv[]) {
  for (uint32_t i = 0; i < argc; i++) {
    if (std::strcmp(argn[i], "mode") == 0) {
      if (std::strcmp(argv[i], "nacl") == 0)
        nacl_mode_ = true;
    } else if (std::strcmp(argn[i], "protocol") == 0) {
      protocol_ = argv[i];
    } else if (std::strcmp(argn[i], "websocket_host") == 0) {
      websocket_host_ = argv[i];
    } else if (std::strcmp(argn[i], "websocket_port") == 0) {
      websocket_port_ = atoi(argv[i]);
    } else if (std::strcmp(argn[i], "ssl_server_port") == 0) {
      ssl_server_port_ = atoi(argv[i]);
    }
  }
  // Create the proper test case from the argument.
  for (uint32_t i = 0; i < argc; i++) {
    if (std::strcmp(argn[i], "testcase") == 0) {
      if (argv[i][0] == '\0')
        break;
      current_case_ = CaseForTestName(argv[i]);
      test_filter_ = argv[i];
      if (!current_case_)
        errors_.append(std::string("Unknown test case ") + argv[i]);
      else if (!current_case_->Init())
        errors_.append(" Test case could not initialize.");
      return true;
    }
  }

  // In DidChangeView, we'll dump out a list of all available tests.
  return true;
}

#if !(defined __native_client__)
pp::Var TestingInstance::GetInstanceObject() {
  if (current_case_)
    return current_case_->GetTestObject();

  return pp::VarPrivate();
}
#endif

void TestingInstance::HandleMessage(const pp::Var& message_data) {
  if (current_case_)
    current_case_->HandleMessage(message_data);
}

void TestingInstance::DidChangeView(const pp::View& view) {
  if (!executed_tests_) {
    executed_tests_ = true;
    pp::Module::Get()->core()->CallOnMainThread(
        0,
        callback_factory_.NewCallback(&TestingInstance::ExecuteTests));
  }
  if (current_case_)
    current_case_->DidChangeView(view);
}

bool TestingInstance::HandleInputEvent(const pp::InputEvent& event) {
  if (current_case_)
    return current_case_->HandleInputEvent(event);
  return false;
}

void TestingInstance::EvalScript(const std::string& script) {
  SendTestCommand("EvalScript", script);
}

void TestingInstance::SetCookie(const std::string& name,
                                const std::string& value) {
  SendTestCommand("SetCookie", name + "=" + value);
}

void TestingInstance::LogTest(const std::string& test_name,
                              const std::string& error_message,
                              PP_TimeTicks start_time) {
  current_test_name_ = test_name;

  // Compute the time to run the test and save it in a string for logging:
  PP_TimeTicks end_time(pp::Module::Get()->core()->GetTimeTicks());
  std::ostringstream number_stream;
  PP_TimeTicks elapsed_time(end_time - start_time);
  number_stream << std::fixed << std::setprecision(3) << elapsed_time;
  std::string time_string(number_stream.str());

  // Tell the browser we're still working.
  ReportProgress(kProgressSignal);

  number_tests_executed_++;

  std::string html;
  html.append("<div class=\"test_line\"><span class=\"test_name\">");
  html.append(test_name);
  html.append("</span> ");
  if (error_message.empty()) {
    html.append("<span class=\"pass\">PASS</span>");
  } else {
    html.append("<span class=\"fail\">FAIL</span>: <span class=\"err_msg\">");
    html.append(error_message);
    html.append("</span>");

    if (!errors_.empty())
      errors_.append(", ");  // Separator for different error messages.
    errors_.append(test_name + " FAIL: " + error_message);
  }
  html.append(" <span class=\"time\">(");
  html.append(time_string);
  html.append("s)</span>");

  html.append("</div>");
  LogHTML(html);

  std::string test_time;
  test_time.append(test_name);
  test_time.append(" finished in ");
  test_time.append(time_string);
  test_time.append(" seconds.");
  LogTestTime(test_time);

  current_test_name_.clear();
}

void TestingInstance::AppendError(const std::string& message) {
  if (!errors_.empty())
    errors_.append(", ");
  errors_.append(message);
}

void TestingInstance::ExecuteTests(int32_t unused) {
  ReportProgress(kProgressSignal);

  // Clear the console.
  SendTestCommand("ClearConsole");

  if (!errors_.empty()) {
    // Catch initialization errors and output the current error string to
    // the console.
    LogError("Plugin initialization failed: " + errors_);
  } else if (!current_case_) {
    LogAvailableTests();
    errors_.append("FAIL: Only listed tests");
  } else {
    current_case_->RunTests(test_filter_);

    if (number_tests_executed_ == 0) {
      errors_.append("No tests executed. The test filter might be too "
                     "restrictive: '" + test_filter_ + "'.");
      LogError(errors_);
    }
    if (current_case_->skipped_tests().size()) {
      // TODO(dmichael): Convert all TestCases to run all tests in one fixture,
      //                 and enable this check. Currently, a lot of our tests
      //                 run 1 test per fixture, which is slow.
      /*
      errors_.append("Some tests were not listed and thus were not run. Make "
                     "sure all tests are passed in the test_case URL (even if "
                     "they are marked DISABLED_). Forgotten tests: ");
      std::set<std::string>::const_iterator iter =
          current_case_->skipped_tests().begin();
      for (; iter != current_case_->skipped_tests().end(); ++iter) {
        errors_.append(*iter);
        errors_.append(" ");
      }
      LogError(errors_);
      */
    }
    if (current_case_->remaining_tests().size()) {
      errors_.append("Some listed tests were not found in the TestCase. Check "
                     "the test names that were passed to make sure they match "
                     "tests in the TestCase. Unknown tests: ");
      std::map<std::string, bool>::const_iterator iter =
          current_case_->remaining_tests().begin();
      for (; iter != current_case_->remaining_tests().end(); ++iter) {
        errors_.append(iter->first);
        errors_.append(" ");
      }
      LogError(errors_);
    }
  }

  if (remove_plugin_)
    SendTestCommand("RemovePluginWhenFinished");
  std::string result(errors_);
  if (result.empty())
    result = "PASS";
  SendTestCommand("DidExecuteTests", result);
  // Note, DidExecuteTests may unload the plugin. We can't really do anything
  // after this point.
}

TestCase* TestingInstance::CaseForTestName(const std::string& name) {
  std::string case_name = name.substr(0, name.find_first_of('_'));
  TestCaseFactory* iter = TestCaseFactory::head_;
  while (iter != NULL) {
    if (case_name == iter->name_)
      return iter->method_(this);
    iter = iter->next_;
  }
  return NULL;
}

void TestingInstance::SendTestCommand(const std::string& command) {
  std::string msg("TESTING_MESSAGE:");
  msg += command;
  PostMessage(pp::Var(msg));
}

void TestingInstance::SendTestCommand(const std::string& command,
                                      const std::string& params) {
  SendTestCommand(command + ":" + params);
}


void TestingInstance::LogAvailableTests() {
  // Print out a listing of all tests.
  std::vector<std::string> test_cases;
  TestCaseFactory* iter = TestCaseFactory::head_;
  while (iter != NULL) {
    test_cases.push_back(iter->name_);
    iter = iter->next_;
  }
  std::sort(test_cases.begin(), test_cases.end());

  std::string html;
  html.append("Available test cases: <dl>");
  for (size_t i = 0; i < test_cases.size(); ++i) {
    html.append("<dd><a href='?testcase=");
    html.append(test_cases[i]);
    if (nacl_mode_)
       html.append("&mode=nacl");
    html.append("'>");
    html.append(test_cases[i]);
    html.append("</a></dd>");
  }
  html.append("</dl>");
  html.append("<button onclick='RunAll()'>Run All Tests</button>");

  LogHTML(html);
}

void TestingInstance::LogError(const std::string& text) {
  std::string html;
  html.append("<span class=\"fail\">FAIL</span>: <span class=\"err_msg\">");
  html.append(text);
  html.append("</span>");
  LogHTML(html);
}

void TestingInstance::LogHTML(const std::string& html) {
  SendTestCommand("LogHTML", html);
}

void TestingInstance::ReportProgress(const std::string& progress_value) {
  SendTestCommand("ReportProgress", progress_value);
}

void TestingInstance::AddPostCondition(const std::string& script) {
  SendTestCommand("AddPostCondition", script);
}

void TestingInstance::LogTestTime(const std::string& test_time) {
  SendTestCommand("LogTestTime", test_time);
}

class Module : public pp::Module {
 public:
  Module() : pp::Module() {}
  virtual ~Module() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new TestingInstance(instance);
  }
};

namespace pp {

#if defined(WIN32)
__declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif
Module* CreateModule() {
  return new ::Module();
}

}  // namespace pp
