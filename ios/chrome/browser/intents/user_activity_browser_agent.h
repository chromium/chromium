// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTENTS_USER_ACTIVITY_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_INTENTS_USER_ACTIVITY_BROWSER_AGENT_H_

#import <UIKit/UIKit.h>

#import "base/scoped_observation.h"
#import "base/sequence_checker.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/browser/shared/coordinator/scene/connection_information.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "url/gurl.h"

// This browser agent handles user intents events.
class UserActivityBrowserAgent
    : public BrowserUserData<UserActivityBrowserAgent> {
 public:
  ~UserActivityBrowserAgent() override;

  // Not copyable or assignable.
  UserActivityBrowserAgent(const UserActivityBrowserAgent&) = delete;
  UserActivityBrowserAgent& operator=(const UserActivityBrowserAgent&) = delete;

  // If the userActivity is a Handoff or an opening from Spotlight, opens a new
  // tab or setup startupParameters to open it later. If a new tab must be
  // opened immediately (e.g. if a Siri Shortcut was triggered by the user while
  // Chrome was already in the foreground), it will be done with the provided
  // `profile`. Returns whether it could continue userActivity.
  BOOL ContinueUserActivity(NSUserActivity* user_activity,
                            BOOL application_is_active);

  // Handles the 3D touch application static items.
  BOOL Handle3DTouchApplicationShortcuts(
      UIApplicationShortcutItem* shortcut_item);

  // Opens a new Tab or routes to correct Tab.
  void RouteToCorrectTab();

  // Return YES if the user intends to open links in a certain mode and the
  // browser will proceed the request.
  BOOL ProceedWithUserActivity(NSUserActivity* user_activity);

 private:
  friend class BrowserUserData<UserActivityBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit UserActivityBrowserAgent(Browser* browser);

  // Private helper methods.
  //
  // Returns an app startup parameter for opening a new tab with a post action.
  AppStartupParameters* StartupParametersForOpeningNewTab(
      TabOpeningPostOpeningAction action);

  // Handles the 3D touch application static items. Does nothing if in first
  // run.
  BOOL HandleShortcutItem(UIApplicationShortcutItem* shortcut_item);

  // Open the requested URLs if the app is active. If the app is not active,
  // updates the startupParameters if needed.
  void OpenRequestedURLs(const std::vector<GURL>& webpage_urls,
                         BOOL application_is_active,
                         BOOL incognito);

  // Checks if a new tab must be opened immediately. If the app is not active,
  // updates the startupParameters if needed. Returns whether it could continue
  // userActivity.
  BOOL ContinueUserActivityURL(NSURL* webpage_url,
                               BOOL application_is_active,
                               BOOL open_existing_tab);

  // Opens multiple tabs.
  void OpenMultipleTabs();

  // Returns the GURL coming from the search query.
  GURL GenerateResultGURLFromSearchQuery(NSString* search_query);

  // Overload of `ContinueUserActivityURL(...)` that computes `is_active` from
  // `UIApplication`.
  void OverloadContinueUserActivityURL(BOOL open_existing_tab,
                                       NSURL* webpage_url);

  // Clears startup parameters.
  void ClearStartupParameters();

  SEQUENCE_CHECKER(sequence_checker_);

  // The browser associated with this agent.
  raw_ptr<Browser> browser_ = nullptr;

  // The ProfileIOS associated to the browser.
  raw_ptr<ProfileIOS> profile_ = nullptr;

  // Contains information about the initialization of scenes.
  __weak id<ConnectionInformation> connection_information_;

  // Container for startup information.
  __weak id<StartupInformation> startup_information_;

  // Tab opener to be used to open a new tab.
  __weak id<TabOpening> tab_opener_;

  // Weak pointer factory.
  base::WeakPtrFactory<UserActivityBrowserAgent> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INTENTS_USER_ACTIVITY_BROWSER_AGENT_H_
