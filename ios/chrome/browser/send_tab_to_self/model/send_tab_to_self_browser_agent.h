// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_BROWSER_AGENT_H_

#import <CoreFoundation/CoreFoundation.h>

#import <string>

#import "base/containers/span.h"
#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/send_tab_to_self/send_tab_to_self_model_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/url_loading/model/url_loading_observer.h"
#import "ios/web/public/web_state_observer.h"

class UrlLoadingNotifierBrowserAgent;
struct UrlLoadParams;

namespace web {
class WebState;
}

namespace send_tab_to_self {
class SendTabToSelfEntry;
class SendTabToSelfModel;
}  // namespace send_tab_to_self

// Service that listens for SendTabToSelf model changes and calls UI
// handlers to update the UI accordingly.
// TODO(crbug.com/519101926): Consider refactoring this class to implement
// send_tab_to_self::ReceivingUiHandler once that interface is moved to
// components/send_tab_to_self, unifying the receiving flow.
class SendTabToSelfBrowserAgent
    : public BrowserUserData<SendTabToSelfBrowserAgent>,
      public send_tab_to_self::SendTabToSelfModelObserver,
      public WebStateListObserver,
      public web::WebStateObserver,
      public UrlLoadingObserver,
      public BrowserObserver {
 public:
  ~SendTabToSelfBrowserAgent() override;

  // BrowserObserver::
  void BrowserDestroyed(Browser* browser) override;

  // SendTabToSelfModelObserver::

  // Updates the UI to reflect the new entries. Calls the handlers
  // registered through ReceivingUIRegistry.
  void OnEntriesAddedRemotely(
      base::span<const send_tab_to_self::SendTabToSelfEntry* const> new_entries)
      override;
  // Updates the UI to reflect the removal of entries. Calls the handlers
  // registered through ReceivingUIRegistry.
  void OnEntriesRemovedRemotely(base::span<const std::string> guids) override;

  // WebStateListObserver::
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // WebStateObserver::
  void WasShown(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // UrlLoadingObserver::
  void TabWillLoadUrl(const UrlLoadParams& params,
                      base::WeakPtr<web::WebState> web_state) override;

 private:
  friend class BrowserUserData<SendTabToSelfBrowserAgent>;

  explicit SendTabToSelfBrowserAgent(Browser* browser);

  // Display an infobar for `entry` on the specified `web_state`.
  void DisplayInfoBar(web::WebState* web_state,
                      const send_tab_to_self::SendTabToSelfEntry* entry);

  // Stop observing the WebState and WebStateList and reset associated
  // variables.
  void CleanUpObserversAndVariables();

  // Owned by the SendTabToSelfSyncService which should outlive this class
  raw_ptr<send_tab_to_self::SendTabToSelfModel> model_;

  // The pending SendTabToSelf entry to display an InfoBar for.
  raw_ptr<const send_tab_to_self::SendTabToSelfEntry> pending_entry_ = nullptr;

  // The WebState that is being observed for activation, if any.
  raw_ptr<web::WebState> pending_web_state_ = nullptr;

  base::ScopedObservation<Browser, BrowserObserver> browser_observation_{this};

  base::ScopedObservation<UrlLoadingNotifierBrowserAgent, UrlLoadingObserver>
      url_loading_observation_{this};

  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};

  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  base::ScopedObservation<send_tab_to_self::SendTabToSelfModel,
                          send_tab_to_self::SendTabToSelfModelObserver>
      model_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_SEND_TAB_TO_SELF_MODEL_SEND_TAB_TO_SELF_BROWSER_AGENT_H_
