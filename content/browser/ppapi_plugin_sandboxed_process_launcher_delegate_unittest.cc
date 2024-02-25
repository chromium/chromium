// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ppapi_plugin_sandboxed_process_launcher_delegate.h"

#include "sandbox/policy/switches.h"
#include "sandbox/policy/win/sandbox_policy_feature_test.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/app_container_base.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::sandbox::policy {

class PpapiPluginFeatureSandboxWinTest
    : public ::sandbox::policy::SandboxFeatureTest {
 public:
  PpapiPluginFeatureSandboxWinTest() = default;

  ::sandbox::MitigationFlags GetExpectedDelayedMitigationFlags() override {
    ::sandbox::MitigationFlags flags =
        SandboxFeatureTest::GetExpectedDelayedMitigationFlags() |
        ::sandbox::MITIGATION_DYNAMIC_CODE_DISABLE;
    return flags;
  }
};

TEST_P(PpapiPluginFeatureSandboxWinTest, PpapiGeneratedPolicyTest) {
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  base::HandlesToInheritVector handles_to_inherit;
  ::sandbox::BrokerServices* broker =
      ::sandbox::SandboxFactory::GetBrokerServices();
  auto policy = broker->CreatePolicy();

  PpapiPluginSandboxedProcessLauncherDelegate test_ppapi_delegate;

  // PreSpawn
  ::sandbox::ResultCode result =
      ::sandbox::policy::SandboxWin::GeneratePolicyForSandboxedProcess(
          cmd_line, ::sandbox::policy::switches::kPpapiSandbox,
          handles_to_inherit, &test_ppapi_delegate, policy.get());
  ASSERT_EQ(::sandbox::ResultCode::SBOX_ALL_OK, result);

  ValidateSecurityLevels(policy->GetConfig());
  ValidatePolicyFlagSettings(policy->GetConfig());
  ValidateAppContainerSettings(policy->GetConfig());
}

INSTANTIATE_TEST_SUITE_P(
    PpapiPluginSandboxSettings,
    PpapiPluginFeatureSandboxWinTest,
    ::testing::Combine(
        /* renderer app container feature */ ::testing::Bool(),
        /* ktm mitigation feature */ ::testing::Bool()));

}  // namespace content::sandbox::policy
