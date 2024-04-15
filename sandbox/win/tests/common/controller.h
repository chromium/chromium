// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_TESTS_COMMON_CONTROLLER_H_
#define SANDBOX_WIN_TESTS_COMMON_CONTROLLER_H_

#include <windows.h>

#include <string>

#include "base/dcheck_is_on.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
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
  // First Result. (0x25B10000 or 632356864)
  SBOX_TEST_FIRST_RESULT = CUSTOMER_CODE | SBOX_TESTS_FACILITY,
  // Second result. (0x25B10001 or 632356865)
  SBOX_TEST_SUCCEEDED,
  // Ping OK. (0x25B10002 or 632356866)
  SBOX_TEST_PING_OK,
  // First info. (0x65B10000 or 1706098688)
  SBOX_TEST_FIRST_INFO = SBOX_TEST_FIRST_RESULT | SEVERITY_INFO_FLAGS,
  // Access was denied. (0x65B10001 or 1706098689)
  SBOX_TEST_DENIED,
  // The resource was not found. (0x65B10002 or 1706098690)
  SBOX_TEST_NOT_FOUND,
  // First error. (0xE5B10000 or -441384960)
  SBOX_TEST_FIRST_ERROR = SBOX_TEST_FIRST_RESULT | SEVERITY_ERROR_FLAGS,
  // Second error. (0xE5B10001 or -441384959)
  SBOX_TEST_SECOND_ERROR,
  // Third error. (0xE5B10002 or -441384958)
  SBOX_TEST_THIRD_ERROR,
  // Fourth error. (0xE5B10003 or -441384957)
  SBOX_TEST_FOURTH_ERROR,
  // Fifth error. (0xE5B10004 or -441384956)
  SBOX_TEST_FIFTH_ERROR,
  // Sixth error. (0xE5B10005 or -441384955)
  SBOX_TEST_SIXTH_ERROR,
  // Seventh error. (0xE5B10006 or -441384954)
  SBOX_TEST_SEVENTH_ERROR,
  // Invalid Parameter. (0xE5B10007 or -441384953)
  SBOX_TEST_INVALID_PARAMETER,
  // Failed to run test. (0xE5B10008 or -441384952)
  SBOX_TEST_FAILED_TO_RUN_TEST,
  // Failed to execute command. (0xE5B10009 or -441384951)
  SBOX_TEST_FAILED_TO_EXECUTE_COMMAND,
  // Test timed out. (0xE5B1000A or -441384950)
  SBOX_TEST_TIMED_OUT,
  // Test failed. (0xE5B1000B or -441384949)
  SBOX_TEST_FAILED,
  // Failed to configure sandbox before test. (0xE5B1000C or -441384948)
  SBOX_TEST_FAILED_SETUP,
  // Last Result. (0xE5B1000D or -441384947)
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

  // Adds a filesystem rules with the path of a file in system32. The function
  // appends "pattern" to "system32" and then call AddRule. Return true if the
  // function succeeds.
  bool AddRuleSys32(FileSemantics semantics, const wchar_t* pattern);

  // Adds a filesystem rules to the policy. Returns true if the functions
  // succeeds.
  bool AllowFileAccess(FileSemantics semantics, const wchar_t* pattern);

  // Starts a child process in the sandbox and ask it to run |command|. Returns
  // a SboxTestResult. By default, the test runs AFTER_REVERT.
  int RunTest(const wchar_t* command);

  // Sets the timeout value for the child to run the command and return.
  void SetTimeout(DWORD timeout_ms);
  void SetTimeout(base::TimeDelta timeout);

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

  // Returns the pointers to the policy object. It can be used to modify
  // the policy manually.
  TargetPolicy* GetPolicy();

  BrokerServices* broker() { return broker_; }

  // Returns the process handle for an asynchronous test.
  HANDLE process() { return target_process_.get(); }

  // Returns the process ID for an asynchronous test.
  DWORD process_id() { return target_process_id_; }

  // Blocks until the number of tracked processes returns to zero.
  bool WaitForAllTargets();

 private:

  // The actual runner.
  int InternalRunTest(const wchar_t* command);
  DWORD timeout_ms();

  raw_ptr<BrokerServices> broker_;
  std::unique_ptr<TargetPolicy> policy_;
  base::TimeDelta timeout_;
  SboxTestsState state_;
  bool is_init_;
  bool is_async_;
  bool no_sandbox_;
  bool disable_csrss_;
  bool kill_on_destruction_;
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
