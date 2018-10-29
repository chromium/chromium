// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/session_sync_service_factory.h"

#include <utility>

#include "base/memory/singleton.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/sync/device_info/local_device_info_provider.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/sync_sessions/local_session_event_router.h"
#include "components/sync_sessions/session_sync_prefs.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/sync/glue/sync_start_util.h"
#include "ios/chrome/browser/sync/model_type_store_service_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sessions/ios_chrome_local_session_event_router.h"
#include "ios/chrome/browser/tabs/tab_model_synced_window_delegate_getter.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/web_thread.h"
#include "url/gurl.h"

using sync_sessions::SessionSyncService;

namespace {

bool ShouldSyncURLImpl(const GURL& url) {
  if (url == kChromeUIHistoryURL) {
    // Whitelist the chrome history page, home for "Tabs from other devices",
    // so it can trigger starting up the sync engine.
    return true;
  }
  return url.is_valid() && !url.SchemeIs(kChromeUIScheme) &&
         !url.SchemeIsFile();
}

// iOS implementation of SyncSessionsClient. Needs to be in a separate class
// due to possible multiple inheritance issues, wherein IOSChromeSyncClient
// might inherit from other interfaces with same methods.
class SyncSessionsClientImpl : public sync_sessions::SyncSessionsClient {
 public:
  explicit SyncSessionsClientImpl(ios::ChromeBrowserState* browser_state)
      : browser_state_(browser_state),
        window_delegates_getter_(
            std::make_unique<TabModelSyncedWindowDelegatesGetter>()),
        local_session_event_router_(
            std::make_unique<IOSChromeLocalSessionEventRouter>(
                browser_state_,
                this,
                ios::sync_start_util::GetFlareForSyncableService(
                    browser_state_->GetStatePath()))),
        session_sync_prefs_(browser_state->GetPrefs()) {}

  ~SyncSessionsClientImpl() override {}

  // SyncSessionsClient implementation.
  favicon::FaviconService* GetFaviconService() override {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    return ios::FaviconServiceFactory::GetForBrowserState(
        browser_state_, ServiceAccessType::IMPLICIT_ACCESS);
  }

  history::HistoryService* GetHistoryService() override {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    return ios::HistoryServiceFactory::GetForBrowserState(
        browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
  }

  sync_sessions::SessionSyncPrefs* GetSessionSyncPrefs() override {
    return &session_sync_prefs_;
  }

  syncer::RepeatingModelTypeStoreFactory GetStoreFactory() override {
    return ModelTypeStoreServiceFactory::GetForBrowserState(browser_state_)
        ->GetStoreFactory();
  }

  const syncer::DeviceInfo* GetLocalDeviceInfo() override {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    browser_sync::ProfileSyncService* profile_sync_service =
        ProfileSyncServiceFactory::GetForBrowserStateIfExists(browser_state_);
    if (!profile_sync_service) {
      return nullptr;
    }
    return profile_sync_service->GetLocalDeviceInfoProvider()
        ->GetLocalDeviceInfo();
  }

  bool ShouldSyncURL(const GURL& url) const override {
    return ShouldSyncURLImpl(url);
  }

  sync_sessions::SyncedWindowDelegatesGetter* GetSyncedWindowDelegatesGetter()
      override {
    return window_delegates_getter_.get();
  }

  sync_sessions::LocalSessionEventRouter* GetLocalSessionEventRouter()
      override {
    return local_session_event_router_.get();
  }

  void NotifyForeignSessionUpdated() override {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    browser_sync::ProfileSyncService* profile_sync_service =
        ProfileSyncServiceFactory::GetForBrowserStateIfExists(browser_state_);
    if (profile_sync_service) {
      profile_sync_service->NotifyForeignSessionUpdated();
    }
  }

 private:
  ios::ChromeBrowserState* const browser_state_;
  const std::unique_ptr<sync_sessions::SyncedWindowDelegatesGetter>
      window_delegates_getter_;
  const std::unique_ptr<IOSChromeLocalSessionEventRouter>
      local_session_event_router_;
  sync_sessions::SessionSyncPrefs session_sync_prefs_;

  DISALLOW_COPY_AND_ASSIGN(SyncSessionsClientImpl);
};

}  // namespace

// static
SessionSyncServiceFactory* SessionSyncServiceFactory::GetInstance() {
  return base::Singleton<SessionSyncServiceFactory>::get();
}

// static
bool SessionSyncServiceFactory::ShouldSyncURLForTesting(const GURL& url) {
  return ShouldSyncURLImpl(url);
}

// static
SessionSyncService* SessionSyncServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<SessionSyncService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

SessionSyncServiceFactory::SessionSyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SessionSyncService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::FaviconServiceFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
}

SessionSyncServiceFactory::~SessionSyncServiceFactory() {}

std::unique_ptr<KeyedService>
SessionSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<sync_sessions::SessionSyncService>(
      ::GetChannel(), std::make_unique<SyncSessionsClientImpl>(browser_state));
}
