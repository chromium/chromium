// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PERMISSIONS_MODEL_PERMISSIONS_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_PERMISSIONS_MODEL_PERMISSIONS_TAB_HELPER_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "base/timer/timer.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/infobars/core/infobar.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

class InfobarOverlayRequestInserter;
class OverlayRequestQueue;

namespace base {
class OneShotTimer;
}  // namespace base

// Tab helper that observes changes to web permissions and creates/replaces the
// respective infobar accordingly.
class PermissionsTabHelper
    : public infobars::InfoBarManager::Observer,
      public web::WebStateObserver,
      public web::WebStateUserData<PermissionsTabHelper> {
 public:
  explicit PermissionsTabHelper(web::WebState* web_state);

  PermissionsTabHelper(const PermissionsTabHelper&) = delete;
  PermissionsTabHelper& operator=(const PermissionsTabHelper&) = delete;
  ~PermissionsTabHelper() override;

  // Present a dialog that asks the user whether the web state is allowed to
  // access `permissions` on the device.
  void PresentPermissionsDecisionDialogWithCompletionHandler(
      NSArray<NSNumber*>* permissions,
      web::WebStatePermissionDecisionHandler handler);

  // web::WebStateObserver implementation.
  void PermissionStateChanged(web::WebState* web_state,
                              web::Permission permission) override;

  void WebStateDestroyed(web::WebState* web_state) override;

  // infobars::InfoBarManager::Observer implementation.
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

 private:
  friend class web::WebStateUserData<PermissionsTabHelper>;

  // Adds/replaces the infobar and show the banner.
  void ShowInfoBar();

  // Update the acceptance of the infobar.
  void UpdateIsInfoBarAccepted();

  // The WebState that this object is attached to.
  raw_ptr<web::WebState> web_state_;

  // The currently displayed infobar.
  raw_ptr<infobars::InfoBar> infobar_ = nullptr;

  // Permissions that have changed its state from NotAccessible to Allowed
  // within a given timeout period.
  NSMutableArray<NSNumber*>* recently_accessible_permissions_ =
      [NSMutableArray array];

  // Timer used to for recently_accessible_permissions_.
  base::OneShotTimer timer_;

  // A mapping of current permissions to their states used to detect changes.
  NSMutableDictionary<NSNumber*, NSNumber*>* permissions_to_state_;

  // Scoped observer that facilitates observing the infobar manager.
  base::ScopedObservation<infobars::InfoBarManager,
                          infobars::InfoBarManager::Observer>
      infobar_manager_scoped_observation_{this};

  // Banner queue for the TabHelper's WebState;
  raw_ptr<OverlayRequestQueue> banner_queue_ = nullptr;

  // Request inserter for the TabHelper's WebState;
  raw_ptr<InfobarOverlayRequestInserter> inserter_ = nullptr;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_PERMISSIONS_MODEL_PERMISSIONS_TAB_HELPER_H_
