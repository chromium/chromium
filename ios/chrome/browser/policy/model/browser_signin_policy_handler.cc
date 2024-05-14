// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/model/browser_signin_policy_handler.h"

#include <memory>

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
  if (!value)
    return true;

  if (!SchemaValidatingPolicyHandler::CheckPolicySettings(policies, errors))
    return false;

  return true;
}

void BrowserSigninPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::INTEGER);
  if (!value)
    return;

  const int int_value = value->GetInt();
  if (static_cast<int>(BrowserSigninMode::kDisabled) > int_value ||
      static_cast<int>(BrowserSigninMode::kForced) < int_value) {
    SYSLOG(ERROR) << "Unexpected value for BrowserSigninMode: " << int_value;
    NOTREACHED_IN_MIGRATION();
    return;
  }

  prefs->SetInteger(prefs::kBrowserSigninPolicy, int_value);
}

}  // namespace policy
