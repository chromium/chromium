// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BADGE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BADGE_TAB_HELPER_H_

#include <map>

#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "components/infobars/core/infobar_manager.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/badges/badge_item.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}

@protocol InfobarBadgeTabHelperDelegate;
@protocol LegacyInfobarBadgeTabHelperDelegate;
@class InfobarBadgeModel;

// TabHelper that observes InfoBarManager. It updates an InfobarBadge delegate
// for relevant Infobar changes.
class InfobarBadgeTabHelper
    : public web::WebStateUserData<InfobarBadgeTabHelper> {
 public:
  ~InfobarBadgeTabHelper() override;

  // Sets the InfobarBadgeTabHelperDelegate to |delegate|.
  void SetDelegate(id<InfobarBadgeTabHelperDelegate> delegate);
  // Updates Infobar badge for the case where an Infobar banner of
  // |infobar_type| was presented.
  void UpdateBadgeForInfobarBannerPresented(InfobarType infobar_type);
  // Updates Infobar badge for the case where an Infobar banner of
  // |infobar_type| was dismissed.
  void UpdateBadgeForInfobarBannerDismissed(InfobarType infobar_type);

  // Returns all BadgeItems for the TabHelper Webstate.
  NSArray<id<BadgeItem>>* GetInfobarBadgeItems();

  // DEPRECATED: The accept state of an infobar is now stored directly in
  // InfoBarIOS, and should be updated there rather than using these functions.
  void UpdateBadgeForInfobarAccepted(InfobarType infobar_type);
  void UpdateBadgeForInfobarReverted(InfobarType infobar_type);

 private:
  friend class web::WebStateUserData<InfobarBadgeTabHelper>;
  explicit InfobarBadgeTabHelper(web::WebState* web_state);

  // Notifies the tab helper to reset state for added or removed infobars with
  // |infobar_type|.  Only called for infobars that support badges.
  void ResetStateForAddedInfobar(InfobarType infobar_type);
  void ResetStateForRemovedInfobar(InfobarType infobar_type);
  // Notifies the tab helper that an infobar with |type| was accepted or
  // reverted.
  void OnInfobarAcceptanceStateChanged(InfobarType infobar_type, bool accepted);

  // Helper object that listens for accept and revert events for an InfoBarIOS.
  class InfobarAcceptanceObserver : public InfoBarIOS::Observer {
   public:
    explicit InfobarAcceptanceObserver(InfobarBadgeTabHelper* tab_helper);
    ~InfobarAcceptanceObserver() override;

    // Returns a reference to the scoped observations.
    base::ScopedMultiSourceObservation<InfoBarIOS, InfoBarIOS::Observer>&
    scoped_observations() {
      return scoped_observations_;
    }

   private:
    // InfoBarIOS::Observer:
    void DidUpdateAcceptedState(InfoBarIOS* infobar) override;
    void InfobarDestroyed(InfoBarIOS* infobar) override;

    // The owning tab helper.
    InfobarBadgeTabHelper* tab_helper_ = nullptr;
    // Scoped observer that facilitates observing InfoBarIOS objects.
    base::ScopedMultiSourceObservation<InfoBarIOS, InfoBarIOS::Observer>
        scoped_observations_{this};
  };

  // Helper object that updates state and adds an InfobarAcceptanceObserver
  // when each infobar is added or removed.
  class InfobarManagerObserver : public infobars::InfoBarManager::Observer {
   public:
    InfobarManagerObserver(InfobarBadgeTabHelper* tab_helper,
                           web::WebState* web_state,
                           InfobarAcceptanceObserver* infobar_accept_observer);
    ~InfobarManagerObserver() override;

   private:
    // InfoBarManagerObserver:
    void OnInfoBarAdded(infobars::InfoBar* infobar) override;
    void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
    void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                           infobars::InfoBar* new_infobar) override;
    void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

    // The owning tab helper.
    InfobarBadgeTabHelper* tab_helper_ = nullptr;
    // The infobar acceptance observer for |tab_helper_|.  Added to each infobar
    // in the observed manager.
    InfobarAcceptanceObserver* infobar_accept_observer_ = nullptr;
    // Scoped observer that facilitates observing an InfoBarManager.
    base::ScopedObservation<infobars::InfoBarManager,
                            infobars::InfoBarManager::Observer>
        scoped_observation_{this};
  };

  // Delegate which displays the Infobar badge.
  __weak id<InfobarBadgeTabHelperDelegate> delegate_ = nil;
  // The infobar accept/revert observer.
  InfobarAcceptanceObserver infobar_accept_observer_;
  // The infobar manager observer.
  InfobarManagerObserver infobar_manager_observer_;
  // The WebState this TabHelper is scoped to.
  web::WebState* web_state_;
  // Map storing the badge models for each InfobarType.
  std::map<InfobarType, InfobarBadgeModel*> infobar_badge_models_;

  WEB_STATE_USER_DATA_KEY_DECL();
  DISALLOW_COPY_AND_ASSIGN(InfobarBadgeTabHelper);
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BADGE_TAB_HELPER_H_
