// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/policy_conversions_client_ios.h"

#import "base/check.h"
#import "base/values.h"
#import "components/policy/core/browser/policy_conversions_client.h"
#import "ios/chrome/browser/policy/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/browser_state_policy_connector.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PolicyConversionsClientIOS::PolicyConversionsClientIOS(
    web::BrowserState* browser_state) {
  DCHECK(browser_state);
  browser_state_ = ChromeBrowserState::FromBrowserState(
      GetBrowserStateRedirectedInIncognito(browser_state));
}

PolicyConversionsClientIOS::~PolicyConversionsClientIOS() = default;

policy::PolicyService* PolicyConversionsClientIOS::GetPolicyService() const {
  return browser_state_->GetPolicyConnector()->GetPolicyService();
}

policy::SchemaRegistry* PolicyConversionsClientIOS::GetPolicySchemaRegistry()
    const {
  return browser_state_->GetPolicyConnector()->GetSchemaRegistry();
}

const policy::ConfigurationPolicyHandlerList*
PolicyConversionsClientIOS::GetHandlerList() const {
  return GetApplicationContext()->GetBrowserPolicyConnector()->GetHandlerList();
}

bool PolicyConversionsClientIOS::HasUserPolicies() const {
  return browser_state_ != nullptr;
}

base::Value::List PolicyConversionsClientIOS::GetExtensionPolicies(
    policy::PolicyDomain policy_domain) {
  // Return an empty list since extensions are not supported on iOS.
  return base::Value::List();
}
