// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/mock_ios_chrome_aim_eligibility_service.h"

#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "testing/gmock/include/gmock/gmock.h"

MockIOSChromeAimEligibilityService::MockIOSChromeAimEligibilityService(
    PrefService& pref_service,
    TemplateURLService* template_url_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    Configuration configuration)
    : MockAimEligibilityService(pref_service,
                                template_url_service,
                                url_loader_factory,
                                identity_manager,
                                std::move(configuration)) {
  ON_CALL(*this, IsAimEligible).WillByDefault(testing::Return(true));
  ON_CALL(*this, IsCreateImagesEligible).WillByDefault(testing::Return(true));
  ON_CALL(*this, IsAimLocallyEligible).WillByDefault(testing::Return(true));
  ON_CALL(*this, IsServerEligibilityEnabled)
      .WillByDefault(testing::Return(false));
}

MockIOSChromeAimEligibilityService::~MockIOSChromeAimEligibilityService() =
    default;

// static
std::unique_ptr<MockIOSChromeAimEligibilityService>
MockIOSChromeAimEligibilityService::CreateTestingProfileService(
    ProfileIOS* profile) {
  Configuration config;
  config.is_off_the_record = profile->IsOffTheRecord();
  return std::make_unique<MockIOSChromeAimEligibilityService>(
      *profile->GetPrefs(),
      ios::TemplateURLServiceFactory::GetForProfile(profile),
      profile->GetSharedURLLoaderFactory(),
      IdentityManagerFactory::GetForProfile(profile), std::move(config));
}
