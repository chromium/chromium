// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_TESTS_COMMON_CONTROLLER_H_
#define SANDBOX_WIN_TESTS_COMMON_CONTROLLER_H_

#include <windows.h>
#include <string>

#include "base/win/scoped_handle.h"
#include "sandbox/win/src/sandbox.h"

namespace sandbox {

// See winerror.h for details.
#define SEVERITY_INFO_FLAGS   0x40000000
#define SEVERITY_ERROR_FLAGS  0xC0000000
#define CUSTOMER_CODE         0x20000000
#define SBOX_TESTS_FACILITY   0x05B10000

// All the possible error codes returned by the child process in
// the sandbox.
enum SboxTestResult {
  SBOX_TEST_FIRST_RESULT = CUSTOMER_CODE | SBOX_TESTS_FACILITY,
  SBOX_TEST_SUCCEEDED,
  SBOX_TEST_PING_OK,
  SBOX_TEST_FIRST_INFO = SBOX_TEST_FIRST_RESULT | SEVERITY_INFO_FLAGS,
  SBOX_TEST_DENIED,     // Access was denied.
  SBOX_TEST_NOT_FOUND,  // The resource was not found.
  SBOX_TEST_FIRST_ERROR = SBOX_TEST_FIRST_RESULT | SEVERITY_ERROR_FLAGS,
  SBOX_TEST_SECOND_ERROR,
  SBOX_TEST_THIRD_ERROR,
  SBOX_TEST_FOURTH_ERROR,
  SBOX_TEST_FIFTH_ERROR,
  SBOX_TEST_SIXTH_ERROR,
  SBOX_TEST_SEVENTH_ERROR,
  SBOX_TEST_INVALID_PARAMETER,
  SBOX_TEST_FAILED_TO_RUN_TEST,
  SBOX_TEST_FAILED_TO_EXECUTE_COMMAND,
  SBOX_TEST_TIMED_OUT,
  SBOX_TEST_FAILED,
  SBOX_TEST_LAST_RESULT
};

inline bool IsSboxTestsResult(SboxTestResult result) {
  unsigned int code = static_cast<unsigned int>(result);
  unsigned int first = static_cast<unsigned int>(SBOX_TEST_FIRST_RESULT);
  unsigned int last = static_cast<unsigned int>(SBOX_TEST_LAST_RESULT);
  return (code > first) && (code < last);
}

enum SboxTestsState {
  MIN_STATE = 1,
  BEFORE_INIT,
  BEFORE_REVERT,
  AFTER_REVERT,
  EVERY_STATE,
  MAX_STATE
};

#define SBOX_TESTS_API __declspec(dllexport)
#define SBOX_TESTS_COMMAND extern "C" SBOX_TESTS_API

extern "C" {
typedef int (*CommandFunction)(int argc, wchar_t **argv);
}

// Class to facilitate the launch of a test inside the sandbox.
class TestRunner {
 public:
  TestRunner(JobLevel job_level, TokenLevel startup_token,
             TokenLevel main_token);

  TestRunner();

  ~TestRunner();

  // Adds a rule to the policy. The parameters are the same as the AddRule
  // function in the sandbox.
  bool AddRule(TargetPolicy::SubSystem subsystem,
               TargetPolicy::Semantics semantics,
               const wchar_t* pattern);

  // Adds a filesystem rules with the path of a file in system32. The function
  // appends "pattern" to "system32" and then call AddRule. Return true if the
  // function succeeds.
  bool AddRuleSys32(TargetPolicy::Semantics semantics, const wchar_t* pattern);

  // Adds a filesystem rules to the policy. Returns true if the functions
  // succeeds.
  bool AddFsRule(TargetPolicy::Semantics semantics, const wchar_t* pattern);

  // Starts a child process in the sandbox and ask it to run |command|. Returns
  // a SboxTestResult. By default, the test runs AFTER_REVERT.
  int RunTest(const wchar_t* command);

  // Sets the timeout value for the child to run the command and return.
  void SetTimeout(DWORD timeout_ms);

  // Sets TestRunner to return without waiting for the process to exit.
  void SetAsynchronous(bool is_async) { is_async_ = is_async; }

  // Sets whether TestRunner sandboxes the child process. ("--no-sandbox")
  void SetUnsandboxed(bool is_no_sandbox) { no_sandbox_ = is_no_sandbox; }

  // Sets whether TestRunner should disable CSRSS or not (default true).
  // Any test that needs to spawn a child process needs to set this to false.
  void SetDisableCsrss(bool disable_csrss) { disable_csrss_ = disable_csrss; }

  // Sets the desired state for the test to run.
  void SetTestState(SboxTestsState desired_state);

  // Sets a flag whether the process should be killed when the TestRunner is
  // destroyed.
  void SetKillOnDestruction(bool value) { kill_on_destruction_ = value; }

  // Sets whether the TargetPolicy should be released after the child process
  // is launched while the test is running.
  void SetReleasePolicyInRun(bool value) { release_policy_in_run_ = value; }

  // Returns the pointers to the policy object. It can be used to modify
  // the policy manually.
  TargetPolicy* GetPolicy();

  BrokerServices* broker() { return broker_; }

  // Returns the process handle for an asynchronous test.
  HANDLE process() { return target_process_.Get(); }

  // Returns the process ID for an asynchronous test.
  DWORD process_id() { return target_process_id_; }

 private:

  // The actual runner.
  int InternalRunTest(const wchar_t* command);

  BrokerServices* broker_;
  scoped_refptr<TargetPolicy> policy_;
  DWORD timeout_;
  SboxTestsState state_;
  bool is_init_;
  bool is_async_;
  bool no_sandbox_;
  bool disable_csrss_;
  bool kill_on_destruction_;
  bool release_policy_in_run_ = false;
  base::win::ScopedHandle target_process_;
  DWORD target_process_id_;
};

// Returns the broker services.
BrokerServices* GetBroker();

// Constructs a full path to a file inside the system32 folder.
std::wstring MakePathToSys32(const wchar_t* name, bool is_obj_man_path);

// Constructs a full path to a file inside the syswow64 folder.
std::wstring MakePathToSysWow64(const wchar_t* name, bool is_obj_man_path);

// Constructs a full path to a file inside the system32 (or syswow64) folder
// depending on whether process is running in wow64 or not.
std::wstring MakePathToSys(const wchar_t* name, bool is_obj_man_path);

// Runs the given test on the target process.
int DispatchCall(int argc, wchar_t **argv);

}  // namespace sandbox

#endif  // SANDBOX_WIN_TESTS_COMMON_CONTROLLER_H_
