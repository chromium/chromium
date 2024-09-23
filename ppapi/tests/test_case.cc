// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_case.h"

#include <stddef.h>
#include <string.h>

#include <algorithm>
#include <sstream>

#include "ppapi/cpp/core.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/pp_thread.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

namespace {

std::string StripPrefix(const std::string& test_name) {
  if (test_name.find("DISABLED_") == 0)
    return test_name.substr(strlen("DISABLED_"));
  return test_name;
}

// Strip the TestCase name off and return the remainder (i.e., everything after
// '_'). If there is no '_', assume only the TestCase was provided, and return
// an empty string.
// For example:
//   StripTestCase("TestCase_TestName");
// returns
//   "TestName"
// while
//   StripTestCase("TestCase);
// returns
//   ""
std::string StripTestCase(const std::string& full_test_name) {
  size_t delim = full_test_name.find_first_of('_');
  if (delim != std::string::npos)
    return full_test_name.substr(delim+1);
  // In this case, our "filter" is the empty string; the full test name is the
  // same as the TestCase name with which we were constructed.
  // TODO(dmichael): It might be nice to be able to PP_DCHECK against the
  // TestCase class name, but we'd have to plumb that name to TestCase somehow.
  return std::string();
}

// Parse |test_filter|, which is a comma-delimited list of (possibly prefixed)
// test names and insert the un-prefixed names into |remaining_tests|, with
// the bool indicating whether the test should be run.
void ParseTestFilter(const std::string& test_filter,
                     std::map<std::string, bool>* remaining_tests) {
  // We can't use base/strings/string_util.h::Tokenize in ppapi, so we have to
  // do it ourselves.
  std::istringstream filter_stream(test_filter);
  std::string current_test;
  while (std::getline(filter_stream, current_test, ',')) {
    // |current_test| might include a prefix, like DISABLED_Foo_TestBar, so we
    // we strip it off if there is one.
    std::string stripped_test_name(StripPrefix(current_test));
    // Strip off the test case and use the test name as a key, because the test
    // name ShouldRunTest wants to use to look up the test doesn't have the
    // TestCase name.
    std::string test_name_without_case(StripTestCase(stripped_test_name));

    // If the test wasn't prefixed, it should be run.
    bool should_run_test = (current_test == stripped_test_name);
    PP_DCHECK(remaining_tests->count(test_name_without_case) == 0);
    remaining_tests->insert(
        std::make_pair(test_name_without_case, should_run_test));
  }
  // There may be a trailing comma; ignore empty strings.
  remaining_tests->erase(std::string());
}

}  // namespace

TestCase::TestCase(TestingInstance* instance)
    : instance_(instance),
      testing_interface_(NULL),
      callback_type_(PP_REQUIRED),
      have_populated_filter_tests_(false) {
  // Get the testing_interface_ if it is available, so that we can do Resource
  // and Var checks on shutdown (see CheckResourcesAndVars). If it is not
  // available, testing_interface_ will be NULL. Some tests do not require it.
  testing_interface_ = GetTestingInterface();
}

TestCase::~TestCase() {
}

bool TestCase::Init() {
  return true;
}

// static
std::string TestCase::MakeFailureMessage(const char* file,
                                         int line,
                                         const char* cmd) {
  std::ostringstream output;
  output << "Failure in " << file << "(" << line << "): " << cmd;
  return output.str();
}

#if !(defined __native_client__)
pp::VarPrivate TestCase::GetTestObject() {
  if (test_object_.is_undefined()) {
    pp::deprecated::ScriptableObject* so = CreateTestObject();
    if (so) {
      test_object_ = pp::VarPrivate(instance_, so);  // Takes ownership.
      // CheckResourcesAndVars runs and looks for leaks before we've actually
      // completely shut down. Ignore the instance object, since it's not a real
      // leak.
      IgnoreLeakedVar(test_object_.pp_var().value.as_id);
    }
  }
  return test_object_;
}
#endif

bool TestCase::CheckTestingInterface() {
  testing_interface_ = GetTestingInterface();
  if (!testing_interface_) {
    // Give a more helpful error message for the testing interface being gone
    // since that needs special enabling in Chrome.
    instance_->AppendError("This test needs the testing interface, which is "
                           "not currently available. In Chrome, use "
                           "--enable-pepper-testing when launching.");
    return false;
  }

  return true;
}

void TestCase::HandleMessage(const pp::Var& message_data) {
}

void TestCase::DidChangeView(const pp::View& view) {
}

bool TestCase::HandleInputEvent(const pp::InputEvent& event) {
  return false;
}

void TestCase::IgnoreLeakedVar(int64_t id) {
  ignored_leaked_vars_.insert(id);
}

#if !(defined __native_client__)
pp::deprecated::ScriptableObject* TestCase::CreateTestObject() {
  return NULL;
}
#endif

bool TestCase::EnsureRunningOverHTTP() {
  if (instance_->protocol() != "http:") {
    instance_->AppendError("This test needs to be run over HTTP.");
    return false;
  }

  return true;
}

bool TestCase::ShouldRunAllTests(const std::string& filter) {
  // If only the TestCase is listed, we're running all the tests in RunTests.
  return (StripTestCase(filter) == std::string());
}

bool TestCase::ShouldRunTest(const std::string& test_name,
                             const std::string& filter) {
  if (ShouldRunAllTests(filter))
    return true;

  // Lazily initialize our "filter_tests_" map.
  if (!have_populated_filter_tests_) {
    ParseTestFilter(filter, &filter_tests_);
    remaining_tests_ = filter_tests_;
    have_populated_filter_tests_ = true;
  }
  std::map<std::string, bool>::iterator iter = filter_tests_.find(test_name);
  if (iter == filter_tests_.end()) {
    // The test name wasn't listed in the filter. Don't run it, but store it
    // so TestingInstance::ExecuteTests can report an error later.
    skipped_tests_.insert(test_name);
    return false;
  }
  remaining_tests_.erase(test_name);
  return iter->second;
}

PP_TimeTicks TestCase::NowInTimeTicks() {
  return pp::Module::Get()->core()->GetTimeTicks();
}

std::string TestCase::CheckResourcesAndVars(std::string errors) {
  if (!errors.empty())
    return errors;

  if (testing_interface_) {
    // TODO(dmichael): Fix tests that leak resources and enable the following:
    /*
    uint32_t leaked_resources =
        testing_interface_->GetLiveObjectsForInstance(instance_->pp_instance());
    if (leaked_resources) {
      std::ostringstream output;
      output << "FAILED: Test leaked " << leaked_resources << " resources.\n";
      errors += output.str();
    }
    */
    const int kVarsToPrint = 100;
    PP_Var vars[kVarsToPrint];
    int found_vars = testing_interface_->GetLiveVars(vars, kVarsToPrint);
    // This will undercount if we are told to ignore a Var which is then *not*
    // leaked. Worst case, we just won't print the little "Test leaked" message,
    // but we'll still print any non-ignored leaked vars we found.
    int leaked_vars =
        found_vars - static_cast<int>(ignored_leaked_vars_.size());
    if (leaked_vars > 0) {
      std::ostringstream output;
      output << "Test leaked " << leaked_vars << " vars (printing at most "
             << kVarsToPrint <<"):<p>";
      errors += output.str();
    }
    for (int i = 0; i < std::min(found_vars, kVarsToPrint); ++i) {
      pp::Var leaked_var(pp::PASS_REF, vars[i]);
      if (ignored_leaked_vars_.count(leaked_var.pp_var().value.as_id) == 0)
        errors += leaked_var.DebugString() + "<p>";
    }
  }
  return errors;
}

// static
void TestCase::QuitMainMessageLoop(PP_Instance instance) {
  PP_Instance* heap_instance = new PP_Instance(instance);
  pp::CompletionCallback callback(&DoQuitMainMessageLoop, heap_instance);
  pp::Module::Get()->core()->CallOnMainThread(0, callback);
}

// static
void TestCase::DoQuitMainMessageLoop(void* pp_instance, int32_t result) {
  PP_Instance* instance = static_cast<PP_Instance*>(pp_instance);
  GetTestingInterface()->QuitMessageLoop(*instance);
  delete instance;
}

void TestCase::RunOnThreadInternal(
    void (*thread_func)(void*),
    void* thread_param,
    const PPB_Testing_Private* testing_interface) {
  PP_Thread thread;
  PP_CreateThread(&thread, thread_func, thread_param);
  // Run a message loop so pepper calls can be dispatched. The background
  // thread will set result_ and make us Quit when it's done.
  testing_interface->RunMessageLoop(instance_->pp_instance());
  PP_JoinThread(thread);
}
