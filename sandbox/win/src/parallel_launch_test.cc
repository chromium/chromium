// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <memory>
#include <string>

#include "base/process/process_info.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_process_information.h"
#include "sandbox/win/src/broker_services.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

// BrokerServicesDelegate common implementation.
class TestBrokerServicesDelegateBase : public BrokerServicesDelegate {
 public:
  bool ParallelLaunchEnabled() override { return true; }

  void ParallelLaunchPostTaskAndReplyWithResult(
      const base::Location& from_here,
      base::OnceCallback<CreateTargetResult()> task,
      base::OnceCallback<void(CreateTargetResult)> reply) override {
    base::ThreadPool::PostTaskAndReplyWithResult(
        from_here,
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
        std::move(task), std::move(reply));
  }

  void AfterTargetProcessCreateOnCreationThread(const void* trace_id,
                                                DWORD process_id) override {}
};

class ParallelLaunchTest : public testing::Test {
 public:
  void SetUp() override {
    task_environment_ = std::make_unique<base::test::TaskEnvironment>();
  }

  static void FinishSpawnTargetAsync(CreateTargetResult* spawn_result,
                                     base::RunLoop* run_loop,
                                     int* launches_remaining_count,
                                     base::win::ScopedProcessInformation target,
                                     DWORD last_error,
                                     ResultCode result) {
    spawn_result->process_info = std::move(target);
    spawn_result->last_error = last_error;
    spawn_result->result_code = result;

    if (--*launches_remaining_count == 0) {
      run_loop->Quit();
    }
  }

 private:
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
};

class SingleLaunch_TestBrokerServicesDelegate
    : public TestBrokerServicesDelegateBase {
 public:
  void BeforeTargetProcessCreateOnCreationThread(
      const void* trace_id) override {
    creation_thread_id_ = ::GetCurrentThreadId();
  }

  DWORD creation_thread_id_ = 0;
};

// Launches a single child with parallel launching enabled. The child process
// will be created on the thread pool.
TEST_F(ParallelLaunchTest, SingleLaunch) {
  BrokerServices* broker = GetBroker();
  ASSERT_TRUE(broker);

  auto* delegate = new SingleLaunch_TestBrokerServicesDelegate();
  static_cast<BrokerServicesBase*>(broker)->SetBrokerServicesDelegateForTesting(
      std::unique_ptr<BrokerServicesDelegate>(delegate));

  // Get the path to the sandboxed app.
  wchar_t prog_name[MAX_PATH];
  GetModuleFileNameW(nullptr, prog_name, MAX_PATH);

  std::wstring arguments(L"\"");
  arguments += prog_name;
  arguments += L"\" -child 0 wait";  // Don't care about the "state" argument.

  auto policy = broker->CreatePolicy();
  EXPECT_EQ(SBOX_ALL_OK, policy->GetConfig()->SetTokenLevel(USER_INTERACTIVE,
                                                            USER_LOCKDOWN));

  CreateTargetResult spawn_result;
  spawn_result.result_code = SBOX_ERROR_GENERIC;

  base::RunLoop run_loop;
  int launches_remaining_count = 1;
  broker->SpawnTargetAsync(
      prog_name, arguments.c_str(), std::move(policy),
      base::BindOnce(&FinishSpawnTargetAsync, base::Unretained(&spawn_result),
                     base::Unretained(&run_loop),
                     base::Unretained(&launches_remaining_count)));
  run_loop.Run();

  // Target creation should happen on a different thread.
  EXPECT_NE(delegate->creation_thread_id_, 0u);
  EXPECT_NE(delegate->creation_thread_id_, GetCurrentThreadId());

  EXPECT_EQ(SBOX_ALL_OK, spawn_result.result_code);

  EXPECT_EQ(1u, ::ResumeThread(spawn_result.process_info.thread_handle()));

  EXPECT_EQ(
      static_cast<DWORD>(WAIT_TIMEOUT),
      ::WaitForSingleObject(spawn_result.process_info.process_handle(), 2000));

  EXPECT_TRUE(
      ::TerminateProcess(spawn_result.process_info.process_handle(), 0));

  ::WaitForSingleObject(spawn_result.process_info.process_handle(), INFINITE);
}

class ParallelLaunch_TestBrokerServicesDelegate
    : public TestBrokerServicesDelegateBase {
 public:
  void BeforeTargetProcessCreateOnCreationThread(
      const void* trace_id) override {
    if (first_launch_) {
      first_creation_thread_id_ = ::GetCurrentThreadId();
      first_trace_id_ = reinterpret_cast<uintptr_t>(trace_id);
      first_launch_ = false;
      EXPECT_TRUE(::SetEvent(reached_first_creation_event_));
      ::WaitForSingleObject(first_block_event_, INFINITE);
    } else {
      second_creation_thread_id_ = ::GetCurrentThreadId();
      second_trace_id_ = reinterpret_cast<uintptr_t>(trace_id);
      EXPECT_TRUE(::SetEvent(first_block_event_));
    }
  }

  bool first_launch_ = true;
  HANDLE reached_first_creation_event_;
  HANDLE first_block_event_;
  DWORD first_creation_thread_id_;
  DWORD second_creation_thread_id_;
  uintptr_t first_trace_id_;
  uintptr_t second_trace_id_;
};

// This test launches two processes and synchronizes the target creation threads
// to run at the same time.
TEST_F(ParallelLaunchTest, ParallelLaunch) {
  BrokerServices* broker = GetBroker();
  ASSERT_TRUE(broker);

  auto* delegate = new ParallelLaunch_TestBrokerServicesDelegate();
  static_cast<BrokerServicesBase*>(broker)->SetBrokerServicesDelegateForTesting(
      std::unique_ptr<BrokerServicesDelegate>(delegate));

  // Get the path to the sandboxed app.
  wchar_t prog_name[MAX_PATH];
  GetModuleFileNameW(nullptr, prog_name, MAX_PATH);

  std::wstring arguments(L"\"");
  arguments += prog_name;
  arguments += L"\" -child 0 wait";  // Don't care about the "state" argument.

  base::RunLoop run_loop;
  int launches_remaining_count = 2;

  // Launch the first process. This will block on the creation thread and wait
  // for the second process launch to unblock it.
  CreateTargetResult first_spawn_result;
  first_spawn_result.result_code = SBOX_ERROR_GENERIC;
  delegate->reached_first_creation_event_ =
      ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
  delegate->first_block_event_ = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);

  {
    auto policy = broker->CreatePolicy();
    EXPECT_EQ(SBOX_ALL_OK, policy->GetConfig()->SetTokenLevel(USER_INTERACTIVE,
                                                              USER_LOCKDOWN));

    broker->SpawnTargetAsync(
        prog_name, arguments.c_str(), std::move(policy),
        base::BindOnce(&FinishSpawnTargetAsync,
                       base::Unretained(&first_spawn_result),
                       base::Unretained(&run_loop),
                       base::Unretained(&launches_remaining_count)));
  }

  ::WaitForSingleObject(delegate->reached_first_creation_event_, INFINITE);

  // Launch the second process.
  CreateTargetResult second_spawn_result;
  second_spawn_result.result_code = SBOX_ERROR_GENERIC;
  {
    auto policy = broker->CreatePolicy();
    EXPECT_EQ(SBOX_ALL_OK, policy->GetConfig()->SetTokenLevel(USER_INTERACTIVE,
                                                              USER_LOCKDOWN));

    broker->SpawnTargetAsync(
        prog_name, arguments.c_str(), std::move(policy),
        base::BindOnce(&FinishSpawnTargetAsync,
                       base::Unretained(&second_spawn_result),
                       base::Unretained(&run_loop),
                       base::Unretained(&launches_remaining_count)));
  }

  run_loop.Run();

  // Targets should be created on different threads.
  EXPECT_NE(delegate->first_creation_thread_id_,
            delegate->second_creation_thread_id_);

  EXPECT_NE(delegate->first_trace_id_, delegate->second_trace_id_);

  EXPECT_EQ(SBOX_ALL_OK, first_spawn_result.result_code);
  EXPECT_EQ(SBOX_ALL_OK, second_spawn_result.result_code);

  EXPECT_EQ(1u,
            ::ResumeThread(first_spawn_result.process_info.thread_handle()));
  EXPECT_EQ(1u,
            ::ResumeThread(second_spawn_result.process_info.thread_handle()));

  HANDLE handles[2] = {first_spawn_result.process_info.process_handle(),
                       second_spawn_result.process_info.process_handle()};
  EXPECT_EQ(static_cast<DWORD>(WAIT_TIMEOUT),
            ::WaitForMultipleObjects(2, handles, /*bWaitAll=*/TRUE, 2000));

  EXPECT_TRUE(
      ::TerminateProcess(first_spawn_result.process_info.process_handle(), 0));
  EXPECT_TRUE(
      ::TerminateProcess(second_spawn_result.process_info.process_handle(), 0));

  ::WaitForMultipleObjects(2, handles, /*bWaitAll=*/TRUE, INFINITE);
}

}  // namespace sandbox
