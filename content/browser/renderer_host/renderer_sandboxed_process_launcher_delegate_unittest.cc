// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/renderer_sandboxed_process_launcher_delegate.h"

#include <optional>
#include <string>

#include "base/win/windows_version.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/switches.h"
#include "sandbox/policy/win/sandbox_policy_feature_test.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/app_container_base.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/sandbox_policy_diagnostic.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace content::sandbox::policy {

namespace {

class TestTargetConfig : public ::sandbox::TargetConfig {
 public:
  ~TestTargetConfig() override {}
  bool IsConfigured() const override { return false; }
  ::sandbox::ResultCode SetTokenLevel(::sandbox::TokenLevel initial,
                                      ::sandbox::TokenLevel lockdown) override {
    return ::sandbox::SBOX_ALL_OK;
  }
  ::sandbox::TokenLevel GetInitialTokenLevel() const override { return {}; }
  ::sandbox::TokenLevel GetLockdownTokenLevel() const override { return {}; }
  ::sandbox::ResultCode SetJobLevel(::sandbox::JobLevel job_level,
                                    uint32_t ui_exceptions) override {
    return ::sandbox::SBOX_ALL_OK;
  }
  ::sandbox::JobLevel GetJobLevel() const override { return {}; }
  void SetJobMemoryLimit(size_t memory_limit) override {}
  ::sandbox::ResultCode AllowFileAccess(::sandbox::FileSemantics semantics,
                                        std::wstring_view pattern) override {
    return ::sandbox::SBOX_ALL_OK;
  }
  ::sandbox::ResultCode AllowExtraDll(std::wstring_view path) override {
    return ::sandbox::SBOX_ALL_OK;
  }
  ::sandbox::ResultCode SetFakeGdiInit() override {
    return ::sandbox::SBOX_ALL_OK;
  }
  void AddDllToUnload(std::wstring_view dll_name) override {}
  const std::vector<std::wstring>& blocklisted_dlls() const {
    return blocklisted_dlls_;
  }
  ::sandbox::ResultCode SetIntegrityLevel(
      ::sandbox::IntegrityLevel level) override {
    return ::sandbox::SBOX_ALL_OK;
  }
  ::sandbox::IntegrityLevel GetIntegrityLevel() const override { return {}; }
  void SetDelayedIntegrityLevel(::sandbox::IntegrityLevel level) override {}
  ::sandbox::ResultCode SetLowBox(base::wcstring_view sid) override {
    return ::sandbox::SBOX_ALL_OK;
  }
  ::sandbox::ResultCode SetProcessMitigations(
      ::sandbox::MitigationFlags flags) override {
    return ::sandbox::SBOX_ALL_OK;
  }
  ::sandbox::MitigationFlags GetProcessMitigations() override { return {}; }
  ::sandbox::ResultCode SetDelayedProcessMitigations(
      ::sandbox::MitigationFlags flags) override {
    return ::sandbox::SBOX_ALL_OK;
  }
  ::sandbox::MitigationFlags GetDelayedProcessMitigations() const override {
    return {};
  }
  void AddRestrictingRandomSid() override {}
  void SetLockdownDefaultDacl() override {}
  void AddKernelObjectToClose(::sandbox::HandleToClose handle_info) override {}
  void SetDisconnectCsrss() override {}

  ::sandbox::ResultCode AddAppContainerProfile(
      base::wcstring_view package_name) override {
    return ::sandbox::SBOX_ALL_OK;
  }

  ::sandbox::AppContainer* GetAppContainer() override { return nullptr; }

  void SetDesktop(::sandbox::Desktop desktop) override {}
  void SetFilterEnvironment(bool env) override {}
  bool GetEnvironmentFiltered() override { return false; }
  void SetZeroAppShim() override {}
  void SetSecurityAttributeName(std::wstring_view name) override {
    security_attribute_name_ = name;
  }

  const std::optional<std::wstring>& GetSecurityAttributeName() const {
    return security_attribute_name_;
  }

 private:
  std::optional<std::wstring> security_attribute_name_;
  std::vector<std::wstring> blocklisted_dlls_;
};

class TestTargetPolicy : public ::sandbox::TargetPolicy {
 public:
  TestTargetPolicy() = default;

  ::sandbox::TargetConfig* GetConfig() override { return &config_; }
  ::sandbox::ResultCode SetStdoutHandle(HANDLE handle) override {
    return ::sandbox::SBOX_ALL_OK;
  }
  ::sandbox::ResultCode SetStderrHandle(HANDLE handle) override {
    return ::sandbox::SBOX_ALL_OK;
  }
  void AddHandleToShare(HANDLE handle) override {}
  void AddDelegateData(base::span<const uint8_t> data) override {}

  const TestTargetConfig& GetTestConfig() const { return config_; }

 private:
  TestTargetConfig config_;
};

}  // namespace

class RendererFeatureSandboxWinTest
    : public ::sandbox::policy::SandboxFeatureTest {
 public:
  RendererFeatureSandboxWinTest() = default;

  ::sandbox::AppContainerType GetExpectedAppContainerType() override {
    // App Containers are not well supported until Windows 10 RS5.
    if (base::win::GetVersion() >= base::win::Version::WIN10_RS5 &&
        ::testing::get<TestParameter::kEnableRendererAppContainer>(GetParam()))
      return ::sandbox::AppContainerType::kLowbox;

    return ::sandbox::AppContainerType::kNone;
  }

  ::sandbox::MitigationFlags GetExpectedMitigationFlags() override {
    // Mitigation flags are set on the policy regardless of the OS version
    return SandboxFeatureTest::GetExpectedMitigationFlags() |
           ::sandbox::MITIGATION_CET_DISABLED;
  }
};

TEST_P(RendererFeatureSandboxWinTest, RendererGeneratedPolicyTest) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  base::HandlesToInheritVector handles_to_inherit;
  ::sandbox::BrokerServices* broker =
      ::sandbox::SandboxFactory::GetBrokerServices();
  auto policy = broker->CreatePolicy();

  content::RendererSandboxedProcessLauncherDelegateWin test_renderer_delegate(
      cmd_line, /*is_pdf_renderer=*/false, /*is_jit_disabled=*/false);

  // PreSpawn
  ::sandbox::ResultCode result =
      ::sandbox::policy::SandboxWin::GeneratePolicyForSandboxedProcess(
          cmd_line, handles_to_inherit, &test_renderer_delegate, policy.get());
  ASSERT_EQ(::sandbox::ResultCode::SBOX_ALL_OK, result);

  ValidateSecurityLevels(policy->GetConfig());
  ValidatePolicyFlagSettings(policy->GetConfig());
  ValidateAppContainerSettings(policy->GetConfig());
}

INSTANTIATE_TEST_SUITE_P(
    RendererSandboxSettings,
    RendererFeatureSandboxWinTest,
    ::testing::Combine(
        /* renderer app container feature */ ::testing::Bool(),
        /* ktm mitigation feature */ ::testing::Bool()));

TEST(RendererSandboxedProcessLauncherDelegateTest,
     IsolationSecurityAttributeName) {
  // This security attribute is present on all processes, and is thus useful for
  // testing.
  static const constexpr wchar_t kProcUniqueAttribute[] = L"TSA://ProcUnique";

  class CustomSecurityAttributeNameBrowserClient
      : public content::ContentBrowserClient {
    std::optional<std::wstring> GetWindowsSecurityAttributeName()
        const override {
      return kProcUniqueAttribute;
    }
  } test_browser_client;

  auto* old_browser_client =
      content::SetBrowserClientForTesting(&test_browser_client);
  absl::Cleanup reset_browser_client = [&old_browser_client]() {
    content::SetBrowserClientForTesting(old_browser_client);
  };

  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  content::RendererSandboxedProcessLauncherDelegateWin test_renderer_delegate(
      cmd_line, /*is_pdf_renderer=*/false, /*is_jit_disabled=*/false);

  // First verify against the mock target policy, this allows the test to
  // examine the final security attribute name is correct.
  {
    TestTargetPolicy policy;

    ::sandbox::ResultCode result =
        ::sandbox::policy::SandboxWin::GeneratePolicyForSandboxedProcess(
            cmd_line, /*handles_to_inherit=*/{}, &test_renderer_delegate,
            &policy);

    ASSERT_EQ(::sandbox::ResultCode::SBOX_ALL_OK, result);
    ASSERT_EQ(policy.GetTestConfig().GetSecurityAttributeName(),
              kProcUniqueAttribute);
  }
  // Verify the broker's policy handles the security attribute name correctly,
  // although it's not possible currently to verify that it correctly applies to
  // the target. This is tested in sbox_unittests.
  {
    auto policy =
        ::sandbox::SandboxFactory::GetBrokerServices()->CreatePolicy();
    ::sandbox::ResultCode result =
        ::sandbox::policy::SandboxWin::GeneratePolicyForSandboxedProcess(
            cmd_line, /*handles_to_inherit=*/{}, &test_renderer_delegate,
            policy.get());
    ASSERT_EQ(::sandbox::ResultCode::SBOX_ALL_OK, result);
  }
}

}  // namespace content::sandbox::policy
