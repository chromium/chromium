// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/model/browser_signin_policy_handler.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/syslog_logging.h"
#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

namespace policy {
BrowserSigninPolicyHandler::BrowserSigninPolicyHandler(Schema chrome_schema)
    : SchemaValidatingPolicyHandler(
          key::kBrowserSignin,
          chrome_schema.GetKnownProperty(key::kBrowserSignin),
          SCHEMA_ALLOW_UNKNOWN) {}

BrowserSigninPolicyHandler::~BrowserSigninPolicyHandler() {}

bool BrowserSigninPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  // `GetValueUnsafe` is used to differentiate between the policy value being
  // unset vs being set with an incorrect type.
  const base::Value* value = policies.GetValueUnsafe(policy_name());
  if (!value) {
    return true;
  }

  if (!SchemaValidatingPolicyHandler::CheckPolicySettings(policies, errors)) {
    return false;
  }

  const PolicyMap::Entry* entry = policies.Get(policy_name());
  if (entry && entry->scope == POLICY_SCOPE_USER && errors) {
    errors->AddError(policy_name(),
                     IDS_POLICY_BROWSER_SIGNIN_USER_SCOPE_WARNING,
                     /*error_path=*/{}, PolicyMap::MessageType::kWarning);
  }

  return true;
}

void BrowserSigninPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
  if (!value) {
    last_recorded_user_scope_value_.reset();
    return;
  }

  const int int_value = value->GetInt();
  if (static_cast<int>(BrowserSigninMode::kDisabled) > int_value ||
      static_cast<int>(BrowserSigninMode::kForced) < int_value) {
    NOTREACHED() << "Unexpected value for BrowserSigninMode: " << int_value;
  }

  const PolicyMap::Entry* entry = policies.Get(policy_name());
  if (entry && entry->scope == POLICY_SCOPE_USER) {
    auto mode = static_cast<BrowserSigninMode>(int_value);
    if (last_recorded_user_scope_value_ != mode) {
      last_recorded_user_scope_value_ = mode;
      base::UmaHistogramEnumeration(
          "Enterprise.BrowserSignin.UserScope.PolicyValue", mode);
    }
  } else {
    last_recorded_user_scope_value_.reset();
  }

  prefs->SetInteger(prefs::kBrowserSigninPolicy, int_value);
}

}  // namespace policy
