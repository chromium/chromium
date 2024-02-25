// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_OVERLAY_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_OVERLAY_TAB_HELPER_H_

#import "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/infobars/core/infobar_manager.h"
#import "ios/web/public/web_state_user_data.h"

class InfobarOverlayRequestInserter;

// Helper class that creates OverlayRequests for the banner UI for InfoBars
// added to an InfoBarManager.
class InfobarOverlayTabHelper
    : public web::WebStateUserData<InfobarOverlayTabHelper> {
 public:
  ~InfobarOverlayTabHelper() override;

 private:
  friend class web::WebStateUserData<InfobarOverlayTabHelper>;
  InfobarOverlayTabHelper(web::WebState* web_state);
  WEB_STATE_USER_DATA_KEY_DECL();

  // Getter for the request inserter.
  InfobarOverlayRequestInserter* request_inserter() {
    return request_inserter_;
  }

  // Helper object that schedules OverlayRequests for the banner UI for InfoBars
  // added to a WebState's InfoBarManager.
  class OverlayRequestScheduler : public infobars::InfoBarManager::Observer {
   public:
    OverlayRequestScheduler(web::WebState* web_state,
                            InfobarOverlayTabHelper* tab_helper);
    ~OverlayRequestScheduler() override;

   private:
    // infobars::InfoBarManager::Observer:
    void OnInfoBarAdded(infobars::InfoBar* infobar) override;
    void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

   private:
    // The owning tab helper.
    raw_ptr<InfobarOverlayTabHelper> tab_helper_ = nullptr;
    raw_ptr<web::WebState> web_state_ = nullptr;
    base::ScopedObservation<infobars::InfoBarManager,
                            infobars::InfoBarManager::Observer>
        scoped_observation_{this};
  };

  // The inserter used to add infobar OverlayRequests to the WebState's queue.
  raw_ptr<InfobarOverlayRequestInserter> request_inserter_;
  // The scheduler used to create OverlayRequests for InfoBars added to the
  // corresponding WebState's InfoBarManagerImpl.
  OverlayRequestScheduler request_scheduler_;
};
#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_INFOBAR_OVERLAY_TAB_HELPER_H_
