// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_SIGNIN_POLICY_HANDLER_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_SIGNIN_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// Policy handler for the BrowserSignin policy.
class BrowserSigninPolicyHandler : public SchemaValidatingPolicyHandler {
 public:
  explicit BrowserSigninPolicyHandler(Schema chrome_schema);
  BrowserSigninPolicyHandler(const BrowserSigninPolicyHandler&) = delete;
  BrowserSigninPolicyHandler& operator=(const BrowserSigninPolicyHandler&) =
      delete;
  ~BrowserSigninPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_BROWSER_SIGNIN_POLICY_HANDLER_H_
