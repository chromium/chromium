// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/browser_state_policy_connector.h"

#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/schema_registry.h"
#include "ios/chrome/browser/policy/browser_policy_connector_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BrowserStatePolicyConnector::BrowserStatePolicyConnector() = default;
BrowserStatePolicyConnector::~BrowserStatePolicyConnector() = default;

void BrowserStatePolicyConnector::Init(
    policy::SchemaRegistry* schema_registry,
    BrowserPolicyConnectorIOS* browser_policy_connector) {
  schema_registry_ = schema_registry;

  // The object returned by GetPlatformConnector() may or may not be in the list
  // returned by GetPolicyProviders().  Explicitly add it to |policy_providers_|
  // here in case it will not be added by the loop below (for example, this
  // could happen if the platform provider is overridden for testing)..
  policy::ConfigurationPolicyProvider* platform_provider =
      browser_policy_connector->GetPlatformProvider();
  policy_providers_.push_back(platform_provider);

  for (auto* provider : browser_policy_connector->GetPolicyProviders()) {
    // Skip the platform provider since it was already handled above. Do not
    // reorder any of the remaining providers because the ordering in this list
    // determines the precedence of the providers.
    if (provider != platform_provider) {
      policy_providers_.push_back(provider);
    }
  }

  policy_service_ =
      std::make_unique<policy::PolicyServiceImpl>(policy_providers_);
}

void BrowserStatePolicyConnector::Shutdown() {}
