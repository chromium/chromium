// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/policy_conversions_client_ios.h"

#import "base/check.h"
#import "base/values.h"
#import "components/policy/core/browser/policy_conversions_client.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/browser_state_policy_connector.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

PolicyConversionsClientIOS::PolicyConversionsClientIOS(
    web::BrowserState* browser_state) {
  DCHECK(browser_state);
  profile_ = ProfileIOS::FromBrowserState(
      GetBrowserStateRedirectedInIncognito(browser_state));
}

PolicyConversionsClientIOS::~PolicyConversionsClientIOS() = default;

policy::PolicyService* PolicyConversionsClientIOS::GetPolicyService() const {
  return profile_->GetPolicyConnector()->GetPolicyService();
}

policy::SchemaRegistry* PolicyConversionsClientIOS::GetPolicySchemaRegistry()
    const {
  return profile_->GetPolicyConnector()->GetSchemaRegistry();
}

const policy::ConfigurationPolicyHandlerList*
PolicyConversionsClientIOS::GetHandlerList() const {
  return GetApplicationContext()->GetBrowserPolicyConnector()->GetHandlerList();
}

bool PolicyConversionsClientIOS::HasUserPolicies() const {
  return profile_ != nullptr;
}

base::Value::List PolicyConversionsClientIOS::GetExtensionPolicies(
    policy::PolicyDomain policy_domain) {
  // Return an empty list since extensions are not supported on iOS.
  return base::Value::List();
}
