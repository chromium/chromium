// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_TEST_PLATFORM_POLICY_PROVIDER_H_
#define IOS_CHROME_BROWSER_POLICY_TEST_PLATFORM_POLICY_PROVIDER_H_

#include "components/policy/core/common/mock_configuration_policy_provider.h"

// Returns a singleton mock that can be installed as the platform policy
// provider when testing. Subsequent calls to this method will return the same
// object, which can then be used to update the current set of policies.
policy::MockConfigurationPolicyProvider* GetTestPlatformPolicyProvider();

#endif  // IOS_CHROME_BROWSER_POLICY_TEST_PLATFORM_POLICY_PROVIDER_H_
