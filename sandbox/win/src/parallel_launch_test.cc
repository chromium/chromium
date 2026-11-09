// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/shared_memory_switch.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/process/process_info.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/gtest_util.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/scoped_process_information.h"
#include "sandbox/win/src/broker_services.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace sandbox {

// BrokerServicesDelegate common implementation.
class TestBrokerServicesDelegateBase : public BrokerServicesDelegate {
 public:
  void ParallelLaunchPostTaskAndReplyWithResult(
      const base::Location& from_here,
      base::OnceCallback<CreateTargetResult()> task,
      base::OnceCallback<void(CreateTargetResult)> reply) override {
    BrokerServicesDelegate::ParallelLaunchPostTaskAndReplyWithResult(
        from_here, WrapCreationTask(std::move(task)), std::move(reply));
  }

  void OnCreateThreadActionCreateFailure(DWORD last_error) override {}
  void OnCreateThreadActionDuplicateFailure(DWORD last_error) override {}

  virtual void BeforeTargetProcessCreateOnCreationThread() {}

 protected:
  virtual base::OnceCallback<CreateTargetResult()> WrapCreationTask(
      base::OnceCallback<CreateTargetResult()> task) {
    // Existing launch tests keep their delegate alive until the reply runs.
    return base::BindOnce(&TestBrokerServicesDelegateBase::
                              BeforeTargetProcessCreateOnCreationThread,
                          base::Unretained(this))
        .Then(std::move(task));
  }
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
  void BeforeTargetProcessCreateOnCreationThread() override {
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

  base::CommandLine cmd_line =
      WaitCommandTestRunner::CreateCommandLineForTesting();

  auto policy = broker->CreatePolicy();
  EXPECT_EQ(SBOX_ALL_OK, policy->GetConfig()->SetTokenLevel(USER_INTERACTIVE,
                                                            USER_LOCKDOWN));

  CreateTargetResult spawn_result;
  spawn_result.result_code = SBOX_ERROR_GENERIC;

  base::RunLoop run_loop;
  int launches_remaining_count = 1;
  broker->SpawnTargetAsync(
      cmd_line, std::move(policy),
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
  void BeforeTargetProcessCreateOnCreationThread() override {
    if (first_launch_) {
      first_creation_thread_id_ = ::GetCurrentThreadId();
      first_launch_ = false;
      EXPECT_TRUE(::SetEvent(reached_first_creation_event_));
      ::WaitForSingleObject(first_block_event_, INFINITE);
    } else {
      second_creation_thread_id_ = ::GetCurrentThreadId();
      EXPECT_TRUE(::SetEvent(first_block_event_));
    }
  }

  bool first_launch_ = true;
  HANDLE reached_first_creation_event_;
  HANDLE first_block_event_;
  DWORD first_creation_thread_id_;
  DWORD second_creation_thread_id_;
};

// This test launches two processes and synchronizes the target creation threads
// to run at the same time.
TEST_F(ParallelLaunchTest, ParallelLaunch) {
  BrokerServices* broker = GetBroker();
  ASSERT_TRUE(broker);

  auto* delegate = new ParallelLaunch_TestBrokerServicesDelegate();
  static_cast<BrokerServicesBase*>(broker)->SetBrokerServicesDelegateForTesting(
      std::unique_ptr<BrokerServicesDelegate>(delegate));

  base::CommandLine cmd_line =
      WaitCommandTestRunner::CreateCommandLineForTesting();

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
        cmd_line, std::move(policy),
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
        cmd_line, std::move(policy),
        base::BindOnce(&FinishSpawnTargetAsync,
                       base::Unretained(&second_spawn_result),
                       base::Unretained(&run_loop),
                       base::Unretained(&launches_remaining_count)));
  }

  run_loop.Run();

  // Targets should be created on different threads.
  EXPECT_NE(delegate->first_creation_thread_id_,
            delegate->second_creation_thread_id_);

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

namespace {

constexpr char kAbandonedTargetInfo[] = "abandoned-target-info";

struct AbandonedTargetInfo {
  base::ProcessId parent_pid;
  // This handle belongs to the parent, not the broker subprocess.
  HANDLE target_handle;
};

struct ShutdownLaunchState {
  base::WaitableEvent spawn_posted;
  base::WaitableEvent creation_started;
  base::WaitableEvent release_creation;
  base::WaitableEvent creation_completed;
  base::WaitableEvent reply_called;
  base::WritableSharedMemoryMapping abandoned_target_mapping;
  bool keep_target_alive = false;
  base::win::ScopedProcessInformation target;
  base::WaitableEvent broker_destroyed;
};

using RefCountedShutdownLaunchState = base::RefCountedData<ShutdownLaunchState>;

class ShutdownLaunchBrokerServicesDelegate
    : public TestBrokerServicesDelegateBase {
 public:
  explicit ShutdownLaunchBrokerServicesDelegate(
      scoped_refptr<RefCountedShutdownLaunchState> state)
      : state_(std::move(state)) {}

 protected:
  base::OnceCallback<CreateTargetResult()> WrapCreationTask(
      base::OnceCallback<CreateTargetResult()> task) override {
    // The delegate and broker will be destroyed while this callback is blocked.
    // Neither the blocking state nor the wrapper may depend on their lifetime.
    return base::BindOnce(
        [](scoped_refptr<RefCountedShutdownLaunchState> state,
           base::OnceCallback<CreateTargetResult()> task) {
          base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
          state->data.creation_started.Signal();
          CHECK(state->data.release_creation.TimedWait(
              TestTimeouts::action_max_timeout()));

          CreateTargetResult result = std::move(task).Run();
          CHECK_EQ(SBOX_ALL_OK, result.result_code);
          CHECK(result.process_info.IsValid());

          if (state->data.keep_target_alive) {
            CHECK(state->data.target.DuplicateFrom(result.process_info));
          } else if (state->data.abandoned_target_mapping.IsValid()) {
            auto* info = state->data.abandoned_target_mapping
                             .GetMemoryAs<AbandonedTargetInfo>();
            base::Process parent = base::Process::OpenWithAccess(
                info->parent_pid, PROCESS_DUP_HANDLE);
            CHECK(parent.IsValid());
            CHECK(::DuplicateHandle(::GetCurrentProcess(),
                                    result.process_info.process_handle(),
                                    parent.Handle(), &info->target_handle,
                                    SYNCHRONIZE | PROCESS_TERMINATE |
                                        PROCESS_QUERY_LIMITED_INFORMATION,
                                    FALSE, 0));
            CHECK_EQ(
                static_cast<DWORD>(WAIT_TIMEOUT),
                ::WaitForSingleObject(result.process_info.process_handle(), 0));
          } else {
            // This test checks creation lifetime, not target cleanup.
            CHECK(::TerminateProcess(result.process_info.process_handle(), 0));
            CHECK_EQ(static_cast<DWORD>(WAIT_OBJECT_0),
                     ::WaitForSingleObject(
                         result.process_info.process_handle(),
                         static_cast<DWORD>(TestTimeouts::action_timeout()
                                                .InMillisecondsRoundedUp())));
          }
          state->data.creation_completed.Signal();
          return result;
        },
        state_, std::move(task));
  }

 private:
  scoped_refptr<RefCountedShutdownLaunchState> state_;
};

}  // namespace

MULTIPROCESS_TEST_MAIN(ParallelLaunchShutdownChild) {
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "ParallelLaunchShutdown");
  auto state = base::MakeRefCounted<RefCountedShutdownLaunchState>();
  const auto& command_line = *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(kAbandonedTargetInfo)) {
    auto region = base::shared_memory::UnsafeSharedMemoryRegionFrom(
        command_line.GetSwitchValueASCII(kAbandonedTargetInfo));
    CHECK(region.has_value());
    state->data.abandoned_target_mapping = region->Map();
    CHECK(state->data.abandoned_target_mapping.IsValid());
  }
  std::optional<BrokerServicesBase> broker(std::in_place);
  CHECK_EQ(SBOX_ALL_OK,
           broker->Init(
               std::make_unique<ShutdownLaunchBrokerServicesDelegate>(state)));

  // Match Chrome's launcher, whose late replies are rejected at pool shutdown.
  // Cancellation on an origin that remains live is covered separately.
  auto origin_runner = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  CHECK(origin_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](BrokerServicesBase* broker,
             scoped_refptr<RefCountedShutdownLaunchState> state) {
            auto policy = broker->CreatePolicy();
            CHECK_EQ(SBOX_ALL_OK, policy->GetConfig()->SetTokenLevel(
                                      USER_INTERACTIVE, USER_LOCKDOWN));
            broker->SpawnTargetAsync(
                WaitCommandTestRunner::CreateCommandLineForTesting(),
                std::move(policy),
                base::BindOnce(
                    [](scoped_refptr<RefCountedShutdownLaunchState> state,
                       base::win::ScopedProcessInformation target,
                       DWORD last_error, ResultCode result) {
                      state->data.reply_called.Signal();
                    },
                    state));
            state->data.spawn_posted.Signal();
          },
          base::Unretained(&broker.value()), state)));

  CHECK(state->data.spawn_posted.TimedWait(TestTimeouts::action_timeout()));
  CHECK(state->data.creation_started.TimedWait(TestTimeouts::action_timeout()));

  CHECK(origin_runner->PostTask(
      FROM_HERE, base::BindOnce(&BrokerServicesBase::Shutdown,
                                base::Unretained(&broker.value()))));
  // A BLOCK_SHUTDOWN creation task would hang here until the parent times out
  // and kills this child (or the creation task's finite wait fails).
  base::ThreadPoolInstance::Get()->Shutdown();
  CHECK(!state->data.creation_completed.IsSignaled());
  broker.reset();

  state->data.release_creation.Signal();
  CHECK(
      state->data.creation_completed.TimedWait(TestTimeouts::action_timeout()));
  // Join also waits for PostTaskAndReply to attempt posting the reply to the
  // shut-down origin sequence, not merely for our creation wrapper to return.
  base::ThreadPoolInstance::Get()->JoinForTesting();
  CHECK(!state->data.reply_called.IsSignaled());
  return 0;
}

TEST(ParallelLaunchShutdownTest, CreationOutlivesBroker) {
  base::LaunchOptions options;
  options.start_hidden = true;
  base::Process child = base::SpawnMultiProcessTestChild(
      "ParallelLaunchShutdownChild",
      base::GetMultiProcessTestChildBaseCommandLine(), options);
  ASSERT_TRUE(child.IsValid());

  int exit_code = -1;
  if (!base::WaitForMultiprocessTestChildExit(
          child, TestTimeouts::action_max_timeout(), &exit_code)) {
    EXPECT_TRUE(base::TerminateMultiProcessTestChild(child, 1, /*wait=*/true));
    FAIL() << "Parallel launch shutdown child timed out";
  }
  EXPECT_EQ(0, exit_code);
}

TEST_F(ParallelLaunchTest, ShutdownCancelsReplyOnLiveOrigin) {
  auto state = base::MakeRefCounted<RefCountedShutdownLaunchState>();
  state->data.keep_target_alive = true;
  auto broker = std::make_unique<BrokerServicesBase>();
  ASSERT_EQ(SBOX_ALL_OK,
            broker->Init(
                std::make_unique<ShutdownLaunchBrokerServicesDelegate>(state)));
  base::Thread teardown_thread("SandboxBrokerTeardown");
  ASSERT_TRUE(teardown_thread.Start());
  base::ScopedClosureRunner release_creation(base::BindOnce(
      [](scoped_refptr<RefCountedShutdownLaunchState> state) {
        state->data.release_creation.Signal();
      },
      state));

  auto policy = broker->CreatePolicy("shutdown-test");
  ASSERT_EQ(SBOX_ALL_OK, policy->GetConfig()->SetTokenLevel(USER_INTERACTIVE,
                                                            USER_LOCKDOWN));
  base::test::TestFuture<void> reply_destroyed;
  broker->SpawnTargetAsync(
      WaitCommandTestRunner::CreateCommandLineForTesting(), std::move(policy),
      base::BindOnce(
          [](scoped_refptr<RefCountedShutdownLaunchState> state,
             base::ScopedClosureRunner signal_reply_destroyed,
             base::win::ScopedProcessInformation target, DWORD last_error,
             ResultCode result) { state->data.reply_called.Signal(); },
          state, base::ScopedClosureRunner(reply_destroyed.GetCallback())));
  ASSERT_TRUE(
      state->data.creation_started.TimedWait(TestTimeouts::action_timeout()));
  broker->Shutdown();
  ASSERT_TRUE(teardown_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](std::unique_ptr<BrokerServicesBase> broker,
                        scoped_refptr<RefCountedShutdownLaunchState> state) {
                       broker.reset();
                       state->data.broker_destroyed.Signal();
                     },
                     std::move(broker), state)));
  ASSERT_TRUE(
      state->data.broker_destroyed.TimedWait(TestTimeouts::action_timeout()));

  release_creation.RunAndReset();
  ASSERT_TRUE(
      state->data.creation_completed.TimedWait(TestTimeouts::action_timeout()));
  ASSERT_TRUE(reply_destroyed.Wait());
  EXPECT_FALSE(state->data.reply_called.IsSignaled());

  base::Process target(state->data.target.TakeProcessHandle());
  ASSERT_TRUE(target.IsValid());
  if (!target.WaitForExitWithTimeout(TestTimeouts::action_timeout(), nullptr)) {
    EXPECT_TRUE(target.Terminate(1, /*wait=*/true));
    FAIL() << "Cancelled reply did not release the target's job";
  }
}

TEST_F(ParallelLaunchTest, SharedConfigOutlivesBroker) {
  std::optional<BrokerServicesBase> broker(std::in_place);
  auto first_policy = broker->CreatePolicy("shared-config-test");
  ASSERT_EQ(SBOX_ALL_OK, first_policy->GetConfig()->SetTokenLevel(
                             USER_INTERACTIVE, USER_LOCKDOWN));
  BrokerServicesBase::FreezeTargetConfigForTesting(first_policy->GetConfig());
  auto second_policy = broker->CreatePolicy("shared-config-test");
  ASSERT_EQ(first_policy->GetConfig(), second_policy->GetConfig());

  broker->Shutdown();
  broker.reset();
  first_policy.reset();

  EXPECT_TRUE(second_policy->GetConfig()->IsConfigured());
  EXPECT_TRUE(
      static_cast<PolicyBase*>(second_policy.get())->MakeTokens().has_value());
}

TEST(ParallelLaunchDeathTest, ShutdownCalledTwice) {
  BrokerServicesBase broker;
  broker.Shutdown();
  EXPECT_CHECK_DEATH_WITH(broker.Shutdown(), "!is_shutdown_");
}

TEST_F(ParallelLaunchTest, ShutdownRejectsNewLaunches) {
  BrokerServicesBase broker;
  ASSERT_EQ(SBOX_ALL_OK,
            broker.Init(std::make_unique<TestBrokerServicesDelegateBase>()));
  broker.Shutdown();
  base::test::TestFuture<base::win::ScopedProcessInformation, DWORD, ResultCode>
      result;
  broker.SpawnTargetAsync(WaitCommandTestRunner::CreateCommandLineForTesting(),
                          broker.CreatePolicy(), result.GetCallback());
  ASSERT_TRUE(result.IsReady());
  auto [process_info, last_error, result_code] = result.Take();
  EXPECT_FALSE(process_info.IsValid());
  EXPECT_EQ(static_cast<DWORD>(ERROR_SHUTDOWN_IN_PROGRESS), last_error);
  EXPECT_EQ(SBOX_ERROR_UNEXPECTED_CALL, result_code);
}

TEST_F(ParallelLaunchTest, ShutdownCancelsQueuedReply) {
  auto state = base::MakeRefCounted<RefCountedShutdownLaunchState>();
  state->data.keep_target_alive = true;
  BrokerServicesBase broker;
  ASSERT_EQ(SBOX_ALL_OK,
            broker.Init(
                std::make_unique<ShutdownLaunchBrokerServicesDelegate>(state)));
  auto policy = broker.CreatePolicy("queued-shutdown-test");
  ASSERT_EQ(SBOX_ALL_OK, policy->GetConfig()->SetTokenLevel(USER_INTERACTIVE,
                                                            USER_LOCKDOWN));
  base::test::TestFuture<void> reply_destroyed;
  broker.SpawnTargetAsync(
      WaitCommandTestRunner::CreateCommandLineForTesting(), std::move(policy),
      base::BindOnce(
          [](scoped_refptr<RefCountedShutdownLaunchState> state,
             base::ScopedClosureRunner signal_reply_destroyed,
             base::win::ScopedProcessInformation target, DWORD last_error,
             ResultCode result) { state->data.reply_called.Signal(); },
          state, base::ScopedClosureRunner(reply_destroyed.GetCallback())));
  state->data.release_creation.Signal();
  // The worker has posted the reply, but the origin has not dispatched it.
  base::ThreadPoolInstance::Get()->FlushForTesting();
  ASSERT_TRUE(state->data.creation_completed.IsSignaled());
  broker.Shutdown();
  ASSERT_TRUE(reply_destroyed.Wait());
  EXPECT_FALSE(state->data.reply_called.IsSignaled());

  base::Process target(state->data.target.TakeProcessHandle());
  ASSERT_TRUE(target.IsValid());
  if (!target.WaitForExitWithTimeout(TestTimeouts::action_timeout(), nullptr)) {
    EXPECT_TRUE(target.Terminate(1, /*wait=*/true));
    FAIL() << "Cancelled reply did not release the target's job";
  }
}

TEST(ParallelLaunchShutdownTest, AbandonedTargetExitsWithBrokerProcess) {
  auto region =
      base::UnsafeSharedMemoryRegion::Create(sizeof(AbandonedTargetInfo));
  ASSERT_TRUE(region.IsValid());
  auto mapping = region.Map();
  ASSERT_TRUE(mapping.IsValid());
  auto* info = mapping.GetMemoryAs<AbandonedTargetInfo>();
  *info = {base::GetCurrentProcId(), nullptr};

  auto command_line = base::GetMultiProcessTestChildBaseCommandLine();
  base::LaunchOptions options;
  options.start_hidden = true;
  base::shared_memory::SharedMemorySwitch shared_memory_switch(
      kAbandonedTargetInfo, /*rendezvous_key_in=*/0, /*descriptor_key_in=*/0);
  shared_memory_switch.AddToLaunchParameters(region, &command_line, &options);
  base::Process child = base::SpawnMultiProcessTestChild(
      "ParallelLaunchShutdownChild", command_line, options);
  ASSERT_TRUE(child.IsValid());

  int exit_code = -1;
  const bool child_exited = base::WaitForMultiprocessTestChildExit(
      child, TestTimeouts::action_max_timeout(), &exit_code);
  if (!child_exited) {
    ASSERT_TRUE(base::TerminateMultiProcessTestChild(child, 1, /*wait=*/true));
  }
  // Read only after the broker subprocess has stopped writing to the mapping.
  base::Process target(info->target_handle);
  if (!child_exited || exit_code != 0) {
    if (target.IsValid()) {
      EXPECT_TRUE(target.Terminate(1, /*wait=*/true));
    }
    FAIL() << "Parallel launch shutdown child failed: exited=" << child_exited
           << ", exit_code=" << exit_code;
  }
  ASSERT_TRUE(target.IsValid());

  // The rejected reply retains the policy and job until process exit.
  if (!target.WaitForExitWithTimeout(TestTimeouts::action_timeout(), nullptr)) {
    EXPECT_TRUE(target.Terminate(1, /*wait=*/true));
    FAIL() << "Abandoned target survived broker process exit";
  }
}

}  // namespace sandbox
