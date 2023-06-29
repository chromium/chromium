// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_pickup/tab_pickup_infobar_delegate.h"

#import "components/infobars/core/infobar_delegate.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
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
    Browser* browser,
    const synced_sessions::DistantSession* session)
    : session_(session), browser_(browser) {
  DCHECK(IsTabPickupEnabled());

  favicon_loader_ = IOSChromeFaviconLoaderFactory::GetForBrowserState(
      browser_->GetBrowserState());

  const synced_sessions::DistantTab* tab = session_->tabs.front().get();

  session_name_ = session_->name;
  synced_time_ = session_->modified_time;
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

void TabPickupInfobarDelegate::OpenDistantTab() {
  // TODO(crbug.com/1457175): Implement this.
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
