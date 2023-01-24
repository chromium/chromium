// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Some tests for the framework itself.

#include <stddef.h>
#include <stdlib.h>

#include <windows.h>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/unguessable_token.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/target_services.h"
#include "sandbox/win/tests/common/controller.h"
#include "sandbox/win/tests/integration_tests/integration_tests_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {
struct PolicyDiagnosticsWaiter {
 public:
  PolicyDiagnosticsWaiter() {
    event.Set(::CreateEventW(nullptr, false, false, nullptr));
    policies = nullptr;
  }

  base::win::ScopedHandle event;
  std::unique_ptr<PolicyList> policies;

  std::unique_ptr<PolicyList> WaitForPolicies() {
    ::WaitForSingleObject(event.Get(), INFINITE);
    return std::move(policies);
  }
};

class TestDiagnosticsReceiver final : public PolicyDiagnosticsReceiver {
 public:
  TestDiagnosticsReceiver() {}
  ~TestDiagnosticsReceiver() override {}
  explicit TestDiagnosticsReceiver(PolicyDiagnosticsWaiter* waiter)
      : waiter_(waiter) {}
  raw_ptr<PolicyDiagnosticsWaiter> waiter_;
  void ReceiveDiagnostics(std::unique_ptr<PolicyList> policies) override {
    waiter_->policies = std::move(policies);
    ::SetEvent(waiter_->event.Get());
  }
  void OnError(ResultCode error) override {
    // Tests should not result in this function being called.
    FAIL() << "OnError should not be called";
  }
};
}  // namespace

// Returns the current process state.
SBOX_TESTS_COMMAND int IntegrationTestsTest_state(int argc, wchar_t **argv) {
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return BEFORE_INIT;

  if (!SandboxFactory::GetTargetServices()->GetState()->RevertedToSelf())
    return BEFORE_REVERT;

  return AFTER_REVERT;
}

// Returns the current process state, keeping track of it.
SBOX_TESTS_COMMAND int IntegrationTestsTest_state2(int argc, wchar_t **argv) {
  static SboxTestsState state = MIN_STATE;
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled()) {
    if (MIN_STATE == state)
      state = BEFORE_INIT;
    return state;
  }

  if (!SandboxFactory::GetTargetServices()->GetState()->RevertedToSelf()) {
    if (BEFORE_INIT == state)
      state = BEFORE_REVERT;
    return state;
  }

  if (BEFORE_REVERT == state)
    state =  AFTER_REVERT;
  return state;
}

// Blocks the process for argv[0] milliseconds simulating stuck child.
SBOX_TESTS_COMMAND int IntegrationTestsTest_stuck(int argc, wchar_t **argv) {
  int timeout = 500;
  if (argc > 0) {
    timeout = _wtoi(argv[0]);
  }

  ::Sleep(timeout);
  return 1;
}

// Returns the number of arguments
SBOX_TESTS_COMMAND int IntegrationTestsTest_args(int argc, wchar_t **argv) {
  for (int i = 0; i < argc; i++) {
    wchar_t argument[20];
    size_t argument_bytes = wcslen(argv[i]) * sizeof(wchar_t);
    memcpy(argument, argv[i], __min(sizeof(argument), argument_bytes));
  }

  return argc;
}

// Sets the first inherited event, then waits on the second. This ensures
// this process is alive and remains alive while its parent tests diagnostics.
SBOX_TESTS_COMMAND int IntegrationTestsTest_event(int argc, wchar_t** argv) {
  if (argc < 2)
    return SBOX_TEST_INVALID_PARAMETER;

  base::win::ScopedHandle handle_started(
      reinterpret_cast<HANDLE>(wcstoul(argv[0], nullptr, 16)));
  if (!handle_started.IsValid())
    return SBOX_TEST_NOT_FOUND;

  base::win::ScopedHandle handle_done(
      reinterpret_cast<HANDLE>(wcstoul(argv[1], nullptr, 16)));
  if (!handle_done.IsValid())
    return SBOX_TEST_NOT_FOUND;

  if (!::SetEvent(handle_started.Get()))
    return SBOX_TEST_FIRST_ERROR;

  if (WAIT_OBJECT_0 != ::WaitForSingleObject(handle_done.Get(), 1000))
    return SBOX_TEST_SECOND_ERROR;

  return SBOX_TEST_SUCCEEDED;
}

// Creates a job and tries to run a process inside it. The function can be
// called with up to two parameters. The first one if set to "none" means that
// the child process should be run with the JobLevel::kNone JobLevel else it is
// run with JobLevel::kLockdown level. The second if present specifies that the
// JOB_OBJECT_LIMIT_BREAKAWAY_OK flag should be set on the job object created
// in this function. The return value is either SBOX_TEST_SUCCEEDED if the test
// has passed or a value between 0 and 4 indicating which part of the test has
// failed.
SBOX_TESTS_COMMAND int IntegrationTestsTest_job(int argc, wchar_t **argv) {
  HANDLE job = ::CreateJobObject(NULL, NULL);
  if (!job)
    return 0;

  JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_limits;
  if (!::QueryInformationJobObject(job, JobObjectExtendedLimitInformation,
                                   &job_limits, sizeof(job_limits), NULL)) {
    return 1;
  }
  // We cheat here and assume no 2-nd parameter means no breakaway flag and any
  // value for the second param means with breakaway flag.
  if (argc > 1) {
    job_limits.BasicLimitInformation.LimitFlags |=
        JOB_OBJECT_LIMIT_BREAKAWAY_OK;
  } else {
    job_limits.BasicLimitInformation.LimitFlags &=
        ~JOB_OBJECT_LIMIT_BREAKAWAY_OK;
  }
  if (!::SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                                &job_limits, sizeof(job_limits))) {
    return 2;
  }
  if (!::AssignProcessToJobObject(job, ::GetCurrentProcess()))
    return 3;

  JobLevel job_level = JobLevel::kLockdown;
  if (argc > 0 && wcscmp(argv[0], L"none") == 0)
    job_level = JobLevel::kNone;

  TestRunner runner(job_level, USER_RESTRICTED_SAME_ACCESS, USER_LOCKDOWN);
  runner.SetTimeout(TestTimeouts::action_timeout());

  if (1 != runner.RunTest(L"IntegrationTestsTest_args 1"))
    return 4;

  // Terminate the job now.
  ::TerminateJobObject(job, SBOX_TEST_SUCCEEDED);
  // We should not make it to here but it doesn't mean our test failed.
  return SBOX_TEST_SUCCEEDED;
}

TEST(IntegrationTestsTest, CallsBeforeInit) {
  TestRunner runner;
  runner.SetTimeout(TestTimeouts::action_timeout());
  runner.SetTestState(BEFORE_INIT);
  ASSERT_EQ(BEFORE_INIT, runner.RunTest(L"IntegrationTestsTest_state"));
}

TEST(IntegrationTestsTest, CallsBeforeRevert) {
  TestRunner runner;
  runner.SetTimeout(TestTimeouts::action_timeout());
  runner.SetTestState(BEFORE_REVERT);
  ASSERT_EQ(BEFORE_REVERT, runner.RunTest(L"IntegrationTestsTest_state"));
}

TEST(IntegrationTestsTest, CallsAfterRevert) {
  TestRunner runner;
  runner.SetTimeout(TestTimeouts::action_timeout());
  runner.SetTestState(AFTER_REVERT);
  ASSERT_EQ(AFTER_REVERT, runner.RunTest(L"IntegrationTestsTest_state"));
}

TEST(IntegrationTestsTest, CallsEveryState) {
  TestRunner runner;
  runner.SetTimeout(TestTimeouts::action_timeout());
  runner.SetTestState(EVERY_STATE);
  ASSERT_EQ(AFTER_REVERT, runner.RunTest(L"IntegrationTestsTest_state2"));
}

TEST(IntegrationTestsTest, ForwardsArguments) {
  TestRunner runner;
  runner.SetTimeout(TestTimeouts::action_timeout());
  runner.SetTestState(BEFORE_INIT);
  ASSERT_EQ(1, runner.RunTest(L"IntegrationTestsTest_args first"));

  TestRunner runner2;
  runner2.SetTimeout(TestTimeouts::action_timeout());
  runner2.SetTestState(BEFORE_INIT);
  ASSERT_EQ(4, runner2.RunTest(L"IntegrationTestsTest_args first second third "
                               L"fourth"));
}

TEST(IntegrationTestsTest, WaitForStuckChild) {
  TestRunner runner;
  runner.SetTimeout(TestTimeouts::action_timeout());
  runner.SetAsynchronous(true);
  runner.SetKillOnDestruction(false);
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"IntegrationTestsTest_stuck 100"));
  ASSERT_EQ(SBOX_ALL_OK, runner.broker()->WaitForAllTargets());
}

TEST(IntegrationTestsTest, NoWaitForStuckChildNoJob) {
  TestRunner runner(JobLevel::kNone, USER_RESTRICTED_SAME_ACCESS,
                    USER_LOCKDOWN);
  runner.SetTimeout(TestTimeouts::action_timeout());
  runner.SetAsynchronous(true);
  runner.SetKillOnDestruction(false);
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"IntegrationTestsTest_stuck 2000"));
  ASSERT_EQ(SBOX_ALL_OK, runner.broker()->WaitForAllTargets());
  // In this case the processes are not tracked by the broker and should be
  // still active.
  DWORD exit_code;
  ASSERT_TRUE(::GetExitCodeProcess(runner.process(), &exit_code));
  ASSERT_EQ(STILL_ACTIVE, exit_code);
  // Terminate the test process now.
  ::TerminateProcess(runner.process(), 0);
}

TEST(IntegrationTestsTest, TwoStuckChildrenSecondOneHasNoJob) {
  TestRunner runner;
  runner.SetTimeout(TestTimeouts::action_timeout());
  runner.SetAsynchronous(true);
  runner.SetKillOnDestruction(false);
  TestRunner runner2(JobLevel::kNone, USER_RESTRICTED_SAME_ACCESS,
                     USER_LOCKDOWN);
  runner2.SetTimeout(TestTimeouts::action_timeout());
  runner2.SetAsynchronous(true);
  runner2.SetKillOnDestruction(false);
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"IntegrationTestsTest_stuck 100"));
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner2.RunTest(L"IntegrationTestsTest_stuck 2000"));
  // Actually both runners share the same singleton broker.
  ASSERT_EQ(SBOX_ALL_OK, runner.broker()->WaitForAllTargets());
  // In this case the processes are not tracked by the broker and should be
  // still active.
  DWORD exit_code;
  // Checking the exit code for |runner| is flaky on the slow bots but at
  // least we know that the wait above has succeeded if we are here.
  ASSERT_TRUE(::GetExitCodeProcess(runner2.process(), &exit_code));
  ASSERT_EQ(STILL_ACTIVE, exit_code);
  // Terminate the test process now.
  ::TerminateProcess(runner2.process(), 0);
}

TEST(IntegrationTestsTest, TwoStuckChildrenFirstOneHasNoJob) {
  TestRunner runner;
  runner.SetTimeout(TestTimeouts::action_timeout());
  runner.SetAsynchronous(true);
  runner.SetKillOnDestruction(false);
  TestRunner runner2(JobLevel::kNone, USER_RESTRICTED_SAME_ACCESS,
                     USER_LOCKDOWN);
  runner2.SetTimeout(TestTimeouts::action_timeout());
  runner2.SetAsynchronous(true);
  runner2.SetKillOnDestruction(false);
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner2.RunTest(L"IntegrationTestsTest_stuck 2000"));
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"IntegrationTestsTest_stuck 100"));
  // Actually both runners share the same singleton broker.
  ASSERT_EQ(SBOX_ALL_OK, runner.broker()->WaitForAllTargets());
  // In this case the processes are not tracked by the broker and should be
  // still active.
  DWORD exit_code;
  // Checking the exit code for |runner| is flaky on the slow bots but at
  // least we know that the wait above has succeeded if we are here.
  ASSERT_TRUE(::GetExitCodeProcess(runner2.process(), &exit_code));
  ASSERT_EQ(STILL_ACTIVE, exit_code);
  // Terminate the test process now.
  ::TerminateProcess(runner2.process(), 0);
}

std::unique_ptr<TestRunner> StuckChildrenRunner() {
  auto runner = std::make_unique<TestRunner>(
      JobLevel::kNone, USER_RESTRICTED_SAME_ACCESS, USER_LOCKDOWN);
  runner->SetTimeout(TestTimeouts::action_timeout());
  runner->SetAsynchronous(true);
  runner->SetKillOnDestruction(false);
  return runner;
}

TEST(IntegrationTestsTest, MultipleStuckChildrenSequential) {
  auto runner = StuckChildrenRunner();
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner->RunTest(L"IntegrationTestsTest_stuck 100"));
  // All runners share the same singleton broker.
  auto* broker = runner->broker();
  ASSERT_EQ(SBOX_ALL_OK, broker->WaitForAllTargets());

  runner = StuckChildrenRunner();
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner->RunTest(L"IntegrationTestsTest_stuck 2000"));
  ASSERT_EQ(SBOX_ALL_OK, broker->WaitForAllTargets());

  DWORD exit_code;
  // Checking the exit code for |runner| is flaky on the slow bots but at
  // least we know that the wait above has succeeded if we are here.
  ASSERT_TRUE(::GetExitCodeProcess(runner->process(), &exit_code));
  ASSERT_EQ(STILL_ACTIVE, exit_code);
  // Terminate the test process now.
  ::TerminateProcess(runner->process(), 0);

  runner = StuckChildrenRunner();
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner->RunTest(L"IntegrationTestsTest_stuck 100"));
  ASSERT_EQ(SBOX_ALL_OK, broker->WaitForAllTargets());
}

// Running from inside job that allows us to escape from it should be ok.
TEST(IntegrationTestsTest, RunChildFromInsideJob) {
  TestRunner runner;
  runner.SetUnsandboxed(true);
  runner.SetTimeout(TestTimeouts::action_timeout());
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"IntegrationTestsTest_job with_job escape_flag"));
}

// Running from inside job that doesn't allow us to escape from it should fail
// on any windows prior to 8.
TEST(IntegrationTestsTest, RunChildFromInsideJobNoEscape) {
  TestRunner runner;
  runner.SetUnsandboxed(true);
  runner.SetTimeout(TestTimeouts::action_timeout());
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"IntegrationTestsTest_job with_job"));
}

// Running without a job object should be ok regardless of the fact that we are
// running inside an outer job.
TEST(IntegrationTestsTest, RunJoblessChildFromInsideJob) {
  TestRunner runner;
  runner.SetUnsandboxed(true);
  runner.SetTimeout(TestTimeouts::action_timeout());
  ASSERT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"IntegrationTestsTest_job none"));
}

// GetPolicyDiagnostics validation
TEST(IntegrationTestsTest, GetPolicyDiagnosticsReflectsActiveChildren) {
  TestRunner runner;

  runner.SetAsynchronous(true);

  // This helper can be reused if it has finished waiting.
  auto waiter = std::make_unique<PolicyDiagnosticsWaiter>();
  {
    // Receiver cannot be reused as it is consumed by GetPolicyDiagnostics().
    auto receiver = std::make_unique<TestDiagnosticsReceiver>(waiter.get());
    auto result = runner.broker()->GetPolicyDiagnostics(std::move(receiver));
    ASSERT_EQ(SBOX_ALL_OK, result);

    // Initially no children so no policies.
    auto policies = waiter->WaitForPolicies();
    ASSERT_EQ(policies->size(), 0U);
  }

  base::win::ScopedHandle handle_started(
      CreateEventW(nullptr, true, false, nullptr));
  base::win::ScopedHandle handle_done(
      CreateEventW(nullptr, true, false, nullptr));

  runner.GetPolicy()->AddHandleToShare(handle_started.Get());
  runner.GetPolicy()->AddHandleToShare(handle_done.Get());
  auto cmd_line = base::StringPrintf(L"IntegrationTestsTest_event %p %p",
                                     handle_started.Get(), handle_done.Get());

  ASSERT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(cmd_line.c_str()));
  ASSERT_EQ(WAIT_OBJECT_0,
            ::WaitForSingleObject(handle_started.Get(),
                                  sandbox::SboxTestEventTimeout()));

  {
    // After starting a process, there should be one policy.
    auto receiver = std::make_unique<TestDiagnosticsReceiver>(waiter.get());
    ASSERT_EQ(SBOX_ALL_OK,
              runner.broker()->GetPolicyDiagnostics(std::move(receiver)));
    auto policies = waiter->WaitForPolicies();
    ASSERT_EQ(policies->size(), 1U);
  }

  SetEvent(handle_done.Get());
  ASSERT_EQ(SBOX_ALL_OK, runner.broker()->WaitForAllTargets());

  // TODO(ajgo) WaitForAllTargets is satisfied when the final process
  // in a job exits but before the final job notification is received
  // by the tracking thread. We have to give that notification a chance
  // before we test to see if the job itself is removed.
  SleepEx(TestTimeouts::tiny_timeout().InMilliseconds(), true);
  {
    // Finally there should be no processes and no policies.
    auto receiver = std::make_unique<TestDiagnosticsReceiver>(waiter.get());
    ASSERT_EQ(SBOX_ALL_OK,
              runner.broker()->GetPolicyDiagnostics(std::move(receiver)));
    auto policies = waiter->WaitForPolicies();
    ASSERT_EQ(policies->size(), 0U);
  }
}

}  // namespace sandbox
