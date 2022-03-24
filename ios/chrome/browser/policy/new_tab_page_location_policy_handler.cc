// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/new_tab_page_location_policy_handler.h"

#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/pref_names.h"
#include "url/gurl.h"

class GURL;

namespace policy {

NewTabPageLocationPolicyHandler::NewTabPageLocationPolicyHandler()
    : TypeCheckingPolicyHandler(key::kNewTabPageLocation,
                                base::Value::Type::STRING) {}

NewTabPageLocationPolicyHandler::~NewTabPageLocationPolicyHandler() {}

bool NewTabPageLocationPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  if (!TypeCheckingPolicyHandler::CheckPolicySettings(policies, errors))
    return false;
  // |GetValueUnsafe| is used to differentiate between the policy value being
  // unset vs being set with an incorrect type.
  const base::Value* value = policies.GetValueUnsafe(policy_name());
  if (value && !GURL(value->GetString()).is_valid()) {
    errors->AddError(policy_name(), IDS_POLICY_VALUE_FORMAT_ERROR);
    return false;
  }
  return true;
}

void NewTabPageLocationPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::STRING);
  if (value) {
    prefs->SetValue(prefs::kNewTabPageLocationOverride, value->Clone());
  }
}

}  // namespace policy
