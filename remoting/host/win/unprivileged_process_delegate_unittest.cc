// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/unprivileged_process_delegate.h"

#include <memory>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "remoting/host/worker_process_ipc_delegate.h"
#include "remoting/host/worker_process_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace remoting {

namespace {

// Use a simple test delegate to satisfy the UnprivilegedProcessDelegate.
class FakeDelegate : public WorkerProcessIpcDelegate {
 public:
  FakeDelegate() {}
  ~FakeDelegate() override {}

  // WorkerProcessIpcDelegate implementation.
  void OnChannelConnected(int32_t peer_pid) override {}
  void OnPermanentError(int exit_code) override {}
  void OnWorkerProcessStopped() override {}
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override {}
};

class FakeUnprivilegedProcessDelegate : public UnprivilegedProcessDelegate {
 public:
  using UnprivilegedProcessDelegate::UnprivilegedProcessDelegate;

  void LaunchProcess(WorkerProcessLauncher* event_handler) override {
    event_handler_ = event_handler;

    base::CommandLine command_line =
        base::GetMultiProcessTestChildBaseCommandLine();
    base::Process process = base::SpawnMultiProcessTestChild(
        "UnprivilegedProcessDelegateTestChild", command_line, {});
    EXPECT_TRUE(process.IsValid());

    ReportProcessLaunched(base::win::ScopedHandle(process.Release()));
  }
};

class VerifyingDelegate : public FakeUnprivilegedProcessDelegate {
 public:
  VerifyingDelegate(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                    std::unique_ptr<base::CommandLine> target_command,
                    IntegrityLevel integrity_level,
                    base::win::ScopedHandle* observed_process_out)
      : FakeUnprivilegedProcessDelegate(std::move(io_task_runner),
                                        std::move(target_command),
                                        integrity_level),
        observed_process_out_(observed_process_out) {}

  void ReportProcessLaunched(base::win::ScopedHandle worker_process) override {
    CHECK(worker_process.is_valid());
    HANDLE handle;
    if (DuplicateHandle(GetCurrentProcess(), worker_process.Get(),
                        GetCurrentProcess(), &handle,
                        PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, 0)) {
      observed_process_out_->Set(handle);
    }
    FakeUnprivilegedProcessDelegate::ReportProcessLaunched(
        std::move(worker_process));
  }

 private:
  raw_ptr<base::win::ScopedHandle> observed_process_out_;
};

}  // namespace

class UnprivilegedProcessDelegateTest : public testing::Test {
 public:
  UnprivilegedProcessDelegateTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  void TearDown() override { observed_process_.Close(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::win::ScopedHandle observed_process_;
};

TEST_F(UnprivilegedProcessDelegateTest, KillProcessLifecycle) {
  base::CommandLine target_command(base::CommandLine::NO_PROGRAM);
  auto delegate = std::make_unique<FakeUnprivilegedProcessDelegate>(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      std::make_unique<base::CommandLine>(target_command),
      UnprivilegedProcessDelegate::IntegrityLevel::kLow);

  FakeDelegate fake_delegate;
  auto worker_launcher = std::make_unique<WorkerProcessLauncher>(
      std::move(delegate), &fake_delegate);

  worker_launcher.reset();
}

TEST_F(UnprivilegedProcessDelegateTest, KillProcessTerminatesWorker) {
  base::CommandLine target_command(base::CommandLine::NO_PROGRAM);

  auto verifying_delegate = std::make_unique<VerifyingDelegate>(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      std::make_unique<base::CommandLine>(target_command),
      UnprivilegedProcessDelegate::IntegrityLevel::kLow, &observed_process_);

  FakeDelegate fake_delegate;
  auto worker_launcher = std::make_unique<WorkerProcessLauncher>(
      std::move(verifying_delegate), &fake_delegate);

  ASSERT_TRUE(observed_process_.is_valid());

  DWORD exit_code;
  ASSERT_TRUE(GetExitCodeProcess(observed_process_.Get(), &exit_code));
  ASSERT_EQ(exit_code, static_cast<DWORD>(STILL_ACTIVE));

  worker_launcher.reset();

  // Verify the process is terminated.
  ASSERT_TRUE(observed_process_.is_valid());
  EXPECT_EQ(WAIT_OBJECT_0, WaitForSingleObject(observed_process_.Get(), 5000));

  ASSERT_TRUE(GetExitCodeProcess(observed_process_.Get(), &exit_code));
  EXPECT_NE(exit_code, static_cast<DWORD>(STILL_ACTIVE));
}

MULTIPROCESS_TEST_MAIN(UnprivilegedProcessDelegateTestChild) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);

  base::RunLoop().Run();
  return 0;
}

}  // namespace remoting
