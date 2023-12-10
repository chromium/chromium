// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_WIN_SANDBOX_POLICY_FEATURE_TEST_H_
#define SANDBOX_POLICY_WIN_SANDBOX_POLICY_FEATURE_TEST_H_

#include "base/test/scoped_feature_list.h"
#include "base/win/sid.h"
#include "sandbox/win/src/app_container.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/src/security_level.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox::policy {

class SandboxFeatureTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          ::testing::tuple</* renderer app container feature */ bool,
                           /* ktm mitigation feature */ bool>> {
 public:
  enum TestParameter { kEnableRendererAppContainer, kEnableKtmMitigation };

  SandboxFeatureTest();

  virtual IntegrityLevel GetExpectedIntegrityLevel();
  virtual TokenLevel GetExpectedLockdownTokenLevel();
  virtual TokenLevel GetExpectedInitialTokenLevel();

  virtual MitigationFlags GetExpectedMitigationFlags();
  virtual MitigationFlags GetExpectedDelayedMitigationFlags();

  virtual AppContainerType GetExpectedAppContainerType();
  virtual std::vector<base::win::Sid> GetExpectedCapabilities();

  void ValidateSecurityLevels(TargetConfig* config);
  void ValidatePolicyFlagSettings(TargetConfig* config);
  void ValidateAppContainerSettings(TargetConfig* config);

  base::test::ScopedFeatureList feature_list_;
};

}  // namespace sandbox::policy

#endif  // SANDBOX_POLICY_WIN_SANDBOX_POLICY_FEATURE_TEST_H_
