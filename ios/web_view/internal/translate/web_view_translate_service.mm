// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/translate/web_view_translate_service.h"

#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "base/no_destructor.h"
#import "components/translate/core/browser/translate_download_manager.h"
#import "ios/web_view/internal/app/application_context.h"

namespace ios_web_view {

WebViewTranslateService::TranslateRequestsAllowedListener::
    TranslateRequestsAllowedListener()
    : resource_request_allowed_notifier_(
          ios_web_view::ApplicationContext::GetInstance()->GetLocalState(),
          /*disable_network_switch=*/nullptr,
          base::BindOnce(&ApplicationContext::GetNetworkConnectionTracker,
                         base::Unretained(ApplicationContext::GetInstance()))) {
  resource_request_allowed_notifier_.Init(this, /*leaky=*/false);
}

WebViewTranslateService::TranslateRequestsAllowedListener::
    ~TranslateRequestsAllowedListener() {}

void WebViewTranslateService::TranslateRequestsAllowedListener::
    OnResourceRequestsAllowed() {
  translate::TranslateLanguageList* language_list =
      translate::TranslateDownloadManager::GetInstance()->language_list();
  DCHECK(language_list);

  language_list->SetResourceRequestsAllowed(
      resource_request_allowed_notifier_.ResourceRequestsAllowed());
}

WebViewTranslateService* WebViewTranslateService::GetInstance() {
  static base::NoDestructor<WebViewTranslateService> instance;
  return instance.get();
}

WebViewTranslateService::WebViewTranslateService() {}

WebViewTranslateService::~WebViewTranslateService() = default;

void WebViewTranslateService::Initialize() {
  translate_requests_allowed_listener_ =
      std::make_unique<TranslateRequestsAllowedListener>();
  // Initialize the allowed state for resource requests.
  translate_requests_allowed_listener_->OnResourceRequestsAllowed();

  // Initialize translate.
  translate::TranslateDownloadManager* download_manager =
      translate::TranslateDownloadManager::GetInstance();
  download_manager->set_url_loader_factory(
      ios_web_view::ApplicationContext::GetInstance()
          ->GetSharedURLLoaderFactory()
          .get());
  download_manager->set_application_locale(
      ios_web_view::ApplicationContext::GetInstance()->GetApplicationLocale());
}

void WebViewTranslateService::Shutdown() {
  translate::TranslateDownloadManager* download_manager =
      translate::TranslateDownloadManager::GetInstance();
  download_manager->Shutdown();
  translate_requests_allowed_listener_.reset();
}

}  // namespace ios_web_view
