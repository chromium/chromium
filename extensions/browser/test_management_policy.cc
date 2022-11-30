// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/test_management_policy.h"

#include "base/strings/utf_string_conversions.h"

namespace extensions {

TestManagementPolicyProvider::TestManagementPolicyProvider()
    : may_load_(true),
      may_modify_status_(true),
      must_remain_enabled_(false),
      must_remain_disabled_(false),
      must_remain_installed_(false),
      disable_reason_(disable_reason::DISABLE_NONE) {
  error_message_ = base::UTF8ToUTF16(expected_error());
}

TestManagementPolicyProvider::TestManagementPolicyProvider(
    int prohibited_actions) {
  SetProhibitedActions(prohibited_actions);
  error_message_ = base::UTF8ToUTF16(expected_error());
}

void TestManagementPolicyProvider::SetProhibitedActions(
    int prohibited_actions) {
  may_load_ = (prohibited_actions & PROHIBIT_LOAD) == 0;
  may_modify_status_ = (prohibited_actions & PROHIBIT_MODIFY_STATUS) == 0;
  must_remain_enabled_ = (prohibited_actions & MUST_REMAIN_ENABLED) != 0;
  must_remain_disabled_ = (prohibited_actions & MUST_REMAIN_DISABLED) != 0;
  must_remain_installed_ = (prohibited_actions & MUST_REMAIN_INSTALLED) != 0;
}

void TestManagementPolicyProvider::SetDisableReason(
    disable_reason::DisableReason reason) {
  disable_reason_ = reason;
}

std::string TestManagementPolicyProvider::GetDebugPolicyProviderName() const {
  return "the test management policy provider";
}

bool TestManagementPolicyProvider::UserMayLoad(const Extension* extension,
                                               std::u16string* error) const {
  if (error && !may_load_)
    *error = error_message_;
  return may_load_;
}

bool TestManagementPolicyProvider::UserMayModifySettings(
    const Extension* extension,
    std::u16string* error) const {
  if (error && !may_modify_status_)
    *error = error_message_;
  return may_modify_status_;
}

bool TestManagementPolicyProvider::ExtensionMayModifySettings(
    const Extension* source_extension,
    const Extension* extension,
    std::u16string* error) const {
  if (error && !may_modify_status_)
    *error = error_message_;
  return may_modify_status_;
}

bool TestManagementPolicyProvider::MustRemainEnabled(
    const Extension* extension,
    std::u16string* error) const {
  if (error && must_remain_enabled_)
    *error = error_message_;
  return must_remain_enabled_;
}

bool TestManagementPolicyProvider::MustRemainDisabled(
    const Extension* extension,
    disable_reason::DisableReason* reason,
    std::u16string* error) const {
  if (must_remain_disabled_) {
    if (error)
      *error = error_message_;
    if (reason)
      *reason = disable_reason_;
  }
  return must_remain_disabled_;
}

bool TestManagementPolicyProvider::MustRemainInstalled(
    const Extension* extension,
    std::u16string* error) const {
  if (error && must_remain_installed_)
    *error = error_message_;
  return must_remain_installed_;
}

}  // namespace extensions
