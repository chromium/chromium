// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/utility_sandbox_delegate.h"

#include <memory>

#include "base/environment.h"
#include "base/test/scoped_feature_list.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/app_container_base.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class UtilitySandboxDelegateWinTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          std::tuple</*AppContainerDisabled=*/bool,
                     /*kPrintCompositorLPAC=*/bool>> {
 public:
  UtilitySandboxDelegateWinTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        sandbox::policy::features::kPrintCompositorLPAC,
        IsLPACFeatureEnabledForTest());
  }

 protected:
  static bool IsAppContainerDisabledForTest() {
    return std::get<0>(GetParam());
  }
  static bool IsLPACFeatureEnabledForTest() { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(UtilitySandboxDelegateWinTest, IsAppContainerDisabled) {
  class MaybeDisableAppCongtainerBrowserClient : public ContentBrowserClient {
    bool IsAppContainerDisabled(sandbox::mojom::Sandbox sandbox_type) override {
      return IsAppContainerDisabledForTest();
    }
  } test_browser_client;

  auto* old_browser_client = SetBrowserClientForTesting(&test_browser_client);

  // Print compositor is chosen as a sandbox type, because it is a utility
  // process that has App Container enabled by default.
  constexpr auto kSandboxType = sandbox::mojom::Sandbox::kPrintCompositor;

  auto cmd_line = base::CommandLine(
      ChildProcessHost::GetChildPath(ChildProcessHost::CHILD_NORMAL));
  cmd_line.AppendSwitchASCII(switches::kProcessType, switches::kUtilityProcess);
  sandbox::policy::SetCommandLineFlagsForSandboxType(&cmd_line, kSandboxType);

  base::EnvironmentMap env;
  auto utility_delegate =
      std::make_unique<UtilitySandboxedProcessLauncherDelegate>(kSandboxType,
                                                                env, cmd_line);
  base::HandlesToInheritVector handles_to_inherit;
  sandbox::BrokerServices* broker =
      sandbox::SandboxFactory::GetBrokerServices();
  auto policy = broker->CreatePolicy();

  sandbox::ResultCode result =
      sandbox::policy::SandboxWin::GeneratePolicyForSandboxedProcess(
          cmd_line, handles_to_inherit, utility_delegate.get(), policy.get());
  ASSERT_EQ(sandbox::ResultCode::SBOX_ALL_OK, result);
  SetBrowserClientForTesting(old_browser_client);

  bool expected_app_container_enabled = IsLPACFeatureEnabledForTest();

  // The content client can override the state to disable it, but not to enable
  // it if the feature is disabled.
  if (IsAppContainerDisabledForTest()) {
    expected_app_container_enabled = false;
  }

  if (expected_app_container_enabled) {
    EXPECT_TRUE(policy->GetConfig()->GetAppContainer());
    EXPECT_EQ(policy->GetConfig()->GetIntegrityLevel(),
              sandbox::IntegrityLevel::INTEGRITY_LEVEL_LAST);
    EXPECT_EQ(policy->GetConfig()->GetInitialTokenLevel(),
              sandbox::TokenLevel::USER_UNPROTECTED);
    EXPECT_EQ(policy->GetConfig()->GetLockdownTokenLevel(),
              sandbox::TokenLevel::USER_UNPROTECTED);
  } else {
    EXPECT_FALSE(policy->GetConfig()->GetAppContainer());
    EXPECT_EQ(policy->GetConfig()->GetIntegrityLevel(),
              sandbox::IntegrityLevel::INTEGRITY_LEVEL_LOW);
    EXPECT_EQ(policy->GetConfig()->GetInitialTokenLevel(),
              sandbox::TokenLevel::USER_RESTRICTED_SAME_ACCESS);
    EXPECT_EQ(policy->GetConfig()->GetLockdownTokenLevel(),
              sandbox::TokenLevel::USER_LOCKDOWN);
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    UtilitySandboxDelegateWinTest,
// TODO(crbug.com/353583970):  Enable `kPrintCompositorLPAC` for ARM when
// flakiness is resolved.
#if defined(ARCH_CPU_ARM_FAMILY)
    ::testing::Combine(/*AppContainerDisabled=*/::testing::Bool(),
                       /*kPrintCompositorLPAC=*/::testing::Values(false)),
#else
    ::testing::Combine(/*AppContainerDisabled=*/::testing::Bool(),
                       /*kPrintCompositorLPAC=*/::testing::Bool()),
#endif
    [](const auto& info) {
      return base::StrCat(
          {std::get<0>(info.param) ? "ACDisabled" : "ACEnabled",
           std::get<1>(info.param) ? "FeatureEnabled" : "FeatureDisabled"});
    });

}  // namespace content
