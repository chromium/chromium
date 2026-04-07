// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/model/mock_ios_contextual_search_service.h"

#import "base/memory/ref_counted.h"
#import "base/memory/scoped_refptr.h"
#import "base/observer_list.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/contextual_search/mock_contextual_search_session_handle.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/variations/model/client/variations_client_service.h"
#import "ios/chrome/browser/variations/model/client/variations_client_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "url/gurl.h"

namespace {

// A mock session handle that owns a mock controller.
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

void MockIOSContextualSearchService::SetTabUploadAutoSucceed(
    bool auto_succeed) {
  tab_upload_auto_succeed_ = auto_succeed;
}

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
      .WillByDefault([this](
                         std::unique_ptr<contextual_search::
                                             ContextualSearchContextController::
                                                 ConfigParams> config,
                         contextual_search::ContextualSearchSource source,
                         std::optional<lens::LensOverlayInvocationSource>
                             invocation_source) {
        std::unique_ptr<testing::NiceMock<
            contextual_search::MockContextualSearchContextController>>
            controller = std::make_unique<testing::NiceMock<
                contextual_search::MockContextualSearchContextController>>();

        if (tab_upload_auto_succeed_) {
          // The EG test expects the upload to succeed to enable the send
          // button.
          using ContextUploadStatusObserverList = base::ObserverList<
              contextual_search::ContextualSearchContextController::
                  ContextUploadStatusObserver>;
          scoped_refptr<base::RefCountedData<ContextUploadStatusObserverList>>
              observers = base::MakeRefCounted<
                  base::RefCountedData<ContextUploadStatusObserverList>>();

          // Mocks observer registration to allow notifying the EG test of
          // upload status changes.
          ON_CALL(*controller, AddObserver(testing::_))
              .WillByDefault(
                  [observers](
                      contextual_search::ContextualSearchContextController::
                          ContextUploadStatusObserver* obs) {
                    observers->data.AddObserver(obs);
                  });
          ON_CALL(*controller, RemoveObserver(testing::_))
              .WillByDefault(
                  [observers](
                      contextual_search::ContextualSearchContextController::
                          ContextUploadStatusObserver* obs) {
                    observers->data.RemoveObserver(obs);
                  });

          // Simulates a successful upload by notifying observers
          // immediately.
          ON_CALL(*controller, StartFileUploadFlow)
              .WillByDefault([observers](
                                 const base::UnguessableToken& token,
                                 std::unique_ptr<lens::ContextualInputData>
                                     input_data,
                                 std::optional<lens::ImageEncodingOptions>
                                     image_options) {
                for (auto& observer : observers->data) {
                  observer.OnContextUploadStatusChanged(
                      token,
                      input_data->primary_content_type.value_or(
                          lens::MimeType::kUnknown),
                      contextual_search::ContextUploadStatus::kUploadSuccessful,
                      std::nullopt);
                }
              });
        }

        std::unique_ptr<testing::NiceMock<MockSessionHandleWithController>>
            session_handle = std::make_unique<
                testing::NiceMock<MockSessionHandleWithController>>(
                std::move(controller));

        if (tab_upload_auto_succeed_) {
          // Returns a valid token for the upload flow.
          ON_CALL(*session_handle, CreateContextToken).WillByDefault([]() {
            return base::UnguessableToken::Create();
          });

          // Delegates tab upload to the controller's file upload flow.
          ON_CALL(*session_handle, StartTabContextUploadFlow)
              .WillByDefault(
                  [handle = session_handle.get()](
                      const base::UnguessableToken& token,
                      std::unique_ptr<lens::ContextualInputData> input_data,
                      std::optional<lens::ImageEncodingOptions> image_options) {
                    handle->GetController()->StartFileUploadFlow(
                        token, std::move(input_data), std::move(image_options));
                  });

          // Completes the flow by providing a placeholder search URL.
          ON_CALL(*session_handle, CreateSearchUrl)
              .WillByDefault(
                  [](std::unique_ptr<
                         contextual_search::ContextualSearchContextController::
                             CreateSearchUrlRequestInfo> request_info,
                     base::OnceCallback<void(GURL)> callback) {
                    std::move(callback).Run(GURL("about:blank"));
                  });
        }

        return session_handle;
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
