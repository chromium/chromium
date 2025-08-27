// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/ios_rules_service.h"

#import "components/enterprise/data_controls/core/browser/rule.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace data_controls {

IOSRulesService::IOSRulesService(ProfileIOS* profile)
    : RulesServiceBase(profile->GetPrefs()), profile_(profile) {}

IOSRulesService::~IOSRulesService() = default;

Verdict IOSRulesService::GetPasteVerdict(
    const ActionContext& source_context,
    const ActionContext& destionation_context,
    ProfileIOS* source_profile,
    ProfileIOS* destination_profile) {
  return GetVerdict(
      Rule::Restriction::kClipboard,
      {
          .source = GetAsActionSource(source_context, source_profile),
          .destination =
              GetAsActionDestination(destionation_context, destination_profile),
      });
}

bool IOSRulesService::incognito_profile() const {
  return profile_->IsOffTheRecord();
}

ActionSource IOSRulesService::GetAsActionSource(
    const ActionContext& source_context,
    ProfileIOS* source_profile) const {
  if (!source_profile) {
    return {.os_clipboard = true};
  }

  return {
      .url = source_context.source.url,
      .incognito = source_profile->IsOffTheRecord(),
      .other_profile = source_profile != profile_,
  };
}

ActionDestination IOSRulesService::GetAsActionDestination(
    const ActionContext& destination_context,
    ProfileIOS* destination_profile) const {
  ActionDestination action_destination;
  action_destination.url = destination_context.destination.url;

  if (destination_profile) {
    action_destination.incognito = destination_profile->IsOffTheRecord();
    action_destination.other_profile = destination_profile != profile_;
  }

  return action_destination;
}

}  // namespace data_controls
