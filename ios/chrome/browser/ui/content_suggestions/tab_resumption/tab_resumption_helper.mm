// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_helper.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sync/service/sync_service.h"
#import "components/sync_sessions/open_tabs_ui_delegate.h"
#import "components/sync_sessions/session_sync_service.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#import "ios/chrome/browser/ntp/home/features.h"
#import "ios/chrome/browser/sessions/session_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/model/distant_session.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/chrome/browser/synced_sessions/model/synced_sessions.h"
#import "ios/chrome/browser/tabs/model/tab_sync_util.h"
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

// Fetches the favicon for the given `item` then asynchronously invokes
// `item_block_handler` and exits.
void FetchFaviconForItem(
    TabResumptionItem* item,
    FaviconLoader* favicon_loader,
    TabResumptionHelper::TabResumptionItemCompletionBlock item_block_handler) {
  favicon_loader->FaviconForPageUrl(
      item.tabURL, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/true, ^(FaviconAttributes* attributes) {
        if (!attributes.usesDefaultImage) {
          item.faviconImage = attributes.faviconImage;
          item_block_handler(item);
        }
      });
}

// Creates a TabResumptionItem corresponding to the last active distant tab then
// asynchronously invokes `item_block_handler` and exits.
void LastSyncedTabItemFromLastActiveDistantTab(
    const synced_sessions::DistantSession* session,
    const synced_sessions::DistantTab* tab,
    FaviconLoader* favicon_loader,
    TabResumptionHelper::TabResumptionItemCompletionBlock item_block_handler) {
  CHECK(!IsTabResumptionEnabledForMostRecentTabOnly());

  TabResumptionItem* tab_resumption_item = [[TabResumptionItem alloc]
      initWithItemType:TabResumptionItemType::kLastSyncedTab];
  tab_resumption_item.sessionName = base::SysUTF8ToNSString(session->name);
  tab_resumption_item.tabTitle = base::SysUTF16ToNSString(tab->title);
  tab_resumption_item.syncedTime = tab->last_active_time;
  tab_resumption_item.tabURL = tab->virtual_url;

  // Fetch the favicon.
  FetchFaviconForItem(tab_resumption_item, favicon_loader, item_block_handler);
}

// Creates a TabResumptionItem corresponding to the last synced tab then
// asynchronously invokes `item_block_handler` and exits.
void MostRecentTabItemFromWebState(
    web::WebState* web_state,
    base::Time opened_time,
    FaviconLoader* favicon_loader,
    TabResumptionHelper::TabResumptionItemCompletionBlock item_block_handler) {
  TabResumptionItem* tab_resumption_item = [[TabResumptionItem alloc]
      initWithItemType:TabResumptionItemType::kMostRecentTab];
  tab_resumption_item.tabTitle =
      base::SysUTF16ToNSString(web_state->GetTitle());
  tab_resumption_item.syncedTime = opened_time;
  tab_resumption_item.tabURL = web_state->GetLastCommittedURL();

  // Fetch the favicon.
  FetchFaviconForItem(tab_resumption_item, favicon_loader, item_block_handler);
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
  session_tag_ = "";
  tab_id_ = SessionID::InvalidValue();

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
    NSDate* most_recent_tab_date = base::apple::ObjCCastStrict<NSDate>(
        [scene sessionObjectForKey:kStartSurfaceSceneEnterIntoBackgroundTime]);
    if (most_recent_tab_date != nil) {
      most_recent_tab_opened_time =
          base::Time::FromNSDate(most_recent_tab_date);
    }
  }

  const synced_sessions::DistantSession* session = nullptr;
  const synced_sessions::DistantTab* tab = nullptr;
  auto const synced_sessions =
      std::make_unique<synced_sessions::SyncedSessions>(session_sync_service_);
  if (!IsTabResumptionEnabledForMostRecentTabOnly()) {
    LastActiveDistantTab last_distant_tab = GetLastActiveDistantTab(
        synced_sessions.get(), TabResumptionForXDevicesTimeThreshold());
    if (last_distant_tab.tab) {
      tab = last_distant_tab.tab;
      if (last_distant_item_url_ != tab->virtual_url) {
        last_distant_item_url_ = tab->virtual_url;
        session = last_distant_tab.session;
        last_synced_tab_synced_time = tab->last_active_time;
      }
    }
  }

  // If both times have not been updated, that means there is no item to return.
  if (most_recent_tab_opened_time == base::Time::UnixEpoch() &&
      last_synced_tab_synced_time == base::Time::UnixEpoch()) {
    return;
  } else if (last_synced_tab_synced_time > most_recent_tab_opened_time) {
    CHECK(!IsTabResumptionEnabledForMostRecentTabOnly());
    LastSyncedTabItemFromLastActiveDistantTab(session, tab, favicon_loader_,
                                              item_block_handler);
    session_tag_ = session->tag;
    tab_id_ = tab->tab_id;
  } else if (can_show_most_recent_item_) {
    MostRecentTabItemFromWebState(most_recent_tab, most_recent_tab_opened_time,
                                  favicon_loader_, item_block_handler);
  }
}

void TabResumptionHelper::SetCanSHowMostRecentItem(const bool show) {
  can_show_most_recent_item_ = show;
}

void TabResumptionHelper::OpenDistantTab() {
  ChromeBrowserState* browser_state = browser_->GetBrowserState();
  WebStateList* web_state_list = browser_->GetWebStateList();

  sync_sessions::OpenTabsUIDelegate* open_tabs_delegate =
      SessionSyncServiceFactory::GetForBrowserState(browser_state)
          ->GetOpenTabsUIDelegate();

  const sessions::SessionTab* session_tab = nullptr;
  if (open_tabs_delegate->GetForeignTab(session_tag_, tab_id_, &session_tab)) {
    new_tab_page_uma::RecordAction(
        browser_state->IsOffTheRecord(), web_state_list->GetActiveWebState(),
        new_tab_page_uma::ACTION_OPENED_FOREIGN_SESSION);

    std::unique_ptr<web::WebState> web_state =
        session_util::CreateWebStateWithNavigationEntries(
            browser_state, session_tab->current_navigation_index,
            session_tab->navigations);
    web_state_list->ReplaceWebStateAt(web_state_list->active_index(),
                                      std::move(web_state));
  }
}
