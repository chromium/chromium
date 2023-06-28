// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_pickup/tab_pickup_infobar_delegate.h"

#import "components/infobars/core/infobar_delegate.h"
#import "components/sync_sessions/session_sync_service.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/sync/session_sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/distant_session.h"
#import "ios/chrome/browser/synced_sessions/distant_tab.h"
#import "ios/chrome/browser/synced_sessions/synced_sessions.h"
#import "ios/chrome/browser/tabs/tab_pickup/features.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TabPickupInfobarDelegate::TabPickupInfobarDelegate(
    ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {
  DCHECK(IsTabPickupEnabled());

  favicon_loader_ =
      IOSChromeFaviconLoaderFactory::GetForBrowserState(browser_state_);

  synced_sessions_ = std::make_unique<synced_sessions::SyncedSessions>(
      SessionSyncServiceFactory::GetForBrowserState(browser_state_));
  synced_sessions::DistantSession const* session =
      synced_sessions_->GetSession(0);
  const synced_sessions::DistantTab* tab = session->tabs.front().get();

  session_name_ = session->name;
  synced_time_ = session->modified_time;
  tab_url_ = tab->virtual_url;
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
