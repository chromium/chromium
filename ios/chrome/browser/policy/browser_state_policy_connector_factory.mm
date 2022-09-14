// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/browser_state_policy_connector_factory.h"

#import "base/check.h"
#import "ios/chrome/browser/policy/browser_state_policy_connector.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

std::unique_ptr<BrowserStatePolicyConnector> BuildBrowserStatePolicyConnector(
    policy::SchemaRegistry* schema_registry,
    BrowserPolicyConnectorIOS* browser_policy_connector,
    policy::ConfigurationPolicyProvider* user_policy_provider) {
  auto connector = std::make_unique<BrowserStatePolicyConnector>();

  // Since extensions are not supported on iOS, the `schema_registry` here has
  // the same registered components as the registry owned by
  // `browser_policy_connector`, despite being a separate instance. The two
  // levels of registry (owned by ApplicationContext vs owned by BrowserState)
  // are maintained to keep a parallel structure with Desktop.
  connector->Init(schema_registry, browser_policy_connector,
                  user_policy_provider);
  return connector;
}
