// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_TESTS_COMMON_CONTROLLER_H_
#define SANDBOX_WIN_TESTS_COMMON_CONTROLLER_H_

#include <concepts>
#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "base/win/scoped_process_information.h"
#include "base/win/windows_types.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/win_utils.h"

namespace sandbox {

namespace internal {

// base::ToString can't handle passing std::wstring so implement our own wrapper
// to handle this special case.
template <typename T>
std::string ToString(const T& value) {
  return base::ToString(value);
}

template <>
std::string ToString(const std::wstring& value);

template <typename T>
concept TestNameDefinition = requires {
  { T::kTestName } -> std::convertible_to<std::string_view>;
};

}  // namespace internal

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

#define SBOX_TEST_DECLARE_COMMAND(name)                         \
  struct name##Def {                                            \
    static constexpr std::string_view kTestName = #name "Impl"; \
  };                                                            \
  using name##TestRunner = GenericTestRunner<name##Def>

#define SBOX_TEST_DEFINE_COMMAND(name) \
  SBOX_TESTS_COMMAND int name##Impl(base::span<const std::wstring> args)

// Declare a command runner type and its implementation.
#define SBOX_TEST_COMMAND(name)    \
  SBOX_TEST_DECLARE_COMMAND(name); \
  SBOX_TEST_DEFINE_COMMAND(name)

extern "C" {
typedef int (*CommandFunction)(int argc, const wchar_t** argv);
}

typedef int (*CommandFunctionArgs)(base::span<const std::wstring>);

// Class to facilitate the launch of a test inside the sandbox.
class TestRunnerBase {
 public:
  TestRunnerBase(const TestRunnerBase&) = delete;
  TestRunnerBase& operator=(const TestRunnerBase&) = delete;
  virtual ~TestRunnerBase();

  // Adds a filesystem rules with the path of a file in system32. The function
  // appends "pattern" to "system32" and then call AddRule. Return true if the
  // function succeeds.
  bool AddRuleSys32(FileSemantics semantics, std::wstring_view pattern);

  // Adds a filesystem rules to the policy. Returns true if the functions
  // succeeds.
  bool AllowFileAccess(FileSemantics semantics, std::wstring_view pattern);

  // Sets the timeout value for the child to run the command and return.
  void SetTimeout(DWORD timeout_ms);
  void SetTimeout(base::TimeDelta timeout);

  // Sets whether TestRunner sandboxes the child process. ("--no-sandbox")
  void SetUnsandboxed(bool is_no_sandbox) { no_sandbox_ = is_no_sandbox; }

  // Sets whether TestRunner should disable CSRSS or not (default true).
  // Any test that needs to spawn a child process needs to set this to false.
  void SetDisableCsrss(bool disable_csrss) { disable_csrss_ = disable_csrss; }

  // Sets the desired state for the test to run.
  void SetTestState(SboxTestsState desired_state) { state_ = desired_state; }

  // Returns the pointer to the policy object. It can be used to modify
  // the policy manually.
  TargetPolicy* GetPolicy();

  // Returns the pointer to the config object. It can be used to modify
  // the config manually.
  TargetConfig* GetConfig();

  BrokerServices* broker() { return broker_; }

  // Blocks until the number of tracked processes returns to zero.
  bool WaitForAllTargets();

 protected:
  static base::CommandLine CreateCommandLine(std::string_view command,
                                             base::span<const std::string> args,
                                             SboxTestsState state,
                                             bool no_sandbox,
                                             bool legacy_command);

  TestRunnerBase(JobLevel job_level,
                 TokenLevel startup_token,
                 TokenLevel main_token);

  base::Process CreateTestProcess(std::string_view command,
                                  base::span<const std::string> args,
                                  bool legacy_command = false);

  int WaitForResult(const base::Process& process) const;

 private:
  base::Process LaunchSandboxProcess(const base::CommandLine& cmd_line);

  raw_ptr<BrokerServices> broker_;
  std::unique_ptr<TargetPolicy> policy_;
  base::TimeDelta timeout_;
  SboxTestsState state_ = AFTER_REVERT;
  bool no_sandbox_ = false;
  bool disable_csrss_ = true;
};

template <internal::TestNameDefinition Test>
class GenericTestRunner final : public TestRunnerBase {
 public:
  using type = Test;

  GenericTestRunner()
      : TestRunnerBase(JobLevel::kLockdown,
                       USER_RESTRICTED_SAME_ACCESS,
                       USER_LOCKDOWN) {}

  GenericTestRunner(JobLevel job_level,
                    TokenLevel startup_token,
                    TokenLevel main_token)
      : TestRunnerBase(job_level, startup_token, main_token) {}

  static base::CommandLine CreateCommandLineForTesting() {
    return CreateCommandLine(Test::kTestName, {}, BEFORE_INIT,
                             /*no_sandbox=*/false,
                             /*legacy_command=*/false);
  }

  // Starts a child process in the sandbox and ask it to run the callback
  // command with optional arguments asynchronously. Return a running process
  // object.
  template <typename... Args>
  base::Process RunTestAsync(Args&&... args) {
    std::vector<std::string> args_vector = {internal::ToString(args)...};
    return CreateTestProcess(Test::kTestName, args_vector);
  }

  // Starts a child process in the sandbox and ask it to run the callback
  // command with optional arguments. Return a SboxTestResult.
  template <typename... Args>
  int RunTest(Args&&... args) {
    base::Process process = RunTestAsync(std::forward<Args>(args)...);
    if (!process.IsValid()) {
      return SBOX_TEST_FAILED_TO_RUN_TEST;
    }
    return WaitForResult(process);
  }
};

// TODO(forshaw): This is to support old code which passes and entire command
// line. Remove once the new API is implemented and all the old tests have been
// updated.
class TestRunner final : public TestRunnerBase {
 public:
  TestRunner()
      : TestRunnerBase(JobLevel::kLockdown,
                       USER_RESTRICTED_SAME_ACCESS,
                       USER_LOCKDOWN) {}
  TestRunner(JobLevel job_level,
             TokenLevel startup_token,
             TokenLevel main_token)
      : TestRunnerBase(job_level, startup_token, main_token) {}
  ~TestRunner() override;

  // Sets TestRunner to return without waiting for the process to exit.
  void SetAsynchronous(bool is_async) { is_async_ = is_async; }

  // Sets a flag whether the process should be killed when the TestRunner is
  // destroyed.
  void SetKillOnDestruction(bool value) { kill_on_destruction_ = value; }

  // Returns the process handle for an asynchronous test.
  base::ProcessHandle process() { return target_process_.Handle(); }

  // Returns the process ID for an asynchronous test.
  base::ProcessId process_id() { return target_process_.Pid(); }

  // Starts a child process in the sandbox and ask it to run `command`.
  // Return a SboxTestResult.
  int RunTest(std::wstring_view command);

 private:
  bool is_async_ = false;
  bool kill_on_destruction_ = true;
  base::Process target_process_;
};

// Declare built-in test commands.
SBOX_TEST_DECLARE_COMMAND(WaitCommand);
SBOX_TEST_DECLARE_COMMAND(PingCommand);

// Returns the broker services.
BrokerServices* GetBroker();

// Constructs a full path to a file inside the system32 (or syswow64) folder
// depending on whether process is running in wow64 or not.
std::wstring MakePathToSys(std::wstring_view name, bool is_obj_man_path);

// Check if this is a child process for a test.
bool IsChildProcessForTesting();

// Runs the given test on the target process.
int DispatchCall();

}  // namespace sandbox

#endif  // SANDBOX_WIN_TESTS_COMMON_CONTROLLER_H_
