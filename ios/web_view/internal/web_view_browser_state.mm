// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/web_view_browser_state.h"

#include <memory>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/gcm_driver/gcm_channel_status_syncer.h"
#include "components/history/core/common/pref_names.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/pref_service_factory.h"
#include "components/signin/ios/browser/active_state_manager.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync_device_info/device_info_prefs.h"
#include "components/sync_sessions/session_sync_prefs.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/web_view/cwv_web_view_buildflags.h"
#include "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/autofill/web_view_autofill_log_router_factory.h"
#include "ios/web_view/internal/autofill/web_view_personal_data_manager_factory.h"
#include "ios/web_view/internal/content_settings/web_view_cookie_settings_factory.h"
#include "ios/web_view/internal/content_settings/web_view_host_content_settings_map_factory.h"
#include "ios/web_view/internal/language/web_view_language_model_manager_factory.h"
#include "ios/web_view/internal/language/web_view_url_language_histogram_factory.h"
#import "ios/web_view/internal/passwords/web_view_password_manager_log_router_factory.h"
#include "ios/web_view/internal/passwords/web_view_password_store_factory.h"
#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#include "ios/web_view/internal/signin/web_view_signin_client_factory.h"
#include "ios/web_view/internal/signin/web_view_signin_error_controller_factory.h"
#import "ios/web_view/internal/sync/web_view_gcm_profile_service_factory.h"
#import "ios/web_view/internal/sync/web_view_model_type_store_service_factory.h"
#import "ios/web_view/internal/sync/web_view_profile_invalidation_provider_factory.h"
#import "ios/web_view/internal/sync/web_view_profile_sync_service_factory.h"
#include "ios/web_view/internal/translate/web_view_translate_accept_languages_factory.h"
#include "ios/web_view/internal/translate/web_view_translate_ranker_factory.h"
#include "ios/web_view/internal/web_view_download_manager.h"
#include "ios/web_view/internal/web_view_url_request_context_getter.h"
#include "ios/web_view/internal/webdata_services/web_view_web_data_service_wrapper_factory.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kPreferencesFilename[] =
    FILE_PATH_LITERAL("ChromeWebViewPreferences");
}

namespace ios_web_view {

WebViewBrowserState::WebViewBrowserState(
    bool off_the_record,
    WebViewBrowserState* recording_browser_state /* = nullptr */)
    : web::BrowserState(),
      off_the_record_(off_the_record),
      download_manager_(std::make_unique<WebViewDownloadManager>(this)) {
  // A recording browser state must not be associated with another recording
  // browser state. An off the record browser state must be associated with
  // a recording browser state.
  DCHECK((!off_the_record && !recording_browser_state) ||
         (off_the_record && recording_browser_state &&
          !recording_browser_state->IsOffTheRecord()));
  recording_browser_state_ = recording_browser_state;

  // IO access is required to setup the browser state. In Chrome, this is
  // already allowed during thread startup. However, startup time of
  // ChromeWebView is not predetermined, so IO access is temporarily allowed.
  bool wasIOAllowed = base::ThreadRestrictions::SetIOAllowed(true);

  CHECK(base::PathService::Get(base::DIR_APP_DATA, &path_));

  request_context_getter_ = new WebViewURLRequestContextGetter(
      GetStatePath(), this, ApplicationContext::GetInstance()->GetNetLog(),
      base::CreateSingleThreadTaskRunner({web::WebThread::IO}));

  // Initialize prefs.
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry =
      new user_prefs::PrefRegistrySyncable;
  RegisterPrefs(pref_registry.get());

  scoped_refptr<PersistentPrefStore> user_pref_store;
  if (off_the_record) {
    user_pref_store = new InMemoryPrefStore();
  } else {
    user_pref_store = new JsonPrefStore(path_.Append(kPreferencesFilename));
  }

  PrefServiceFactory factory;
  factory.set_user_prefs(user_pref_store);
  prefs_ = factory.Create(pref_registry.get());

  base::ThreadRestrictions::SetIOAllowed(wasIOAllowed);

#if BUILDFLAG(IOS_WEB_VIEW_ENABLE_SYNC)
  ActiveStateManager::FromBrowserState(this)->SetActive(true);
#endif  // BUILDFLAG(IOS_WEB_VIEW_ENABLE_SYNC)

  BrowserStateDependencyManager::GetInstance()->CreateBrowserStateServices(
      this);
}

WebViewBrowserState::~WebViewBrowserState() {
  BrowserStateDependencyManager::GetInstance()->DestroyBrowserStateServices(
      this);

#if BUILDFLAG(IOS_WEB_VIEW_ENABLE_SYNC)
  ActiveStateManager::FromBrowserState(this)->SetActive(false);
#endif  // BUILDFLAG(IOS_WEB_VIEW_ENABLE_SYNC)

  base::PostTask(FROM_HERE, {web::WebThread::IO},
                 base::BindOnce(&WebViewURLRequestContextGetter::ShutDown,
                                request_context_getter_));
}

PrefService* WebViewBrowserState::GetPrefs() {
  DCHECK(prefs_);
  return prefs_.get();
}

WebViewBrowserState* WebViewBrowserState::GetRecordingBrowserState() {
  if (recording_browser_state_) {
    return recording_browser_state_;
  } else if (!off_the_record_) {
    return this;
  } else {
    return nullptr;
  }
}

// static
WebViewBrowserState* WebViewBrowserState::FromBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<WebViewBrowserState*>(browser_state);
}

bool WebViewBrowserState::IsOffTheRecord() const {
  return off_the_record_;
}

base::FilePath WebViewBrowserState::GetStatePath() const {
  return path_;
}

net::URLRequestContextGetter* WebViewBrowserState::GetRequestContext() {
  return request_context_getter_.get();
}

void WebViewBrowserState::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* pref_registry) {
  pref_registry->RegisterBooleanPref(prefs::kOfferTranslateEnabled, true);
  pref_registry->RegisterBooleanPref(prefs::kSavingBrowserHistoryDisabled,
                                     true);
  language::LanguagePrefs::RegisterProfilePrefs(pref_registry);
  translate::TranslatePrefs::RegisterProfilePrefs(pref_registry);

#if BUILDFLAG(IOS_WEB_VIEW_ENABLE_AUTOFILL)
  autofill::prefs::RegisterProfilePrefs(pref_registry);
  password_manager::PasswordManager::RegisterProfilePrefs(pref_registry);
#endif  // BUILDFLAG(IOS_WEB_VIEW_ENABLE_AUTOFILL)

#if BUILDFLAG(IOS_WEB_VIEW_ENABLE_SYNC)
  gcm::GCMChannelStatusSyncer::RegisterProfilePrefs(pref_registry);
  sync_sessions::SessionSyncPrefs::RegisterProfilePrefs(pref_registry);
  syncer::SyncPrefs::RegisterProfilePrefs(pref_registry);
  syncer::DeviceInfoPrefs::RegisterProfilePrefs(pref_registry);
#endif  // BUILDFLAG(IOS_WEB_VIEW_ENABLE_SYNC)

  // Instantiate all factories to setup dependency graph for pref registration.
  WebViewLanguageModelManagerFactory::GetInstance();
  WebViewTranslateRankerFactory::GetInstance();
  WebViewUrlLanguageHistogramFactory::GetInstance();
  WebViewTranslateAcceptLanguagesFactory::GetInstance();

#if BUILDFLAG(IOS_WEB_VIEW_ENABLE_AUTOFILL)
  autofill::WebViewAutofillLogRouterFactory::GetInstance();
  WebViewPersonalDataManagerFactory::GetInstance();
  WebViewWebDataServiceWrapperFactory::GetInstance();
  WebViewPasswordManagerLogRouterFactory::GetInstance();
  WebViewPasswordStoreFactory::GetInstance();
#endif  // BUILDFLAG(IOS_WEB_VIEW_ENABLE_AUTOFILL)

#if BUILDFLAG(IOS_WEB_VIEW_ENABLE_SYNC)
  WebViewCookieSettingsFactory::GetInstance();
  WebViewHostContentSettingsMapFactory::GetInstance();
  WebViewSigninClientFactory::GetInstance();
  WebViewSigninErrorControllerFactory::GetInstance();
  WebViewIdentityManagerFactory::GetInstance();
  WebViewGCMProfileServiceFactory::GetInstance();
  WebViewProfileInvalidationProviderFactory::GetInstance();
  WebViewProfileSyncServiceFactory::GetInstance();
  WebViewModelTypeStoreServiceFactory::GetInstance();
#endif  // BUILDFLAG(IOS_WEB_VIEW_ENABLE_SYNC)

  BrowserStateDependencyManager::GetInstance()
      ->RegisterBrowserStatePrefsForServices(pref_registry);
}

}  // namespace ios_web_view
