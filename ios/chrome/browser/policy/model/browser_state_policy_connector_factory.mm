// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/browser_state_policy_connector_factory.h"

#import "base/check.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "ios/chrome/browser/policy/model/browser_state_policy_connector.h"

std::unique_ptr<BrowserStatePolicyConnector> BuildBrowserStatePolicyConnector(
    policy::SchemaRegistry* schema_registry,
    BrowserPolicyConnectorIOS* browser_policy_connector,
    policy::UserCloudPolicyManager* user_policy_manager) {
  auto connector = std::make_unique<BrowserStatePolicyConnector>();

  // Since extensions are not supported on iOS, the `schema_registry` here has
  // the same registered components as the registry owned by
  // `browser_policy_connector`, despite being a separate instance. The two
  // levels of registry (owned by ApplicationContext vs owned by BrowserState)
  // are maintained to keep a parallel structure with Desktop.
  connector->Init(schema_registry, browser_policy_connector,
                  user_policy_manager);
  return connector;
}
