// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/model/supervised_user_service_platform_delegate.h"

#import "components/prefs/pref_service.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/common/channel_info.h"

SupervisedUserServicePlatformDelegate::SupervisedUserServicePlatformDelegate(
    PrefService& pref_service,
    BrowserList& browser_list)
    : pref_service_(pref_service), browser_list_(browser_list) {}

std::string SupervisedUserServicePlatformDelegate::GetCountryCode() const {
  std::string country;
  variations::VariationsService* variations_service =
      GetApplicationContext()->GetVariationsService();
  if (variations_service) {
    country = variations_service->GetStoredPermanentCountry();
    if (country.empty()) {
      country = variations_service->GetLatestCountry();
    }
  }
  return country;
}

version_info::Channel SupervisedUserServicePlatformDelegate::GetChannel()
    const {
  return ::GetChannel();
}

bool SupervisedUserServicePlatformDelegate::ShouldCloseIncognitoTabs() const {
  return IsIncognitoModeDisabled(&pref_service_.get());
}

void SupervisedUserServicePlatformDelegate::CloseIncognitoTabs() {
  for (Browser* browser :
       browser_list_->BrowsersOfType(BrowserList::BrowserType::kIncognito)) {
    CloseAllWebStates(*browser->GetWebStateList(),
                      WebStateList::ClosingReason::kUserAction);
  }
}
