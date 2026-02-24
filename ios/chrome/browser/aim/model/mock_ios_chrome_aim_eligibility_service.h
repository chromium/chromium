// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_MODEL_MOCK_IOS_CHROME_AIM_ELIGIBILITY_SERVICE_H_
#define IOS_CHROME_BROWSER_AIM_MODEL_MOCK_IOS_CHROME_AIM_ELIGIBILITY_SERVICE_H_

#import <memory>

#import "components/omnibox/browser/mock_aim_eligibility_service.h"

class ProfileIOS;

// Mock implementation of IOSChromeAimEligibilityService.
class MockIOSChromeAimEligibilityService : public MockAimEligibilityService {
 public:
  MockIOSChromeAimEligibilityService(
      PrefService& pref_service,
      TemplateURLService* template_url_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager,
      Configuration configuration = {});
  ~MockIOSChromeAimEligibilityService() override;

  // Factory method to create a testing instance for the given profile.
  static std::unique_ptr<MockIOSChromeAimEligibilityService>
  CreateTestingProfileService(ProfileIOS* profile);
};

#endif  // IOS_CHROME_BROWSER_AIM_MODEL_MOCK_IOS_CHROME_AIM_ELIGIBILITY_SERVICE_H_
