// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_IOS_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_IOS_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"
#include "ios/web/public/web_state_observer.h"

namespace ios {
class ChromeBrowserState;
}

namespace web {
class WebState;
}

namespace send_tab_to_self {
class SendTabToSelfEntry;
class SendTabToSelfModel;

// Service that listens for SendTabToSelf model changes and calls UI
// handlers to update the UI accordingly.
class SendTabToSelfClientServiceIOS : public KeyedService,
                                      public SendTabToSelfModelObserver,
                                      public WebStateListObserver,
                                      public web::WebStateObserver {
 public:
  SendTabToSelfClientServiceIOS(ios::ChromeBrowserState* browser_state,
                                SendTabToSelfModel* model);
  ~SendTabToSelfClientServiceIOS() override;

  // SendTabToSelfModelObserver::
  // Keeps track of when the model is loaded so that updates to the
  // model can be pushed afterwards.
  void SendTabToSelfModelLoaded() override;
  // Updates the UI to reflect the new entries. Calls the handlers
  // registered through ReceivingUIRegistry.
  void EntriesAddedRemotely(
      const std::vector<const SendTabToSelfEntry*>& new_entries) override;
  // Updates the UI to reflect the removal of entries. Calls the handlers
  // registered through ReceivingUIRegistry.
  void EntriesRemovedRemotely(const std::vector<std::string>& guids) override;

  // WebStateListObserver::
  void WebStateActivatedAt(WebStateList* web_state_list,
                           web::WebState* old_web_state,
                           web::WebState* new_web_state,
                           int active_index,
                           int reason) override;

  // WebStateObserver::
  void WasShown(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  // Display an infobar for |entry| on the specified |web_state|.
  void DisplayInfoBar(web::WebState* web_state,
                      const SendTabToSelfEntry* entry);

  // Stop observing the WebState and WebStateList and reset associated
  // variables.
  void CleanUpObserversAndVariables();

  // Owned by the SendTabToSelfSyncService which should outlive this class
  SendTabToSelfModel* model_;

  // The current browser state. Must outlive this object.
  ios::ChromeBrowserState* browser_state_;

  // The pending SendTabToSelf entry to display an InfoBar for.
  const SendTabToSelfEntry* entry_ = nullptr;

  // The WebStateList that is being observed.
  WebStateList* web_state_list_ = nullptr;

  // The WebState that is being observed.
  web::WebState* web_state_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfClientServiceIOS);
};

}  // namespace send_tab_to_self

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_CLIENT_SERVICE_IOS_H_
