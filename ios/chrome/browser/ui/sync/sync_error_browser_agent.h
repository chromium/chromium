// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SYNC_SYNC_ERROR_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_UI_SYNC_SYNC_ERROR_BROWSER_AGENT_H_

#import "ios/chrome/browser/main/browser_observer.h"
#import "ios/chrome/browser/main/browser_user_data.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"

class Browser;
@protocol SigninPresenter;
@protocol SyncPresenter;

// Browser agent that is responsible for displaying sync errors.
class SyncErrorBrowserAgent : public BrowserObserver,
                              public WebStateListObserver,
                              public BrowserUserData<SyncErrorBrowserAgent> {
 public:
  SyncErrorBrowserAgent(const SyncErrorBrowserAgent&) = delete;
  SyncErrorBrowserAgent& operator=(const SyncErrorBrowserAgent&) = delete;

  ~SyncErrorBrowserAgent() override;

  // Sets the UI providers to present sign in and sync UI when needed.
  void SetUIProviders(id<SigninPresenter> signin_presenter_provider,
                      id<SyncPresenter> sync_presenter_provider);

  // Clears the UI providers.
  void ClearUIProviders();

 private:
  explicit SyncErrorBrowserAgent(Browser* browser);
  friend class BrowserUserData<SyncErrorBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  // BrowserObserver methods
  void BrowserDestroyed(Browser* browser) override;

  // WebStateListObserver methods
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index,
                          bool activating) override;

  // Returns the state of the Browser
  ChromeBrowserState* GetBrowserState();

  Browser* browser_ = nullptr;

  // Provider to a SignIn presenter
  __weak id<SigninPresenter> signin_presenter_provider_;
  // Provider to a Sync presenter
  __weak id<SyncPresenter> sync_presenter_provider_;
};

#endif  // IOS_CHROME_BROWSER_UI_SYNC_SYNC_ERROR_BROWSER_AGENT_H_
