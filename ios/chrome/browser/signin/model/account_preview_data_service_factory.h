// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_PREVIEW_DATA_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_PREVIEW_DATA_SERVICE_FACTORY_H_

#import <memory>

#import "components/signin/core/browser/account_preview_data_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// KeyedService factory that creates the Profile-associated
// `AccountPreviewDataService`.
class AccountPreviewDataServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          AccountPreviewDataServiceFactory,
          signin::AccountPreviewDataService> {
 public:
  explicit AccountPreviewDataServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
  void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) final;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_PREVIEW_DATA_SERVICE_FACTORY_H_
