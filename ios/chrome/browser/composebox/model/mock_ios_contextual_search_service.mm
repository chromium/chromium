// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/model/mock_ios_contextual_search_service.h"

#import <map>
#import <memory>
#import <optional>
#import <string>
#import <string_view>
#import <utility>
#import <vector>

#import "base/functional/callback.h"
#import "base/observer_list.h"
#import "base/unguessable_token.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/contextual_search/contextual_search_types.h"
#import "components/contextual_search/mock_contextual_search_session_handle.h"
#import "components/lens/contextual_input.h"
#import "components/lens/lens_bitmap_processing.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/variations/model/client/variations_client_service.h"
#import "ios/chrome/browser/variations/model/client/variations_client_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "url/gurl.h"

namespace {

constexpr std::string_view kPlaceholderUrl = "about:blank";
constexpr std::string_view kPdfMimeSubstring = "pdf";
constexpr std::string_view kImageMimeSubstring = "image";

// A mock session handle that owns a mock controller and tracks attached
// `FileInfo` entries so `InputStateModel` observes uploaded context changes.
class MockSessionHandleWithController
    : public contextual_search::MockContextualSearchSessionHandle {
 public:
  MockSessionHandleWithController(
      std::unique_ptr<testing::NiceMock<
          contextual_search::MockContextualSearchContextController>> controller,
      bool auto_succeed_uploads)
      : controller_(std::move(controller)) {
    ON_CALL(*this, GetController())
        .WillByDefault(testing::Return(controller_.get()));
    ON_CALL(*this, CreateContextToken).WillByDefault([]() {
      return base::UnguessableToken::Create();
    });
    ON_CALL(*this, GetUploadedContextFileInfos).WillByDefault([this]() {
      std::vector<contextual_search::FileInfo> infos;
      infos.reserve(uploaded_files_.size());
      for (const auto& [token, info] : uploaded_files_) {
        infos.push_back(info);
      }
      return infos;
    });
    ON_CALL(*controller_, GetFileInfo)
        .WillByDefault([this](const base::UnguessableToken& token)
                           -> const contextual_search::FileInfo* {
          auto it = uploaded_files_.find(token);
          return it != uploaded_files_.end() ? &it->second : nullptr;
        });
    ON_CALL(*controller_, DeleteFile)
        .WillByDefault([this](const base::UnguessableToken& token) {
          return uploaded_files_.erase(token) > 0;
        });
    ON_CALL(*controller_, ClearFiles).WillByDefault([this]() {
      uploaded_files_.clear();
    });

    if (auto_succeed_uploads) {
      // Mocks observer registration so tests waiting for completed uploads
      // (such as an enabled Send button) receive immediate completion signals.
      ON_CALL(*controller_, AddObserver(testing::_))
          .WillByDefault(
              [this](contextual_search::ContextualSearchContextController::
                         ContextUploadStatusObserver* obs) {
                observers_.AddObserver(obs);
              });
      ON_CALL(*controller_, RemoveObserver(testing::_))
          .WillByDefault(
              [this](contextual_search::ContextualSearchContextController::
                         ContextUploadStatusObserver* obs) {
                observers_.RemoveObserver(obs);
              });
      ON_CALL(*controller_, StartFileUploadFlow)
          .WillByDefault(
              [this](const base::UnguessableToken& token,
                     std::unique_ptr<lens::ContextualInputData> input_data,
                     std::optional<lens::ImageEncodingOptions> image_options) {
                lens::MimeType mime_type =
                    input_data ? input_data->primary_content_type.value_or(
                                     lens::MimeType::kUnknown)
                               : lens::MimeType::kUnknown;
                for (auto& observer : observers_) {
                  observer.OnContextUploadStatusChanged(
                      token, mime_type,
                      contextual_search::ContextUploadStatus::kUploadSuccessful,
                      std::nullopt);
                }
              });
    }

    ON_CALL(*this, StartTabContextUploadFlow)
        .WillByDefault(
            [this, auto_succeed_uploads](
                const base::UnguessableToken& token,
                std::unique_ptr<lens::ContextualInputData> input_data,
                std::optional<lens::ImageEncodingOptions> image_options) {
              contextual_search::FileInfo file_info;
              file_info.file_token = token;
              file_info.tab_url =
                  (input_data && input_data->page_url.has_value())
                      ? input_data->page_url
                      : GURL(kPlaceholderUrl);
              if (input_data) {
                file_info.tab_title = input_data->page_title;
                file_info.tab_session_id = input_data->tab_session_id;
                file_info.mime_type = input_data->primary_content_type.value_or(
                    lens::MimeType::kUnknown);
              }
              uploaded_files_[token] = file_info;

              if (auto_succeed_uploads) {
                controller_->StartFileUploadFlow(token, std::move(input_data),
                                                 std::move(image_options));
              }
            });

    ON_CALL(*this, StartFileContextUploadFlow)
        .WillByDefault(
            [this, auto_succeed_uploads](
                const base::UnguessableToken& token, std::string file_name,
                std::string file_mime_type, mojo_base::BigBuffer file_bytes,
                std::optional<lens::ImageEncodingOptions> image_options) {
              contextual_search::FileInfo file_info;
              file_info.file_token = token;
              file_info.file_name = file_name;
              file_info.mime_type_string = file_mime_type;
              if (file_mime_type.find(kPdfMimeSubstring) != std::string::npos) {
                file_info.mime_type = lens::MimeType::kPdf;
              } else if (file_mime_type.find(kImageMimeSubstring) !=
                         std::string::npos) {
                file_info.mime_type = lens::MimeType::kImage;
              }
              uploaded_files_[token] = file_info;

              if (auto_succeed_uploads) {
                std::unique_ptr<lens::ContextualInputData> input_data =
                    std::make_unique<lens::ContextualInputData>();
                input_data->primary_content_type = file_info.mime_type;
                controller_->StartFileUploadFlow(token, std::move(input_data),
                                                 std::move(image_options));
              }
            });
  }

 private:
  base::ObserverList<contextual_search::ContextualSearchContextController::
                         ContextUploadStatusObserver>
      observers_;
  std::map<base::UnguessableToken, contextual_search::FileInfo> uploaded_files_;
  std::unique_ptr<testing::NiceMock<
      contextual_search::MockContextualSearchContextController>>
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
        last_search_query_text_.clear();
        std::unique_ptr<testing::NiceMock<
            contextual_search::MockContextualSearchContextController>>
            controller = std::make_unique<testing::NiceMock<
                contextual_search::MockContextualSearchContextController>>();

        std::unique_ptr<testing::NiceMock<MockSessionHandleWithController>>
            session_handle = std::make_unique<
                testing::NiceMock<MockSessionHandleWithController>>(
                std::move(controller), tab_upload_auto_succeed_);

        // Completes the flow by recording the requested query text and
        // providing a placeholder search URL.
        ON_CALL(*session_handle, CreateSearchUrl)
            .WillByDefault(
                [this](
                    std::unique_ptr<
                        contextual_search::ContextualSearchContextController::
                            CreateSearchUrlRequestInfo> request_info,
                    base::OnceCallback<void(GURL)> callback) {
                  last_search_query_text_ =
                      request_info ? request_info->query_text : "";
                  std::move(callback).Run(GURL(kPlaceholderUrl));
                });

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
      std::string(GetApplicationContext()
                      ->GetApplicationLocaleStorage()
                      ->GetTag()
                      .tag_string()));
}
