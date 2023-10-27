// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_TAB_PICKUP_TAB_PICKUP_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_TAB_PICKUP_TAB_PICKUP_INFOBAR_DELEGATE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"

class Browser;
class FaviconLoader;
class GURL;

namespace synced_sessions {
struct DistantSession;
struct DistantTab;
}

// An interface derived from ConfirmInfoBarDelegate for the Tab Pickup InfoBar.
class TabPickupInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  TabPickupInfobarDelegate(Browser* browser,
                           const synced_sessions::DistantSession* session,
                           const synced_sessions::DistantTab* tab);

  ~TabPickupInfobarDelegate() override;

  // Fetches the favicon image and executes the given procedural block.
  void FetchFavIconImage(ProceduralBlock block_handler);

  // Opens the last synced tab from another device.
  void OpenDistantTab();

  // Getters.
  const std::string GetSessionName() const { return session_name_; }
  const GURL& GetTabURL() const { return tab_url_; }
  const base::Time& GetSyncedTime() const { return synced_time_; }
  UIImage* GetFaviconImage() { return favicon_image_; }

  // ConfirmInfoBarDelegate implementation.
  std::u16string GetMessageText() const override;
  InfoBarIdentifier GetIdentifier() const override;
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override;

 private:
  // Session name.
  std::string session_name_;
  // Time the session is last modified.
  base::Time synced_time_;
  // Las synced tab URL.
  GURL tab_url_;
  // Tab identifier.
  SessionID tab_id_ = SessionID::InvalidValue();
  // Distant session tag.
  std::string session_tag_;
  // Favicon of the last synced tab.
  UIImage* favicon_image_ = nullptr;

  // The owning Browser.
  raw_ptr<Browser> browser_ = nullptr;
  // Loads favicons.
  raw_ptr<FaviconLoader> favicon_loader_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_PERMISSIONS_PERMISSIONS_INFOBAR_DELEGATE_H_
