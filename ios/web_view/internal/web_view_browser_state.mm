// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_browser_state.h"

#import <memory>

#import "base/base_paths.h"
#import "base/files/file_path.h"
#import "base/memory/ptr_util.h"
#import "base/path_service.h"
#import "base/threading/thread_restrictions.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/in_memory_pref_store.h"
#import "components/prefs/json_pref_store.h"
#import "components/prefs/pref_filter.h"
#import "components/prefs/pref_service_factory.h"
#import "components/profile_metrics/browser_profile_type.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/sync/base/pref_names.h"
#import "components/sync/service/glue/sync_transport_data_prefs.h"
#import "components/sync/service/sync_prefs.h"
#import "components/sync_device_info/device_info_prefs.h"
#import "components/unified_consent/unified_consent_service.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/browser_state_prefs.h"
#import "ios/web_view/internal/web_view_download_manager.h"
#import "ios/web_view/internal/web_view_url_request_context_getter.h"
#import "ui/base/l10n/l10n_util_mac.h"

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

  BrowserStateDependencyManager::GetInstance()->MarkBrowserStateLive(this);

  profile_metrics::SetBrowserProfileType(
      this, off_the_record ? profile_metrics::BrowserProfileType::kIncognito
                           : profile_metrics::BrowserProfileType::kRegular);

  {
    // IO access is required to setup the browser state. In Chrome, this is
    // already allowed during thread startup. However, startup time of
    // ChromeWebView is not predetermined, so IO access is temporarily allowed.
    base::ScopedAllowBlocking allow_blocking;

    CHECK(base::PathService::Get(base::DIR_APP_DATA, &path_));

    request_context_getter_ = new WebViewURLRequestContextGetter(
        GetStatePath(), this, ApplicationContext::GetInstance()->GetNetLog(),
        web::GetIOThreadTaskRunner({}));

    // Initialize prefs.
    scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry =
        new user_prefs::PrefRegistrySyncable;
    RegisterBrowserStatePrefs(pref_registry.get());

    scoped_refptr<PersistentPrefStore> user_pref_store;
    if (off_the_record) {
      user_pref_store = new InMemoryPrefStore();
    } else {
      user_pref_store = new JsonPrefStore(path_.Append(kPreferencesFilename));
    }

    PrefServiceFactory factory;
    factory.set_user_prefs(user_pref_store);
    prefs_ = factory.Create(pref_registry.get());
  }

  BrowserStateDependencyManager::GetInstance()->CreateBrowserStateServices(
      this);
}

WebViewBrowserState::~WebViewBrowserState() {
  BrowserStateDependencyManager::GetInstance()->DestroyBrowserStateServices(
      this);

  web::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&WebViewURLRequestContextGetter::ShutDown,
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

// static
WebViewBrowserState* WebViewBrowserState::FromWebUIIOS(web::WebUIIOS* web_ui) {
  return FromBrowserState(web_ui->GetWebState()->GetBrowserState());
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

}  // namespace ios_web_view
