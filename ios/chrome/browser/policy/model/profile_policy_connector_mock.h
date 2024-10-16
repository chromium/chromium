// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_PROFILE_POLICY_CONNECTOR_MOCK_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_PROFILE_POLICY_CONNECTOR_MOCK_H_

#include <memory>

#include "ios/chrome/browser/policy/model/profile_policy_connector.h"

namespace policy {
class PolicyService;
class SchemaRegistry;
}  // namespace policy

// ProfilePolicyConnector creates and manages the per-BrowserState policy
// components and their integration with PrefService.
// ProfilePolicyConnector isn't a keyed service because the pref service,
// which isn't a keyed service, has a hard dependency on the policy
// infrastructure. In order to outlive the pref service, the policy connector
// must live outside the keyed services.
class ProfilePolicyConnectorMock : public ProfilePolicyConnector {
 public:
  ProfilePolicyConnectorMock(
      std::unique_ptr<policy::PolicyService> policy_service,
      policy::SchemaRegistry* schema_registry);
  ~ProfilePolicyConnectorMock();
  ProfilePolicyConnectorMock(const ProfilePolicyConnectorMock&) = delete;
  ProfilePolicyConnectorMock& operator=(const ProfilePolicyConnectorMock&) =
      delete;
};

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_PROFILE_POLICY_CONNECTOR_MOCK_H_
