// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MODEL_MOCK_IOS_CONTEXTUAL_SEARCH_SERVICE_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MODEL_MOCK_IOS_CONTEXTUAL_SEARCH_SERVICE_H_

#import "components/contextual_search/mock_contextual_search_context_controller.h"
#import "components/lens/lens_overlay_invocation_source.h"
#import "ios/chrome/browser/composebox/model/ios_contextual_search_service.h"
#import "testing/gmock/include/gmock/gmock.h"

class ProfileIOS;

// Mock implementation of IOSContextualSearchService.
class MockIOSContextualSearchService : public IOSContextualSearchService {
 public:
  MockIOSContextualSearchService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      TemplateURLService* template_url_service,
      variations::VariationsClient* variations_client,
      version_info::Channel channel,
      const std::string& locale);
  ~MockIOSContextualSearchService() override;

  // Factory method to create a testing instance for the given profile.
  static std::unique_ptr<MockIOSContextualSearchService>
  CreateTestingProfileService(ProfileIOS* profile);

  MOCK_METHOD(
      std::unique_ptr<contextual_search::ContextualSearchContextController>,
      CreateComposeboxQueryController,
      (std::unique_ptr<
          contextual_search::ContextualSearchContextController::ConfigParams>),
      (override));

  MOCK_METHOD(
      std::unique_ptr<contextual_search::ContextualSearchSessionHandle>,
      CreateSession,
      (std::unique_ptr<
           contextual_search::ContextualSearchContextController::ConfigParams>,
       contextual_search::ContextualSearchSource,
       std::optional<lens::LensOverlayInvocationSource>),
      (override));
};

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MODEL_MOCK_IOS_CONTEXTUAL_SEARCH_SERVICE_H_
