// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_WEB_NAVIGATION_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_WEB_NAVIGATION_BROWSER_AGENT_H_

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/web/common/user_agent.h"

class Browser;
@protocol WebNavigationNTPDelegate;
class WebStateList;

namespace web {
class WebState;
}

// A browser agent that encapsulates logic for common web navigation tasks on
// the current active web state in the associated browser.
class WebNavigationBrowserAgent
    : public BrowserUserData<WebNavigationBrowserAgent> {
 public:
  // Not copyable or moveable.
  WebNavigationBrowserAgent(const WebNavigationBrowserAgent&) = delete;
  WebNavigationBrowserAgent& operator=(const WebNavigationBrowserAgent&) =
      delete;
  ~WebNavigationBrowserAgent() override;

  // Sets an optional delegate if NTP navigation needs to be handled. If this
  // is not set, no special considerations for the NTP are made.
  void SetDelegate(id<WebNavigationNTPDelegate> delegate);

  // All of the following methods will silently no-op (or return false) if there
  // is no active web state in the associated browser's WebStateList.

  // True if the given `web_state` can navigate back.
  bool CanGoBack(const web::WebState* web_state);
  // True if it is possible to navigate back.
  bool CanGoBack();
  // Navigates back.
  void GoBack();
  // True if the given `web_state` can navigate forward.
  bool CanGoForward(const web::WebState* web_state);
  // True if it is possible to navigate forward.
  bool CanGoForward();
  // Navigates forward.
  void GoForward();
  // Stops the active web state's loading.
  void StopLoading();
  // Reloads the active web state.
  void Reload();
  // Requests the "desktop" version of the current page in the active tab
  void RequestDesktopSite();
  // Requests the "mobile" version of the current page in the active tab.
  void RequestMobileSite();

 private:
  friend class BrowserUserData<WebNavigationBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit WebNavigationBrowserAgent(Browser* browser);

  // Reloads the original url of the last non-redirect item (including
  // non-history items) with `userAgentType`.
  void ReloadWithUserAgentType(web::UserAgentType userAgentType);
  // Return the UserAgentType for a given `web_state`.
  web::UserAgentType UserAgentType(web::WebState* web_state);

  // The web state list for the associated browser. This should never be
  // null.
  raw_ptr<WebStateList> web_state_list_;
  // The delegate, if assigned. This may be nil.
  id<WebNavigationNTPDelegate> delegate_;
  // The associated browser.
  raw_ptr<Browser> browser_;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_WEB_NAVIGATION_BROWSER_AGENT_H_
