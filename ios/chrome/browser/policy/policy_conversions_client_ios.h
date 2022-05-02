// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_POLICY_CONVERSIONS_CLIENT_IOS_H_
#define IOS_CHROME_BROWSER_POLICY_POLICY_CONVERSIONS_CLIENT_IOS_H_

#include "components/policy/core/browser/policy_conversions_client.h"

class ChromeBrowserState;

namespace web {
class BrowserState;
}

// PolicyConversionsClientIOS provides an implementation of the
// PolicyConversionsClient interface that is based on ChromeBrowserState and is
// suitable for use in //ios/chrome.
class PolicyConversionsClientIOS : public policy::PolicyConversionsClient {
 public:
  // Creates a PolicyConversionsClientIOS which retrieves BrowserState-specific
  // policy information from the given |browser_state|.
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
  base::Value GetExtensionPolicies(policy::PolicyDomain policy_domain) override;

 private:
  ChromeBrowserState* browser_state_;
};

#endif  // IOS_CHROME_BROWSER_POLICY_POLICY_CONVERSIONS_CLIENT_IOS_H_
