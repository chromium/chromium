// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_PAGE_PLACEHOLDER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_WEB_PAGE_PLACEHOLDER_BROWSER_AGENT_H_

#import <string>

#include "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

// Browser agent used to add or cancel a page placeholder for next navigation.
class PagePlaceholderBrowserAgent
    : public BrowserUserData<PagePlaceholderBrowserAgent> {
 public:
  ~PagePlaceholderBrowserAgent() override;

  // Not copyable or assignable.
  PagePlaceholderBrowserAgent(const PagePlaceholderBrowserAgent&) = delete;
  PagePlaceholderBrowserAgent& operator=(const PagePlaceholderBrowserAgent&) =
      delete;

  // Used to inform that a new foreground tab is about to be opened.
  void ExpectNewForegroundTab();

  // Adds a page placeholder.
  void AddPagePlaceholder();

  // Calcels the page placeholder.
  void CancelPagePlaceholder();

 private:
  friend class BrowserUserData<PagePlaceholderBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit PagePlaceholderBrowserAgent(Browser* browser);

  WebStateList* web_state_list_ = nullptr;

  // True if waiting for a foreground tab due to expectNewForegroundTab.
  bool expecting_foreground_tab_;
};

#endif  // IOS_CHROME_BROWSER_WEB_PAGE_PLACEHOLDER_BROWSER_AGENT_H_
