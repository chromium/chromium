// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/win/sandbox_policy_feature_test.h"

#include "sandbox/policy/features.h"

namespace sandbox {
namespace policy {

SandboxFeatureTest::SandboxFeatureTest() {
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;

  if (::testing::get<TestParameter::kEnableRendererAppContainer>(GetParam()))
    enabled_features.push_back(features::kRendererAppContainer);
  else
    disabled_features.push_back(features::kRendererAppContainer);

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
      ::sandbox::MITIGATION_BOTTOM_UP_ASLR | ::sandbox::MITIGATION_DEP |
      ::sandbox::MITIGATION_DEP_NO_ATL_THUNK |
      ::sandbox::MITIGATION_EXTENSION_POINT_DISABLE |
      ::sandbox::MITIGATION_FSCTL_DISABLED |
      ::sandbox::MITIGATION_HEAP_TERMINATE |
      ::sandbox::MITIGATION_IMAGE_LOAD_NO_LOW_LABEL |
      ::sandbox::MITIGATION_IMAGE_LOAD_NO_REMOTE |
      ::sandbox::MITIGATION_KTM_COMPONENT |
      ::sandbox::MITIGATION_NONSYSTEM_FONT_DISABLE |
      ::sandbox::MITIGATION_RESTRICT_INDIRECT_BRANCH_PREDICTION |
      ::sandbox::MITIGATION_SEHOP | ::sandbox::MITIGATION_WIN32K_DISABLE;

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

void SandboxFeatureTest::ValidateSecurityLevels(TargetConfig* config) {
  EXPECT_EQ(config->GetIntegrityLevel(), GetExpectedIntegrityLevel());
  EXPECT_EQ(config->GetLockdownTokenLevel(), GetExpectedLockdownTokenLevel());
  EXPECT_EQ(config->GetInitialTokenLevel(), GetExpectedInitialTokenLevel());
}

void SandboxFeatureTest::ValidatePolicyFlagSettings(TargetConfig* config) {
  EXPECT_EQ(config->GetProcessMitigations(), GetExpectedMitigationFlags());
  EXPECT_EQ(config->GetDelayedProcessMitigations(),
            GetExpectedDelayedMitigationFlags());
}

void SandboxFeatureTest::ValidateAppContainerSettings(TargetConfig* config) {
  if (GetExpectedAppContainerType() == ::sandbox::AppContainerType::kLowbox) {
    EXPECT_EQ(GetExpectedAppContainerType(),
              config->GetAppContainer()->GetAppContainerType());

    EXPECT_EQ(config->GetAppContainer()->GetCapabilities(),
              GetExpectedCapabilities());
  } else {
    EXPECT_EQ(config->GetAppContainer(), nullptr);
  }
}
}  // namespace policy
}  // namespace sandbox
