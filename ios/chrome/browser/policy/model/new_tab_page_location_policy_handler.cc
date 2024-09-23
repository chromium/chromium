// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/model/new_tab_page_location_policy_handler.h"

#include "base/strings/strcat.h"
#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#include "url/gurl.h"

class GURL;

namespace policy {

NewTabPageLocationPolicyHandler::NewTabPageLocationPolicyHandler()
    : TypeCheckingPolicyHandler(key::kNewTabPageLocation,
                                base::Value::Type::STRING) {}

NewTabPageLocationPolicyHandler::~NewTabPageLocationPolicyHandler() {}

std::string NewTabPageLocationPolicyHandler::FormatNewTabPageLocationURL(
    const std::string& ntp_location) {
  url::Component scheme;
  if (!ntp_location.empty() &&
      !url::ExtractScheme(ntp_location.data(),
                          static_cast<int>(ntp_location.length()), &scheme)) {
    return base::StrCat(
        {url::kHttpsScheme, url::kStandardSchemeSeparator, ntp_location});
  }
  return ntp_location;
}

bool NewTabPageLocationPolicyHandler::ValidateNewTabPageLocationURL(
    const base::Value* value) {
  if (value) {
    std::string ntp_location = value->GetString();
    if (ntp_location.empty())
      return false;
    ntp_location = NewTabPageLocationPolicyHandler::FormatNewTabPageLocationURL(
        ntp_location);
    return GURL(ntp_location).is_valid();
  }
  return false;
}

bool NewTabPageLocationPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  if (!TypeCheckingPolicyHandler::CheckPolicySettings(policies, errors))
    return false;
  // `GetValueUnsafe` is used to differentiate between the policy value being
  // unset vs being set with an incorrect type.
  const base::Value* value = policies.GetValueUnsafe(policy_name());
  if (!value) {
    return true;
  }
  if (NewTabPageLocationPolicyHandler::ValidateNewTabPageLocationURL(value)) {
    return true;
  }
  errors->AddError(policy_name(), IDS_POLICY_INVALID_URL_ERROR);
  return false;
}

void NewTabPageLocationPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::STRING);
  if (value) {
    std::string ntp_location =
        NewTabPageLocationPolicyHandler::FormatNewTabPageLocationURL(
            value->GetString());
    prefs->SetValue(prefs::kNewTabPageLocationOverride,
                    base::Value(ntp_location));
  }
}

}  // namespace policy
