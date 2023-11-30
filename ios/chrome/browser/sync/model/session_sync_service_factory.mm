// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"

#import <utility>

#import "base/memory/raw_ptr.h"
#import "base/no_destructor.h"
#import "base/time/time.h"
#import "components/dom_distiller/core/url_constants.h"
#import "components/history/core/browser/history_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/sync/model/model_type_store_service.h"
#import "components/sync/service/sync_service.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "components/sync_device_info/device_info_tracker.h"
#import "components/sync_sessions/local_session_event_router.h"
#import "components/sync_sessions/session_sync_prefs.h"
#import "components/sync_sessions/session_sync_service_impl.h"
#import "components/sync_sessions/sync_sessions_client.h"
#import "components/sync_sessions/synced_window_delegates_getter.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/glue/sync_start_util.h"
#import "ios/chrome/browser/sync/model/model_type_store_service_factory.h"
#import "ios/chrome/browser/sync/model/sessions/ios_chrome_local_session_event_router.h"
#import "ios/chrome/browser/tabs/model/ios_synced_window_delegate_getter.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/web/public/thread/web_thread.h"
#import "url/gurl.h"

using sync_sessions::SessionSyncService;

namespace {

// Note: iOS doesn't use the "chrome-native://" scheme, but in some
// circumstances, such URLs can get synced from other platforms. Marking them as
// "non-syncable" here means they'll be filtered out from UIs such as Recent
// Tabs.
const char kChromeNativeScheme[] = "chrome-native";

bool ShouldSyncURLImpl(const GURL& url) {
  return url.is_valid() && !url.SchemeIs(kChromeUIScheme) &&
         !url.SchemeIs(kChromeNativeScheme) && !url.SchemeIsFile() &&
         !url.SchemeIs(dom_distiller::kDomDistillerScheme);
}

// iOS implementation of SyncSessionsClient. Needs to be in a separate class
// due to possible multiple inheritance issues, wherein IOSChromeSyncClient
// might inherit from other interfaces with same methods.
class SyncSessionsClientImpl : public sync_sessions::SyncSessionsClient {
 public:
  SyncSessionsClientImpl(
      const base::FilePath& browser_state_path,
      PrefService* pref_service,
      BrowserList* browser_list,
      history::HistoryService* history_service,
      syncer::DeviceInfoSyncService* device_info_service,
      syncer::ModelTypeStoreService* model_type_store_service)
      : history_service_(history_service),
        device_info_service_(device_info_service),
        model_type_store_service_(model_type_store_service),
        window_delegates_getter_(browser_list),
        local_session_event_router_(
            browser_list,
            this,
            ios::sync_start_util::GetFlareForSyncableService(
                browser_state_path)),
        session_sync_prefs_(pref_service) {}

  SyncSessionsClientImpl(const SyncSessionsClientImpl&) = delete;
  SyncSessionsClientImpl& operator=(const SyncSessionsClientImpl&) = delete;

  ~SyncSessionsClientImpl() override {}

  // SyncSessionsClient implementation.
  sync_sessions::SessionSyncPrefs* GetSessionSyncPrefs() override {
    return &session_sync_prefs_;
  }

  syncer::RepeatingModelTypeStoreFactory GetStoreFactory() override {
    return model_type_store_service_->GetStoreFactory();
  }

  void ClearAllOnDemandFavicons() override {
    if (!history_service_) {
      return;
    }
    history_service_->ClearAllOnDemandFavicons();
  }

  bool ShouldSyncURL(const GURL& url) const override {
    return ShouldSyncURLImpl(url);
  }

  bool IsRecentLocalCacheGuid(const std::string& cache_guid) const override {
    return device_info_service_->GetDeviceInfoTracker()->IsRecentLocalCacheGuid(
        cache_guid);
  }

  sync_sessions::SyncedWindowDelegatesGetter* GetSyncedWindowDelegatesGetter()
      override {
    return &window_delegates_getter_;
  }

  sync_sessions::LocalSessionEventRouter* GetLocalSessionEventRouter()
      override {
    return &local_session_event_router_;
  }

 private:
  raw_ptr<history::HistoryService> history_service_;
  raw_ptr<syncer::DeviceInfoSyncService> device_info_service_;
  raw_ptr<syncer::ModelTypeStoreService> model_type_store_service_;
  IOSSyncedWindowDelegatesGetter window_delegates_getter_;
  IOSChromeLocalSessionEventRouter local_session_event_router_;
  sync_sessions::SessionSyncPrefs session_sync_prefs_;
};

}  // namespace

// static
SessionSyncServiceFactory* SessionSyncServiceFactory::GetInstance() {
  static base::NoDestructor<SessionSyncServiceFactory> instance;
  return instance.get();
}

// static
bool SessionSyncServiceFactory::ShouldSyncURLForTesting(const GURL& url) {
  return ShouldSyncURLImpl(url);
}

// static
SessionSyncService* SessionSyncServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<SessionSyncService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

SessionSyncServiceFactory::SessionSyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SessionSyncService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
}

SessionSyncServiceFactory::~SessionSyncServiceFactory() {}

std::unique_ptr<KeyedService>
SessionSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<sync_sessions::SessionSyncServiceImpl>(
      ::GetChannel(),
      std::make_unique<SyncSessionsClientImpl>(
          browser_state->GetStatePath(), browser_state->GetPrefs(),
          BrowserListFactory::GetForBrowserState(browser_state),
          ios::HistoryServiceFactory::GetForBrowserState(
              browser_state, ServiceAccessType::EXPLICIT_ACCESS),
          DeviceInfoSyncServiceFactory::GetForBrowserState(browser_state),
          ModelTypeStoreServiceFactory::GetForBrowserState(browser_state)));
}
