// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/win/sandbox_policy_feature_test.h"

namespace sandbox {
namespace policy {

SandboxFeatureTest::SandboxFeatureTest() {
  std::vector<base::Feature> enabled_features;
  std::vector<base::Feature> disabled_features;

  if (::testing::get<TestParameter::kEnableRendererAppContainer>(GetParam()))
    enabled_features.push_back(features::kRendererAppContainer);
  else
    disabled_features.push_back(features::kRendererAppContainer);

  if (::testing::get<TestParameter::kEnableKtmMitigation>(GetParam()))
    enabled_features.push_back(features::kWinSboxDisableKtmComponent);
  else
    disabled_features.push_back(features::kWinSboxDisableKtmComponent);

  feature_list_.InitWithFeatures(enabled_features, disabled_features);
}

IntegrityLevel SandboxFeatureTest::GetExpectedIntegrityLevel() {
  return IntegrityLevel::INTEGRITY_LEVEL_LOW;
}

TokenLevel SandboxFeatureTest::GetExpectedLockdownTokenLevel() {
  return TokenLevel::USER_LOCKDOWN;
}

TokenLevel SandboxFeatureTest::GetExpectedInitialTokenLevel() {
  return TokenLevel::USER_RESTRICTED_SAME_ACCESS;
}

MitigationFlags SandboxFeatureTest::GetExpectedMitigationFlags() {
  // Mitigation flags are set on the policy regardless of the OS version
  ::sandbox::MitigationFlags flags =
      ::sandbox::MITIGATION_HEAP_TERMINATE |
      ::sandbox::MITIGATION_BOTTOM_UP_ASLR | ::sandbox::MITIGATION_DEP |
      ::sandbox::MITIGATION_DEP_NO_ATL_THUNK |
      ::sandbox::MITIGATION_EXTENSION_POINT_DISABLE |
      ::sandbox::MITIGATION_SEHOP |
      ::sandbox::MITIGATION_NONSYSTEM_FONT_DISABLE |
      ::sandbox::MITIGATION_IMAGE_LOAD_NO_REMOTE |
      ::sandbox::MITIGATION_IMAGE_LOAD_NO_LOW_LABEL |
      ::sandbox::MITIGATION_RESTRICT_INDIRECT_BRANCH_PREDICTION;

#if !defined(NACL_WIN64)
  // Win32k mitigation is only set on the operating systems it's available on
  if (base::win::GetVersion() >= base::win::Version::WIN8)
    flags = flags | ::sandbox::MITIGATION_WIN32K_DISABLE;
#endif

  if (::testing::get<TestParameter::kEnableKtmMitigation>(GetParam()))
    flags = flags | ::sandbox::MITIGATION_KTM_COMPONENT;

  return flags;
}

MitigationFlags SandboxFeatureTest::GetExpectedDelayedMitigationFlags() {
  return ::sandbox::MITIGATION_DLL_SEARCH_ORDER |
         ::sandbox::MITIGATION_FORCE_MS_SIGNED_BINS;
}

AppContainerType SandboxFeatureTest::GetExpectedAppContainerType() {
  return AppContainerType::kNone;
}

std::vector<base::win::Sid> SandboxFeatureTest::GetExpectedCapabilities() {
  return {};
}

void SandboxFeatureTest::ValidateSecurityLevels(TargetPolicy* policy) {
  EXPECT_EQ(policy->GetIntegrityLevel(), GetExpectedIntegrityLevel());
  EXPECT_EQ(policy->GetLockdownTokenLevel(), GetExpectedLockdownTokenLevel());
  EXPECT_EQ(policy->GetInitialTokenLevel(), GetExpectedInitialTokenLevel());
}

void SandboxFeatureTest::ValidatePolicyFlagSettings(TargetPolicy* policy) {
  EXPECT_EQ(policy->GetProcessMitigations(), GetExpectedMitigationFlags());
  EXPECT_EQ(policy->GetDelayedProcessMitigations(),
            GetExpectedDelayedMitigationFlags());
}

void SandboxFeatureTest::ValidateAppContainerSettings(TargetPolicy* policy) {
  if (GetExpectedAppContainerType() == ::sandbox::AppContainerType::kLowbox) {
    EXPECT_EQ(GetExpectedAppContainerType(),
              policy->GetAppContainer()->GetAppContainerType());

    EqualSidList(policy->GetAppContainer()->GetCapabilities(),
                 GetExpectedCapabilities());
  } else {
    EXPECT_EQ(policy->GetAppContainer().get(), nullptr);
  }
}
}  // namespace policy
}  // namespace sandbox
