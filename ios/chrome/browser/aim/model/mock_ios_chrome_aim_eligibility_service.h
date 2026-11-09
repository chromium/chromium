// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_MODEL_MOCK_IOS_CHROME_AIM_ELIGIBILITY_SERVICE_H_
#define IOS_CHROME_BROWSER_AIM_MODEL_MOCK_IOS_CHROME_AIM_ELIGIBILITY_SERVICE_H_

#import <memory>

#import "base/callback_list.h"
#import "components/omnibox/browser/mock_aim_eligibility_service.h"
#import "third_party/omnibox_proto/tool_mode.pb.h"

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

  // Enables or disables `tool` in the mock `SearchboxConfig`, updates the
  // matching tool eligibility expectation when applicable, and notifies
  // registered eligibility observers.
  void SetToolEnabled(omnibox::ToolMode tool, bool enabled);

  // Restores the default set of server tools (`TOOL_MODE_IMAGE_GEN`,
  // `TOOL_MODE_CANVAS`, and `TOOL_MODE_DEEP_SEARCH`) in the mock
  // `SearchboxConfig` and notifies registered eligibility observers.
  void ResetDefaultTools();

 private:
  base::RepeatingClosureList mock_eligibility_changed_callbacks_;
};

#endif  // IOS_CHROME_BROWSER_AIM_MODEL_MOCK_IOS_CHROME_AIM_ELIGIBILITY_SERVICE_H_
