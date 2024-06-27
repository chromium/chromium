// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_LOCAL_UPDATE_OBSERVER_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_LOCAL_UPDATE_OBSERVER_H_

#import "base/memory/raw_ptr.h"
#import "base/scoped_multi_source_observation.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_observer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

namespace tab_groups {

class TabGroupSyncService;

// Service for propagating local updates of Tab Group to the sync service.
class TabGroupLocalUpdateObserver : public BrowserListObserver,
                                    public WebStateListObserver,
                                    public web::WebStateObserver {
 public:
  // Ignores the synchronisation of `web_state` for its first navigation.
  void IgnoreNavigationForWebState(web::WebState* web_state);

  // BrowserListObserver.
  void OnBrowserAdded(const BrowserList* browser_list,
                      Browser* browser) override;
  void OnBrowserRemoved(const BrowserList* browser_list,
                        Browser* browser) override;
  void OnBrowserListShutdown(BrowserList* browser_list) override;

  // WebStateListObserver.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;
  void BatchOperationEnded(WebStateList* web_state_list) override;
  void WebStateListDestroyed(WebStateList* web_state_list) override;

  // WebStateObserver.
  void TitleWasSet(web::WebState* web_state) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  TabGroupLocalUpdateObserver(BrowserList* browser_list,
                             TabGroupSyncService* sync_service);
  ~TabGroupLocalUpdateObserver() override;

  TabGroupLocalUpdateObserver(const TabGroupLocalUpdateObserver&) = delete;
  TabGroupLocalUpdateObserver& operator=(const TabGroupLocalUpdateObserver&) =
      delete;

 private:
  // Start observing the web state list associated with `browser`, if browser is
  // active and its webstates.
  void StartObservingBrowser(Browser* browser);
  // Stop observing the `web_state_list` and its web states.
  void StopObservingWebStateList(WebStateList* web_state_list);

  // Start/stop observing the web state.
  void StartObservingWebState(web::WebState* web_state);
  void StopObservingWebState(web::WebState* web_state);

  raw_ptr<TabGroupSyncService> sync_service_ = nullptr;
  raw_ptr<BrowserList> browser_list_ = nullptr;

  // Vector that contains identifiers that should be ignored for their first
  // navigation.
  std::set<web::WebStateID> ignored_identifiers_;

  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
  base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};
  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};
};

}  // namespace tab_groups

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_LOCAL_UPDATE_OBSERVER_H_
