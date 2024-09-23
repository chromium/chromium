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
#import "ios/web/public/web_state_observer.h"

namespace web {
class WebState;
}  // namespace web

class Browser;

// Browser Agent that manages the most recent WebState for the Start Surface and
// listens to WebStateListObserver for instances of that WebState's removal and
// updates to the current page's favicon for that WebState.
class StartSurfaceRecentTabBrowserAgent
    : public BrowserUserData<StartSurfaceRecentTabBrowserAgent>,
      public BrowserObserver,
      public WebStateListObserver,
      public web::WebStateObserver,
      public favicon::FaviconDriverObserver {
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

 private:
  friend class BrowserUserData<StartSurfaceRecentTabBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  // Constructor used by CreateForBrowser().
  explicit StartSurfaceRecentTabBrowserAgent(Browser* browser);

  // BrowserObserver
  void BrowserDestroyed(Browser* browser) override;

  // WebStateListObserver:
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // web::WebStateObserver
  void WebStateDestroyed(web::WebState* web_state) override;
  void TitleWasSet(web::WebState* web_state) override;

  // favicon::FaviconDriverObserver
  void OnFaviconUpdated(favicon::FaviconDriver* driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

  // A list of observers notified when the most recent tab is removed. Weak
  // references.
  base::ObserverList<StartSurfaceRecentTabObserver, true> observers_;
  // Manages observation relationship between `this` and favicon::FaviconDriver.
  base::ScopedObservation<favicon::FaviconDriver,
                          favicon::FaviconDriverObserver>
      favicon_driver_observer_{this};
  // Manages observation relationship between `this` and web::WebState.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
  // The most recent tab managed by this Browser Agent.
  raw_ptr<web::WebState> most_recent_tab_ = nullptr;
  // Browser.
  raw_ptr<Browser> browser_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_RECENT_TAB_BROWSER_AGENT_H_
