// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_TEST_MANAGEMENT_POLICY_H_
#define EXTENSIONS_BROWSER_TEST_MANAGEMENT_POLICY_H_

#include <string>

#include "extensions/browser/management_policy.h"

namespace extensions {

// This class provides a simple way to create providers with specific
// restrictions and a known error message, for use in testing.
class TestManagementPolicyProvider : public ManagementPolicy::Provider {
 public:
  enum AllowedActionFlag {
    ALLOW_ALL = 0,
    PROHIBIT_LOAD = 1 << 0,
    PROHIBIT_MODIFY_STATUS = 1 << 1,
    MUST_REMAIN_ENABLED = 1 << 2,
    MUST_REMAIN_DISABLED = 1 << 3,
    MUST_REMAIN_INSTALLED = 1 << 4,
  };

  static std::string expected_error() {
    return "Action prohibited by test provider.";
  }

  TestManagementPolicyProvider();
  explicit TestManagementPolicyProvider(int prohibited_actions);

  void SetProhibitedActions(int prohibited_actions);
  void SetDisableReason(disable_reason::DisableReason reason);

  std::string GetDebugPolicyProviderName() const override;

  bool UserMayLoad(const Extension* extension,
                   std::u16string* error) const override;

  bool UserMayModifySettings(const Extension* extension,
                             std::u16string* error) const override;

  bool ExtensionMayModifySettings(const Extension* source_extension,
                                  const Extension* extension,
                                  std::u16string* error) const override;

  bool MustRemainEnabled(const Extension* extension,
                         std::u16string* error) const override;

  bool MustRemainDisabled(const Extension* extension,
                          disable_reason::DisableReason* reason,
                          std::u16string* error) const override;

  bool MustRemainInstalled(const Extension* extension,
                           std::u16string* error) const override;

 private:
  bool may_load_;
  bool may_modify_status_;
  bool must_remain_enabled_;
  bool must_remain_disabled_;
  bool must_remain_installed_;
  disable_reason::DisableReason disable_reason_;

  std::u16string error_message_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_TEST_MANAGEMENT_POLICY_H_
