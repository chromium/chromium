// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/ios_rules_service.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace data_controls {

IOSRulesService::IOSRulesService(ProfileIOS* profile)
    : RulesServiceBase(profile->GetPrefs()), profile_(profile) {}

IOSRulesService::~IOSRulesService() = default;

bool IOSRulesService::incognito_profile() const {
  return profile_->IsOffTheRecord();
}

}  // namespace data_controls
