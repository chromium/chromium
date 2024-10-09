// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_INSERTION_MODEL_TAB_INSERTION_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_TAB_INSERTION_MODEL_TAB_INSERTION_BROWSER_AGENT_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/web/public/navigation/navigation_manager.h"

namespace web {
class WebState;
}

namespace TabInsertion {

// Position the tab automatically. This value is used as index parameter in
// methods that require an index when the caller doesn't have a preference
// on the position where the tab will be open.
int const kPositionAutomatically = -1;

// Parameters defining how a new tab is inserted.
struct Params {
  Params();
  ~Params();

  // The opener web state of the inserted tab.
  raw_ptr<web::WebState> parent = nullptr;

  // Whether the page is opened by DOM.
  bool opened_by_dom = false;

  // The preferred index of the inserted tab.
  int index = TabInsertion::kPositionAutomatically;

  // Whether the tab would be loaded instantly. If `false`, it would not be
  // loaded until opened.
  bool instant_load = true;

  // If `true`, the tab would be opened in the background, otherwise it would be
  // opened right after inserted.
  bool in_background = false;

  // If set to `true`, the opener of the inserted tab would be the current tab,
  // otherwise the parent should be explicitly defined by `parent`.
  bool inherit_opener = false;

  // If set to `true`, the inserted tab will be configured to show the start
  // surface. This is usually set when loading "chrome://newtab".
  bool should_show_start_surface = false;

  // If set to `true`, new tab animation would be skipped. An example case when
  // this field is `true` is opening the new tab from external intents.
  bool should_skip_new_tab_animation = false;

  // Tentative title of the inserted tab before the tab URL is loaded.
  // Note: Currently only applies to web states not instant-loaded.
  std::u16string placeholder_title;

  // Whether the inserted tab is pinned tab.
  bool insert_pinned = false;

  // Whether the inserted tab is in a group.
  bool insert_in_group = false;

  // The tab group where the tab should be inserted (if null and
  // `insert_in_group` the tab is inserted in a new tab group).
  base::WeakPtr<const TabGroup> tab_group;
};

}  // namespace TabInsertion

// Browser agent handling all tab insertion logic.
class TabInsertionBrowserAgent
    : public BrowserUserData<TabInsertionBrowserAgent> {
 public:
  ~TabInsertionBrowserAgent() override;

  // Opens a tab at the specified URL in `web_load_params`. For certain
  // transition types, will consult the order controller and thus may only use
  // `tab_insertion_params.index` as a hint.
  web::WebState* InsertWebState(
      const web::NavigationManager::WebLoadParams& web_load_params,
      const TabInsertion::Params& tab_insertion_params);

  // Opens a new blank tab in response to DOM window opening action. Creates a
  // web state with empty navigation manager.
  web::WebState* InsertWebStateOpenedByDOM(web::WebState* parent);

 private:
  friend class BrowserUserData<TabInsertionBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit TabInsertionBrowserAgent(Browser* browser);

  raw_ptr<Browser> browser_;
};

#endif  // IOS_CHROME_BROWSER_TAB_INSERTION_MODEL_TAB_INSERTION_BROWSER_AGENT_H_
