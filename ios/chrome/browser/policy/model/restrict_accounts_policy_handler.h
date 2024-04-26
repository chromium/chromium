// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_RESTRICT_ACCOUNTS_POLICY_HANDLER_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_RESTRICT_ACCOUNTS_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// Policy handler for the RestrictAccountsToPattern policy.
// TODO(crbug.com/40205573): Move this to components.
class RestrictAccountsPolicyHandler : public SchemaValidatingPolicyHandler {
 public:
  explicit RestrictAccountsPolicyHandler(Schema chrome_schema);
  RestrictAccountsPolicyHandler(const RestrictAccountsPolicyHandler&) = delete;
  RestrictAccountsPolicyHandler& operator=(
      const RestrictAccountsPolicyHandler&) = delete;
  ~RestrictAccountsPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_RESTRICT_ACCOUNTS_POLICY_HANDLER_H_
