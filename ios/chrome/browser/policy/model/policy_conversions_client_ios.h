// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_POLICY_CONVERSIONS_CLIENT_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_POLICY_CONVERSIONS_CLIENT_IOS_H_

#import "base/memory/raw_ptr.h"
#import "components/policy/core/browser/policy_conversions_client.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace web {
class BrowserState;
}

// PolicyConversionsClientIOS provides an implementation of the
// PolicyConversionsClient interface that is based on Profile and is
// suitable for use in //ios/chrome.
class PolicyConversionsClientIOS : public policy::PolicyConversionsClient {
 public:
  // Creates a PolicyConversionsClientIOS which retrieves BrowserState-specific
  // policy information from the given `browser_state`.
  explicit PolicyConversionsClientIOS(web::BrowserState* browser_state);

  PolicyConversionsClientIOS(const PolicyConversionsClientIOS&) = delete;
  PolicyConversionsClientIOS& operator=(const PolicyConversionsClientIOS&) =
      delete;
  ~PolicyConversionsClientIOS() override;

  // PolicyConversionsClient.
  policy::PolicyService* GetPolicyService() const override;
  policy::SchemaRegistry* GetPolicySchemaRegistry() const override;
  const policy::ConfigurationPolicyHandlerList* GetHandlerList() const override;
  bool HasUserPolicies() const override;
  base::Value::List GetExtensionPolicies(
      policy::PolicyDomain policy_domain) override;

 private:
  raw_ptr<ProfileIOS> profile_;
};

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_POLICY_CONVERSIONS_CLIENT_IOS_H_
