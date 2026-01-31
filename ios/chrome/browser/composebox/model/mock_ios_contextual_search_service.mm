// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/model/mock_ios_contextual_search_service.h"

#import "components/application_locale_storage/application_locale_storage.h"
#import "components/contextual_search/mock_contextual_search_session_handle.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/variations/model/client/variations_client_service.h"
#import "ios/chrome/browser/variations/model/client/variations_client_service_factory.h"
#import "ios/chrome/common/channel_info.h"

namespace {

class MockSessionHandleWithController
    : public contextual_search::MockContextualSearchSessionHandle {
 public:
  explicit MockSessionHandleWithController(
      std::unique_ptr<contextual_search::ContextualSearchContextController>
          controller)
      : controller_(std::move(controller)) {
    ON_CALL(*this, GetController())
        .WillByDefault(testing::Return(controller_.get()));
  }

 private:
  std::unique_ptr<contextual_search::ContextualSearchContextController>
      controller_;
};

}  // namespace

MockIOSContextualSearchService::MockIOSContextualSearchService(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TemplateURLService* template_url_service,
    variations::VariationsClient* variations_client,
    version_info::Channel channel,
    const std::string& locale)
    : IOSContextualSearchService(identity_manager,
                                 url_loader_factory,
                                 template_url_service,
                                 variations_client,
                                 channel,
                                 locale) {
  ON_CALL(*this, CreateSession)
      .WillByDefault(
          [](std::unique_ptr<
                 contextual_search::ContextualSearchContextController::
                     ConfigParams> config,
             contextual_search::ContextualSearchSource source,
             std::optional<lens::LensOverlayInvocationSource>
                 invocation_source) {
            auto controller = std::make_unique<testing::NiceMock<
                contextual_search::MockContextualSearchContextController>>();

            return std::make_unique<
                testing::NiceMock<MockSessionHandleWithController>>(
                std::move(controller));
          });
}

MockIOSContextualSearchService::~MockIOSContextualSearchService() = default;

// static
std::unique_ptr<MockIOSContextualSearchService>
MockIOSContextualSearchService::CreateTestingProfileService(
    ProfileIOS* profile) {
  auto* variations_client_service =
      VariationsClientServiceFactory::GetForProfile(profile);
  return std::make_unique<MockIOSContextualSearchService>(
      IdentityManagerFactory::GetForProfile(profile),
      GetApplicationContext()->GetSharedURLLoaderFactory(),
      ios::TemplateURLServiceFactory::GetForProfile(profile),
      static_cast<variations::VariationsClient*>(variations_client_service),
      ::GetChannel(),
      GetApplicationContext()->GetApplicationLocaleStorage()->Get());
}
