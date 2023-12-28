// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_NEW_TAB_PAGE_LOCATION_POLICY_HANDLER_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_NEW_TAB_PAGE_LOCATION_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// Policy handler for the NewTabPageLocation policy.
class NewTabPageLocationPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  explicit NewTabPageLocationPolicyHandler();
  NewTabPageLocationPolicyHandler(const NewTabPageLocationPolicyHandler&) =
      delete;
  NewTabPageLocationPolicyHandler& operator=(
      const NewTabPageLocationPolicyHandler&) = delete;
  ~NewTabPageLocationPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  // Format the New Tab Page Location URL string to be the correct format of a
  // URL.
  std::string FormatNewTabPageLocationURL(const std::string& ntp_location);

  // Verifies that the value is a valid URL.
  bool ValidateNewTabPageLocationURL(const base::Value* value);
};

}  // namespace policy

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_NEW_TAB_PAGE_LOCATION_POLICY_HANDLER_H_
