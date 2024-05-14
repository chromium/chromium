// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/model/translate_service_ios.h"

#import "base/functional/bind.h"
#import "base/notreached.h"
#import "components/language/core/browser/language_model.h"
#import "components/prefs/pref_service.h"
#import "components/translate/core/browser/translate_download_manager.h"
#import "components/translate/core/browser/translate_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "url/gurl.h"

namespace {
// The singleton instance of TranslateServiceIOS.
TranslateServiceIOS* g_translate_service = nullptr;
}  // namespace

TranslateServiceIOS::TranslateServiceIOS()
    : resource_request_allowed_notifier_(
          GetApplicationContext()->GetLocalState(),
          nullptr,
          base::BindOnce(&ApplicationContext::GetNetworkConnectionTracker,
                         base::Unretained(GetApplicationContext()))) {
  resource_request_allowed_notifier_.Init(this, true /* leaky */);
}

TranslateServiceIOS::~TranslateServiceIOS() {}

// static
void TranslateServiceIOS::Initialize() {
  if (g_translate_service) {
    return;
  }

  g_translate_service = new TranslateServiceIOS;
  // Initialize the allowed state for resource requests.
  g_translate_service->OnResourceRequestsAllowed();
  translate::TranslateDownloadManager* download_manager =
      translate::TranslateDownloadManager::GetInstance();
  download_manager->set_url_loader_factory(
      GetApplicationContext()->GetSharedURLLoaderFactory());
  download_manager->set_application_locale(
      GetApplicationContext()->GetApplicationLocale());
}

// static
void TranslateServiceIOS::Shutdown() {
  translate::TranslateDownloadManager* download_manager =
      translate::TranslateDownloadManager::GetInstance();
  download_manager->Shutdown();
  delete g_translate_service;
  g_translate_service = nullptr;
}

void TranslateServiceIOS::OnResourceRequestsAllowed() {
  translate::TranslateLanguageList* language_list =
      translate::TranslateDownloadManager::GetInstance()->language_list();
  if (!language_list) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  language_list->SetResourceRequestsAllowed(
      resource_request_allowed_notifier_.ResourceRequestsAllowed());
}

// static
std::string TranslateServiceIOS::GetTargetLanguage(
    PrefService* prefs,
    language::LanguageModel* language_model) {
  return translate::TranslateManager::GetTargetLanguage(
      ChromeIOSTranslateClient::CreateTranslatePrefs(prefs).get(),
      language_model);
}

// static
bool TranslateServiceIOS::IsTranslatableURL(const GURL& url) {
  // A URL is translatable unless it is one of the following:
  // - empty (can happen for popups created with window.open(""))
  // - an internal URL
  return !url.is_empty() && !url.SchemeIs(kChromeUIScheme);
}
