// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/unprivileged_process_delegate.h"

#include <windows.h>

#include <userenv.h>

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "base/win/access_token.h"
#include "remoting/host/win/security_descriptor.h"
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

class FallbackTestDelegate : public UnprivilegedProcessDelegate {
 public:
  FallbackTestDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      std::unique_ptr<base::CommandLine> target_command,
      DWORD first_attempt_error)
      : UnprivilegedProcessDelegate(std::move(io_task_runner),
                                    std::move(target_command),
                                    IntegrityLevel::kUntrusted),
        first_attempt_error_(first_attempt_error) {}

  const std::vector<bool>& had_security_capabilities_calls() const {
    return had_security_capabilities_calls_;
  }

 protected:
  bool DoLaunchProcessWithToken(
      const base::FilePath& binary,
      const base::CommandLine::StringType& command_line,
      HANDLE user_token,
      SECURITY_ATTRIBUTES* process_attributes,
      SECURITY_ATTRIBUTES* thread_attributes,
      const base::HandlesToInheritVector& handles_to_inherit,
      SECURITY_CAPABILITIES* security_capabilities,
      DWORD creation_flags,
      const wchar_t* desktop_name,
      base::win::ScopedHandle* process_out,
      base::win::ScopedHandle* thread_out,
      DWORD* last_error_out) override {
    had_security_capabilities_calls_.push_back(security_capabilities !=
                                               nullptr);
    if (had_security_capabilities_calls_.size() == 1u) {
      if (last_error_out) {
        *last_error_out = first_attempt_error_;
      }
      return false;
    }

    process_out->Set(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
    thread_out->Set(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (last_error_out) {
      *last_error_out = ERROR_SUCCESS;
    }
    return process_out->is_valid() && thread_out->is_valid();
  }

 private:
  const DWORD first_attempt_error_;
  std::vector<bool> had_security_capabilities_calls_;
};

}  // namespace

class UnprivilegedProcessDelegateTest : public testing::Test {
 public:
  UnprivilegedProcessDelegateTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        test_profile_name_(
            L"chromoting.unittest." +
            base::ASCIIToWide(base::UnguessableToken::Create().ToString())) {}

  void TearDown() override {
    observed_process_.Close();
    ::DeleteAppContainerProfile(test_profile_name_.c_str());
  }

 protected:
  using AppContainer = UnprivilegedProcessDelegate::AppContainer;

  static std::optional<AppContainer> CreateAppContainer(
      const std::wstring& profile_name) {
    return UnprivilegedProcessDelegate::CreateAppContainer(profile_name);
  }

  static base::win::ScopedHandle PrewarmAppContainerForToken(
      const std::wstring& profile_name,
      const base::win::AccessToken& base_token) {
    return UnprivilegedProcessDelegate::PrewarmAppContainerForToken(
        profile_name, base_token);
  }

  static bool PrepareAndLaunchWithFallback(
      UnprivilegedProcessDelegate* delegate,
      const std::wstring& profile_name,
      base::win::ScopedHandle* worker_process_out,
      base::win::ScopedHandle* worker_thread_out) {
    delegate->app_container_ = CreateAppContainer(profile_name);
    if (!delegate->app_container_.has_value()) {
      return false;
    }
    SECURITY_CAPABILITIES capabilities =
        delegate->app_container_->GetSecurityCapabilities();
    base::CommandLine command_line(base::FilePath(L"dummy.exe"));
    return delegate->LaunchWorkerProcessWithFallback(
        command_line, /*user_token=*/nullptr, /*process_attributes=*/nullptr,
        /*thread_attributes=*/nullptr, /*handles_to_inherit=*/{}, &capabilities,
        worker_process_out, worker_thread_out);
  }

  static bool HasActiveAppContainer(
      const UnprivilegedProcessDelegate& delegate) {
    return delegate.app_container_.has_value();
  }

  base::test::TaskEnvironment task_environment_;
  base::win::ScopedHandle observed_process_;
  const std::wstring test_profile_name_;
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

TEST_F(UnprivilegedProcessDelegateTest, AppContainerCapabilities) {
  std::wstring profile_name =
      L"chromoting.unittest." +
      base::ASCIIToWide(base::UnguessableToken::Create().ToString());

  std::optional<AppContainer> app_container = CreateAppContainer(profile_name);
  ASSERT_TRUE(app_container.has_value());

  SECURITY_CAPABILITIES capabilities = app_container->GetSecurityCapabilities();
  EXPECT_TRUE(app_container->package_sid);
  EXPECT_EQ(capabilities.AppContainerSid, app_container->package_sid.get());
  EXPECT_EQ(capabilities.CapabilityCount, 5u);

  ASSERT_NE(capabilities.Capabilities, nullptr);
  EXPECT_EQ(capabilities.Capabilities[0].Attributes,
            static_cast<DWORD>(SE_GROUP_ENABLED));
}

TEST_F(UnprivilegedProcessDelegateTest,
       PrewarmAppContainerCreatesValidLowBoxToken) {
  // Use a fixed profile name rather than a random `UnguessableToken` so
  // repeated test runs on Windows 11 24H2/25H2 bots reuse a single LowBox
  // table slot instead of leaking one slot per run (crbug.com/582900491).
  // `PrewarmAppContainerForToken()` does not create or delete on-disk/registry
  // AppContainer profiles, so sharing this name across parallel test shards is
  // also race-free.
  constexpr wchar_t kPrewarmTestProfileName[] = L"chromoting.unittest.prewarm";

  std::optional<base::win::AccessToken> current_token =
      base::win::AccessToken::FromCurrentProcess(/*impersonation=*/false,
                                                 TOKEN_DUPLICATE);
  ASSERT_TRUE(current_token.has_value());

  base::win::ScopedHandle prewarmed_handle =
      PrewarmAppContainerForToken(kPrewarmTestProfileName, *current_token);
  ASSERT_TRUE(prewarmed_handle.is_valid());

  std::optional<base::win::AccessToken> prewarmed_token =
      base::win::AccessToken::FromToken(std::move(prewarmed_handle));
  ASSERT_TRUE(prewarmed_token.has_value());
  EXPECT_TRUE(prewarmed_token->IsAppContainer());

  PSID expected_raw_sid = nullptr;
  ASSERT_HRESULT_SUCCEEDED(::DeriveAppContainerSidFromAppContainerName(
      kPrewarmTestProfileName, &expected_raw_sid));
  std::optional<base::win::Sid> expected_sid =
      base::win::Sid::FromPSID(expected_raw_sid);
  ::FreeSid(expected_raw_sid);
  ASSERT_TRUE(expected_sid.has_value());
  EXPECT_EQ(prewarmed_token->AppContainerSid(), expected_sid);
}

TEST_F(UnprivilegedProcessDelegateTest,
       FallsBackWithoutAppContainerOnNoSystemResources) {
  FallbackTestDelegate delegate(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM),
      ERROR_NO_SYSTEM_RESOURCES);

  base::win::ScopedHandle worker_process;
  base::win::ScopedHandle worker_thread;
  EXPECT_TRUE(PrepareAndLaunchWithFallback(&delegate, test_profile_name_,
                                           &worker_process, &worker_thread));

  std::vector<bool> expected_calls = {true, false};
  EXPECT_EQ(delegate.had_security_capabilities_calls(), expected_calls);
  EXPECT_FALSE(HasActiveAppContainer(delegate));
  EXPECT_TRUE(worker_process.is_valid());
  EXPECT_TRUE(worker_thread.is_valid());
}

TEST_F(UnprivilegedProcessDelegateTest,
       DoesNotFallbackWithoutAppContainerOnOtherErrors) {
  FallbackTestDelegate delegate(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM),
      ERROR_ACCESS_DENIED);

  base::win::ScopedHandle worker_process;
  base::win::ScopedHandle worker_thread;
  EXPECT_FALSE(PrepareAndLaunchWithFallback(&delegate, test_profile_name_,
                                            &worker_process, &worker_thread));

  std::vector<bool> expected_calls = {true};
  EXPECT_EQ(delegate.had_security_capabilities_calls(), expected_calls);
  EXPECT_TRUE(HasActiveAppContainer(delegate));
}

MULTIPROCESS_TEST_MAIN(UnprivilegedProcessDelegateTestChild) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);

  base::RunLoop().Run();
  return 0;
}

}  // namespace remoting
