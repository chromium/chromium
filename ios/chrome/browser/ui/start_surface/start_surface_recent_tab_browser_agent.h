// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_START_SURFACE_START_SURFACE_RECENT_TAB_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_UI_START_SURFACE_START_SURFACE_RECENT_TAB_BROWSER_AGENT_H_

#import "base/observer_list.h"
#import "base/scoped_observation.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/main/browser_observer.h"
#import "ios/chrome/browser/main/browser_user_data.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state_observer.h"

namespace web {
class WebState;
}  // namespace web

class Browser;

// Interface for listening to updates to the most recent tab.
class StartSurfaceRecentTabObserver {
 public:
  StartSurfaceRecentTabObserver() {}

  // Not copyable or moveable.
  StartSurfaceRecentTabObserver(const StartSurfaceRecentTabObserver&) = delete;
  StartSurfaceRecentTabObserver& operator=(
      const StartSurfaceRecentTabObserver&) = delete;

  // Notifies the receiver that the most recent tab was removed.
  virtual void MostRecentTabRemoved(web::WebState* web_state) {}
  // Notifies the receiver that the favicon for the current page of the most
  // recent tab was updated to `image`.
  virtual void MostRecentTabFaviconUpdated(UIImage* image) {}

  virtual void MostRecentTabTitleUpdated(const std::u16string& title) {}

 protected:
  virtual ~StartSurfaceRecentTabObserver() = default;
};

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
  void WebStateDetachedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index) override;

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
  base::ObserverList<StartSurfaceRecentTabObserver, true>::Unchecked observers_;
  // Manages observation relationship between `this` and favicon::FaviconDriver.
  base::ScopedObservation<favicon::FaviconDriver,
                          favicon::FaviconDriverObserver>
      favicon_driver_observer_{this};
  // Manages observation relationship between `this` and web::WebState.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
  // The most recent tab managed by this Browser Agent.
  web::WebState* most_recent_tab_ = nullptr;
  // Browser.
  Browser* browser_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_UI_START_SURFACE_START_SURFACE_RECENT_TAB_BROWSER_AGENT_H_
