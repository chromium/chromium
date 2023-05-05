// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_TAB_INSERTION_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_TAB_INSERTION_BROWSER_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/web/public/navigation/navigation_manager.h"

namespace web {
class WebState;
}

class ChromeBrowserState;
class WebStateList;

namespace TabInsertion {

// Position the tab automatically. This value is used as index parameter in
// methods that require an index when the caller doesn't have a preference
// on the position where the tab will be open.
int const kPositionAutomatically = -1;

}  // namespace TabInsertion

class TabInsertionBrowserAgent
    : public BrowserUserData<TabInsertionBrowserAgent> {
 public:
  ~TabInsertionBrowserAgent() override;

  web::WebState* InsertWebState(
      const web::NavigationManager::WebLoadParams& params,
      web::WebState* parent,
      bool opened_by_dom,
      int index,
      bool in_background,
      bool inherit_opener,
      bool should_show_start_surface,
      bool should_skip_new_tab_animation);

  web::WebState* InsertWebStateOpenedByDOM(web::WebState* parent);

 private:
  friend class BrowserUserData<TabInsertionBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit TabInsertionBrowserAgent(Browser* browser);

  ChromeBrowserState* browser_state_;
  WebStateList* web_state_list_;
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_TAB_INSERTION_BROWSER_AGENT_H_
