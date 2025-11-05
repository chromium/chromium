// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_RECENT_TAB_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_RECENT_TAB_BROWSER_AGENT_H_

#import "base/memory/raw_ptr.h"
#import "base/observer_list.h"
#import "base/scoped_observation.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_recent_tab_observer.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

class Browser;

// Browser Agent that manages the most recent WebState for the Start Surface and
// listens to WebStateListObserver for instances of that WebState's removal and
// updates to the current page's favicon for that WebState.
class StartSurfaceRecentTabBrowserAgent
    : public BrowserUserData<StartSurfaceRecentTabBrowserAgent>,
      public web::WebStateObserver,
      public favicon::FaviconDriverObserver,
      public TabsDependencyInstaller {
 public:
  // Notifies the Browser Agent to save the most recent WebState.
  void SaveMostRecentTab();

  // Returns the most recent WebState.
  web::WebState* most_recent_tab() { return most_recent_tab_; }

  // Add/Remove observers for this Browser Agent.
  void AddObserver(StartSurfaceRecentTabObserver* observer);
  void RemoveObserver(StartSurfaceRecentTabObserver* observer);

  ~StartSurfaceRecentTabBrowserAgent() override;

  // Not copyable or moveable.
  StartSurfaceRecentTabBrowserAgent(const StartSurfaceRecentTabBrowserAgent&) =
      delete;
  StartSurfaceRecentTabBrowserAgent& operator=(
      const StartSurfaceRecentTabBrowserAgent&) = delete;

  // TabsDependencyInstaller implementation:
  void OnWebStateInserted(web::WebState* web_state) override;
  void OnWebStateRemoved(web::WebState* web_state) override;
  void OnWebStateDeleted(web::WebState* web_state) override;
  void OnActiveWebStateChanged(web::WebState* old_active,
                               web::WebState* new_active) override;

 private:
  friend class BrowserUserData<StartSurfaceRecentTabBrowserAgent>;

  // Constructor used by CreateForBrowser().
  explicit StartSurfaceRecentTabBrowserAgent(Browser* browser);

  // web::WebStateObserver
  void WebStateDestroyed(web::WebState* web_state) override;
  void TitleWasSet(web::WebState* web_state) override;

  // favicon::FaviconDriverObserver
  void OnFaviconUpdated(favicon::FaviconDriver* driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

  // Called when the most recent tab changes.
  void SetMostRecentTab(web::WebState* web_state);

  // A list of observers notified when the most recent tab is removed. Weak
  // references.
  base::ObserverList<StartSurfaceRecentTabObserver, true> observers_;

  // The most recent tab managed by this Browser Agent.
  raw_ptr<web::WebState> most_recent_tab_ = nullptr;

  // Manages observation relationship between `this` and web::WebState.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      most_recent_tab_observation_{this};

  // Manages observation relationship between `this` and favicon::FaviconDriver.
  base::ScopedObservation<favicon::FaviconDriver,
                          favicon::FaviconDriverObserver>
      favicon_driver_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_RECENT_TAB_BROWSER_AGENT_H_
