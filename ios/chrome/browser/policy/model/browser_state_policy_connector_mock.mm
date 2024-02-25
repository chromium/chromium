// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/browser_state_policy_connector_mock.h"

#import "components/policy/core/common/policy_service_impl.h"
#import "components/policy/core/common/schema_registry.h"

BrowserStatePolicyConnectorMock::BrowserStatePolicyConnectorMock(
    std::unique_ptr<policy::PolicyService> policy_service,
    policy::SchemaRegistry* schema_registry) {
  policy_service_ = std::move(policy_service);
  schema_registry_ = schema_registry;
}

BrowserStatePolicyConnectorMock::~BrowserStatePolicyConnectorMock() = default;
