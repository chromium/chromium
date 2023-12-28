// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/restrict_accounts_policy_handler.h"

#include <memory>

#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_map.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/signin/model/pattern_account_restriction.h"

namespace policy {

RestrictAccountsPolicyHandler::RestrictAccountsPolicyHandler(
    Schema chrome_schema)
    : SchemaValidatingPolicyHandler(
          key::kRestrictAccountsToPatterns,
          chrome_schema.GetKnownProperty(key::kRestrictAccountsToPatterns),
          SCHEMA_ALLOW_UNKNOWN_AND_INVALID_LIST_ENTRY) {}

RestrictAccountsPolicyHandler::~RestrictAccountsPolicyHandler() {}

bool RestrictAccountsPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  // `GetValueUnsafe` is used to differentiate between the policy value being
  // unset vs being set with an incorrect type.
  const base::Value* value = policies.GetValueUnsafe(policy_name());
  if (!value)
    return true;
  if (!ArePatternsValid(value)) {
    errors->AddError(policy_name(),
                     IDS_POLICY_INVALID_ACCOUNT_PATTERN_FORMAT_ERROR);
  }
  if (!value->is_list()) {
    LOG_POLICY(ERROR, POLICY_PROCESSING)
        << "RestrictAccountsToPatterns Policy Error: value must be a list";
    return false;
  }
  // Even if the pattern is not valid, the policy should be applied.
  return true;
}

void RestrictAccountsPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::LIST);
  if (value)
    prefs->SetValue(prefs::kRestrictAccountsToPatterns, value->Clone());
}

}  // namespace policy
