// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_BLOCKED_POPUP_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_BLOCKED_POPUP_TAB_HELPER_H_

#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/infobars/core/infobar_manager.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/web/public/lazy_web_state_user_data.h"
#import "ios/web/public/navigation/referrer.h"
#import "url/gurl.h"

namespace infobars {
class InfoBar;
}  // namespace infobars

namespace web {
class WebState;
}  // namespace web

// Handles blocked popups. Will display an infobar informing the user and
// allowing the user to add an exception and navigate to the site.
class BlockedPopupTabHelper
    : public infobars::InfoBarManager::Observer,
      public web::LazyWebStateUserData<BlockedPopupTabHelper> {
 public:
  explicit BlockedPopupTabHelper(web::WebState* web_state);

  BlockedPopupTabHelper(const BlockedPopupTabHelper&) = delete;
  BlockedPopupTabHelper& operator=(const BlockedPopupTabHelper&) = delete;

  ~BlockedPopupTabHelper() override;

  // Returns true if popup requested by the page with the given `source_url`
  // should be blocked.
  bool ShouldBlockPopup(const GURL& source_url);

  // Shows the popup blocker infobar for the popup with given popup_url.
  // `referrer` represents the frame which requested this popup.
  void HandlePopup(const GURL& popup_url, const web::Referrer& referrer);

  // infobars::InfoBarManager::Observer implementation.
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnManagerShuttingDown(
      infobars::InfoBarManager* infobar_manager) override;

  // Encapsulates information about popup.
  struct Popup {
    Popup(const GURL& popup_url, const web::Referrer& referrer)
        : popup_url(popup_url), referrer(referrer) {}
    // URL of the popup window.
    const GURL popup_url;
    // Referrer which requested this popup.
    const web::Referrer referrer;
  };

 private:
  friend class web::LazyWebStateUserData<BlockedPopupTabHelper>;

  friend class BlockedPopupTabHelperTest;

  // Shows the infobar for the current popups. Will also handle replacing an
  // existing infobar with the updated count.
  void ShowInfoBar();

  // Returns the profile for the WebState that this object is attached to.
  ProfileIOS* GetProfile() const;

  // Registers this object as an observer for the InfoBarManager associated with
  // `web_state_`.  Does nothing if already registered.
  void RegisterAsInfoBarManagerObserverIfNeeded(
      infobars::InfoBarManager* infobar_manager);

  // The WebState that this object is attached to.
  raw_ptr<web::WebState> web_state_;
  // The currently displayed infobar.
  raw_ptr<infobars::InfoBar> infobar_;
  // The popups to open.
  std::vector<Popup> popups_;
  // For management of infobars::InfoBarManager::Observer registration.  This
  // object will not start observing the InfoBarManager until ShowInfoBars() is
  // called.
  base::ScopedObservation<infobars::InfoBarManager,
                          infobars::InfoBarManager::Observer>
      scoped_observation_{this};

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_BLOCKED_POPUP_TAB_HELPER_H_
