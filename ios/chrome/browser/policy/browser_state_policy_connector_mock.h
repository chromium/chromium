// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_BROWSER_STATE_POLICY_CONNECTOR_MOCK_H_
#define IOS_CHROME_BROWSER_POLICY_BROWSER_STATE_POLICY_CONNECTOR_MOCK_H_

#include <memory>

#include "ios/chrome/browser/policy/browser_state_policy_connector.h"

namespace policy {
class PolicyService;
class SchemaRegistry;
}  // namespace policy

// BrowserStatePolicyConnector creates and manages the per-BrowserState policy
// components and their integration with PrefService.
class BrowserStatePolicyConnectorMock : public BrowserStatePolicyConnector {
 public:
  BrowserStatePolicyConnectorMock(
      std::unique_ptr<policy::PolicyService> policy_service,
      policy::SchemaRegistry* schema_registry);
  ~BrowserStatePolicyConnectorMock();
  BrowserStatePolicyConnectorMock(const BrowserStatePolicyConnectorMock&) =
      delete;
  BrowserStatePolicyConnectorMock& operator=(
      const BrowserStatePolicyConnectorMock&) = delete;
};

#endif  // IOS_CHROME_BROWSER_POLICY_BROWSER_STATE_POLICY_CONNECTOR_MOCK_H_
