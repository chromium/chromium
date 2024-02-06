// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_helper.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/sync_sessions/open_tabs_ui_delegate.h"
#import "components/sync_sessions/session_sync_service.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/metrics/model/new_tab_page_uma.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"
#import "ios/chrome/browser/sessions/session_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/model/distant_session.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/chrome/browser/synced_sessions/model/synced_sessions.h"
#import "ios/chrome/browser/tabs/model/tab_sync_util.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_helper_delegate.h"
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

}  // namespace

TabResumptionHelper::TabResumptionHelper(
    Browser* browser,
    signin::IdentityManager* identity_manager,
    PrefService* local_state)
    : browser_(browser), local_state_(local_state) {
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
  start_surface_recent_tab_observer_.Observe(recent_tab_browser_agent_);

  if (!IsTabResumptionEnabledForMostRecentTabOnly()) {
    foreign_session_updated_subscription_ =
        session_sync_service_->SubscribeToForeignSessionsChanged(
            base::BindRepeating(&TabResumptionHelper::ForeignSessionsChanged,
                                base::Unretained(this)));
    scoped_observation_.Observe(sync_service_);
    identity_manager_observer_.Observe(identity_manager);
  }
}

TabResumptionHelper::~TabResumptionHelper() = default;

#pragma mark - Public methods

void TabResumptionHelper::LastTabResumptionItem() {
  if (tab_resumption_prefs::IsTabResumptionDisabled(local_state_)) {
    return;
  }

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
    SceneState* scene = browser_->GetSceneState();
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

  web::WebState* active_web_state =
      browser_->GetWebStateList()->GetActiveWebState();
  bool can_show_most_recent_item =
      NewTabPageTabHelper::FromWebState(active_web_state)
          ->ShouldShowStartSurface();
  // If both times have not been updated, that means there is no item to return.
  if (most_recent_tab_opened_time == base::Time::UnixEpoch() &&
      last_synced_tab_synced_time == base::Time::UnixEpoch()) {
    return;
  } else if (last_synced_tab_synced_time > most_recent_tab_opened_time) {
    CHECK(!IsTabResumptionEnabledForMostRecentTabOnly());
    LastSyncedTabItemFromLastActiveDistantTab(session, tab, favicon_loader_);
    session_tag_ = session->tag;
    tab_id_ = tab->tab_id;
  } else if (can_show_most_recent_item) {
    MostRecentTabItemFromWebState(most_recent_tab, most_recent_tab_opened_time,
                                  favicon_loader_);
  }
}

void TabResumptionHelper::OpenDistantTab() {
  ChromeBrowserState* browser_state = browser_->GetBrowserState();
  WebStateList* web_state_list = browser_->GetWebStateList();

  sync_sessions::OpenTabsUIDelegate* open_tabs_delegate =
      SessionSyncServiceFactory::GetForBrowserState(browser_state)
          ->GetOpenTabsUIDelegate();

  const sessions::SessionTab* session_tab = nullptr;
  if (open_tabs_delegate->GetForeignTab(session_tag_, tab_id_, &session_tab)) {
    bool is_ntp = web_state_list->GetActiveWebState()->GetVisibleURL() ==
                  kChromeUINewTabURL;
    new_tab_page_uma::RecordNTPAction(
        browser_state->IsOffTheRecord(), is_ntp,
        new_tab_page_uma::ACTION_OPENED_FOREIGN_SESSION);

    std::unique_ptr<web::WebState> web_state =
        session_util::CreateWebStateWithNavigationEntries(
            browser_state, session_tab->current_navigation_index,
            session_tab->navigations);
    web_state_list->ReplaceWebStateAt(web_state_list->active_index(),
                                      std::move(web_state));
  }
}

void TabResumptionHelper::SetDelegate(
    id<TabResumptionHelperDelegate> delegate) {
  delegate_ = delegate;
  if (delegate_) {
    LastTabResumptionItem();
  }
}

void TabResumptionHelper::OnStateChanged(syncer::SyncService* sync) {
  // If tabs are not synced, hide the tab resumption tile.
  if (!sync_service_->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kTabs)) {
    [delegate_ removeTabResumptionModule];
  }
}

void TabResumptionHelper::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kCleared: {
      // If the user is signed out, remove the tab resumption tile.
      [delegate_ removeTabResumptionModule];
      break;
    }
    default:
      break;
  }
}

#pragma mark - Private

void TabResumptionHelper::ForeignSessionsChanged() {
  LastTabResumptionItem();
}

void TabResumptionHelper::FetchFaviconForItem(TabResumptionItem* item,
                                              FaviconLoader* favicon_loader) {
  favicon_loader->FaviconForPageUrl(
      item.tabURL, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/true,
      base::CallbackToBlock(
          base::BindRepeating(&TabResumptionHelper::OnFaviconForPageUrl,
                              weak_ptr_factory_.GetWeakPtr(), item)));
}

void TabResumptionHelper::OnFaviconForPageUrl(TabResumptionItem* item,
                                              FaviconAttributes* attributes) {
  if (!attributes.usesDefaultImage) {
    item.faviconImage = attributes.faviconImage;
    tab_resumption_item_ = item;
    [delegate_ tabResumptionHelperDidReceiveItem];
  }
}

void TabResumptionHelper::LastSyncedTabItemFromLastActiveDistantTab(
    const synced_sessions::DistantSession* session,
    const synced_sessions::DistantTab* tab,
    FaviconLoader* favicon_loader) {
  CHECK(!IsTabResumptionEnabledForMostRecentTabOnly());

  TabResumptionItem* tab_resumption_item = [[TabResumptionItem alloc]
      initWithItemType:TabResumptionItemType::kLastSyncedTab];
  tab_resumption_item.sessionName = base::SysUTF8ToNSString(session->name);
  tab_resumption_item.tabTitle = base::SysUTF16ToNSString(tab->title);
  tab_resumption_item.syncedTime = tab->last_active_time;
  tab_resumption_item.tabURL = tab->virtual_url;

  // Fetch the favicon.
  FetchFaviconForItem(tab_resumption_item, favicon_loader);
}

void TabResumptionHelper::MostRecentTabItemFromWebState(
    web::WebState* web_state,
    base::Time opened_time,
    FaviconLoader* favicon_loader) {
  TabResumptionItem* tab_resumption_item = [[TabResumptionItem alloc]
      initWithItemType:TabResumptionItemType::kMostRecentTab];
  tab_resumption_item.tabTitle =
      base::SysUTF16ToNSString(web_state->GetTitle());
  tab_resumption_item.syncedTime = opened_time;
  tab_resumption_item.tabURL = web_state->GetLastCommittedURL();

  // Fetch the favicon.
  FetchFaviconForItem(tab_resumption_item, favicon_loader);
}

void TabResumptionHelper::MostRecentTabRemoved(web::WebState* web_state) {
  if (tab_resumption_item_ && tab_resumption_item_.itemType == kMostRecentTab) {
    [delegate_ removeTabResumptionModule];
  }
}
