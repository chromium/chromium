// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_pickup/tab_pickup_infobar_delegate.h"

#import "base/metrics/histogram_functions.h"
#import "components/infobars/core/infobar_delegate.h"
#import "components/sync_sessions/open_tabs_ui_delegate.h"
#import "components/sync_sessions/session_sync_service.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#import "ios/chrome/browser/sessions/session_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/distant_session.h"
#import "ios/chrome/browser/synced_sessions/distant_tab.h"
#import "ios/chrome/browser/synced_sessions/synced_sessions.h"
#import "ios/chrome/browser/tabs/tab_pickup/features.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/web/public/web_state.h"

TabPickupInfobarDelegate::TabPickupInfobarDelegate(
    Browser* browser,
    const synced_sessions::DistantSession* session)
    : browser_(browser) {
  CHECK(IsTabPickupEnabled());
  CHECK(!IsTabPickupDisabledByUser());

  favicon_loader_ = IOSChromeFaviconLoaderFactory::GetForBrowserState(
      browser_->GetBrowserState());

  const synced_sessions::DistantTab* tab = session->tabs.front().get();

  session_name_ = session->name;
  synced_time_ = session->modified_time;
  tab_url_ = tab->virtual_url;
  tab_id_ = tab->tab_id;
  session_tag_ = tab->session_tag;
}

TabPickupInfobarDelegate::~TabPickupInfobarDelegate() = default;

#pragma mark - Public methods

void TabPickupInfobarDelegate::FetchFavIconImage(
    ProceduralBlock block_handler) {
  favicon_loader_->FaviconForPageUrl(
      tab_url_, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/true, ^(FaviconAttributes* attributes) {
        if (!attributes.usesDefaultImage) {
          favicon_image_ = attributes.faviconImage;
          block_handler();
        }
      });
}

void TabPickupInfobarDelegate::OpenDistantTab() {
  ChromeBrowserState* browser_state = browser_->GetBrowserState();
  WebStateList* web_state_list = browser_->GetWebStateList();

  sync_sessions::OpenTabsUIDelegate* open_tabs_delegate =
      SessionSyncServiceFactory::GetForBrowserState(browser_state)
          ->GetOpenTabsUIDelegate();

  const sessions::SessionTab* session_tab = nullptr;
  if (open_tabs_delegate->GetForeignTab(session_tag_, tab_id_, &session_tab)) {
    base::TimeDelta time_since_last_use = base::Time::Now() - synced_time_;
    base::UmaHistogramCustomTimes("IOS.DistantTab.TimeSinceLastUse",
                                  time_since_last_use, base::Minutes(1),
                                  base::Days(24), 50);
    base::UmaHistogramCustomTimes("IOS.TabPickup.TabOpened.TimeSinceLastUse",
                                  time_since_last_use, base::Minutes(1),
                                  base::Days(24), 50);

    new_tab_page_uma::RecordAction(
        browser_state->IsOffTheRecord(), web_state_list->GetActiveWebState(),
        new_tab_page_uma::ACTION_OPENED_FOREIGN_SESSION);

    std::unique_ptr<web::WebState> web_state =
        session_util::CreateWebStateWithNavigationEntries(
            browser_state, session_tab->current_navigation_index,
            session_tab->navigations);
    web_state_list->InsertWebState(
        web_state_list->count(), std::move(web_state),
        (WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE),
        WebStateOpener());
  }
}

void TabPickupInfobarDelegate::OpenTabPickupSettings() {
  id<ApplicationCommands> application_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), ApplicationCommands);
  [application_handler showTabPickupSettings];
}

#pragma mark - ConfirmInfoBarDelegate methods

// Returns an empty message to satisfy implementation requirement for
// ConfirmInfoBarDelegate.
std::u16string TabPickupInfobarDelegate::GetMessageText() const {
  return std::u16string();
}

infobars::InfoBarDelegate::InfoBarIdentifier
TabPickupInfobarDelegate::GetIdentifier() const {
  return TAB_PICKUP_INFOBAR_DELEGATE;
}

bool TabPickupInfobarDelegate::EqualsDelegate(
    infobars::InfoBarDelegate* delegate) const {
  return delegate->GetIdentifier() == GetIdentifier();
}
