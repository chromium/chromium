// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_helper.h"

#import "base/strings/sys_string_conversions.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/sync/service/sync_service.h"
#import "components/sync_sessions/session_sync_service.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/sessions/session_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/sync/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/distant_session.h"
#import "ios/chrome/browser/synced_sessions/distant_tab.h"
#import "ios/chrome/browser/synced_sessions/synced_sessions.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_item.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"

namespace {

// The key to store the timestamp when the scene enters into background.
NSString* kStartSurfaceSceneEnterIntoBackgroundTime =
    @"StartSurfaceSceneEnterIntoBackgroundTime";

// Creates a TabResumptionItem corresponding to the last synced tab then invokes
// `item_block_handler` and exits.
void LastSyncedTabItemFromSession(
    const synced_sessions::DistantSession* session,
    FaviconLoader* favicon_loader,
    TabResumptionHelper::TabResumptionItemCompletionBlock item_block_handler) {
  CHECK(!IsTabResumptionEnabledForMostRecentTabOnly());

  const synced_sessions::DistantTab* tab = session->tabs.front().get();

  TabResumptionItem* tab_resumption_item = [[TabResumptionItem alloc]
      initWithItemType:TabResumptionItemType::kLastSyncedTab];
  tab_resumption_item.sessionName = base::SysUTF8ToNSString(session->name);
  tab_resumption_item.tabTitle = base::SysUTF16ToNSString(tab->title);
  tab_resumption_item.syncedTime = session->modified_time;
  tab_resumption_item.tabURL = tab->virtual_url;

  // Fetch the favicon.
  favicon_loader->FaviconForPageUrl(
      tab_resumption_item.tabURL, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/true, ^(FaviconAttributes* attributes) {
        if (!attributes.usesDefaultImage) {
          tab_resumption_item.faviconImage = attributes.faviconImage;
          item_block_handler(tab_resumption_item);
        }
      });
}

// Creates a TabResumptionItem corresponding to the last synced tab then invokes
// `item_block_handler` and exits.
void MostRecentTabItemFromWebState(
    web::WebState* web_state,
    base::Time opened_time,
    TabResumptionHelper::TabResumptionItemCompletionBlock item_block_handler) {
  TabResumptionItem* tab_resumption_item = [[TabResumptionItem alloc]
      initWithItemType:TabResumptionItemType::kMostRecentTab];
  tab_resumption_item.tabTitle =
      base::SysUTF16ToNSString(web_state->GetTitle());
  tab_resumption_item.syncedTime = opened_time;
  tab_resumption_item.tabURL = web_state->GetLastCommittedURL();

  // Retrieve favicon associated to the `web_state`.
  favicon::WebFaviconDriver* driver =
      favicon::WebFaviconDriver::FromWebState(web_state);
  if (driver->FaviconIsValid()) {
    gfx::Image favicon = driver->GetFavicon();
    if (!favicon.IsEmpty()) {
      tab_resumption_item.faviconImage = favicon.ToUIImage();
    }
  }

  item_block_handler(tab_resumption_item);
}

}  // namespace

TabResumptionHelper::TabResumptionHelper(Browser* browser) : browser_(browser) {
  CHECK(browser_);
  CHECK(IsTabResumptionEnabled());

  ChromeBrowserState* browser_state = browser_->GetBrowserState();
  session_sync_service_ =
      SessionSyncServiceFactory::GetForBrowserState(browser_state);
  sync_service_ = SyncServiceFactory::GetForBrowserState(browser_state);
  favicon_loader_ =
      IOSChromeFaviconLoaderFactory::GetForBrowserState(browser_state);
  recent_tab_browser_agent_ =
      StartSurfaceRecentTabBrowserAgent::FromBrowser(browser_);
}

#pragma mark - Public methods

void TabResumptionHelper::LastTabResumptionItem(
    TabResumptionItemCompletionBlock item_block_handler) {
  if (!IsTabResumptionEnabledForMostRecentTabOnly()) {
    // If sync is enabled and `GetOpenTabsUIDelegate()` returns nullptr, that
    // means the `session_sync_service_` is not fully operational.
    if (sync_service_->IsSyncFeatureEnabled() &&
        !session_sync_service_->GetOpenTabsUIDelegate()) {
      return;
    }
  }

  base::Time most_recent_tab_opened_time = base::Time::UnixEpoch();
  base::Time last_synced_tab_synced_time = base::Time::UnixEpoch();

  web::WebState* most_recent_tab = recent_tab_browser_agent_->most_recent_tab();
  if (most_recent_tab) {
    SceneState* scene =
        SceneStateBrowserAgent::FromBrowser(browser_)->GetSceneState();
    most_recent_tab_opened_time = base::Time::FromNSDate((NSDate*)[scene
        sessionObjectForKey:kStartSurfaceSceneEnterIntoBackgroundTime]);
  }

  const synced_sessions::DistantSession* session;
  auto const synced_sessions =
      std::make_unique<synced_sessions::SyncedSessions>(session_sync_service_);
  if (!IsTabResumptionEnabledForMostRecentTabOnly()) {
    if (synced_sessions->GetSessionCount()) {
      // Get the last synced session and tab.
      session = synced_sessions->GetSession(0);
      // TODO(crbug.com/1464185): Add restrictions.
      last_synced_tab_synced_time = session->modified_time;
    }
  }

  // If both times have not been updated, that means there is no item to return.
  if (most_recent_tab_opened_time == base::Time::UnixEpoch() &&
      last_synced_tab_synced_time == base::Time::UnixEpoch()) {
    return;
  } else if (last_synced_tab_synced_time > most_recent_tab_opened_time) {
    CHECK(!IsTabResumptionEnabledForMostRecentTabOnly());
    LastSyncedTabItemFromSession(session, favicon_loader_, item_block_handler);
  } else {
    MostRecentTabItemFromWebState(most_recent_tab, most_recent_tab_opened_time,
                                  item_block_handler);
  }
}
