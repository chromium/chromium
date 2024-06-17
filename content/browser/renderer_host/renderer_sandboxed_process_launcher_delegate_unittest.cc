// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/renderer_sandboxed_process_launcher_delegate.h"

#include "base/win/windows_version.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/switches.h"
#include "sandbox/policy/win/sandbox_policy_feature_test.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/app_container_base.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::sandbox::policy {

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

}  // namespace content::sandbox::policy
