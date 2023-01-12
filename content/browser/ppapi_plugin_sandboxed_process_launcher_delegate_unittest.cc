// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ppapi_plugin_sandboxed_process_launcher_delegate.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#include "sandbox/policy/win/sandbox_policy_feature_test.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/app_container_base.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#endif

using ::testing::_;
using ::testing::Return;

using ::testing::ElementsAre;
using ::testing::Pair;

namespace content {
namespace sandbox {
namespace policy {

#if BUILDFLAG(IS_WIN)
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

  base::test::ScopedFeatureList feature_list_;
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
#endif

}  // namespace policy
}  // namespace sandbox
}  // namespace content
