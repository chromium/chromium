// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_PAGE_PLACEHOLDER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_WEB_PAGE_PLACEHOLDER_BROWSER_AGENT_H_

#include "base/memory/raw_ptr.h"
#include "ios/chrome/browser/sessions/session_restoration_observer.h"
#include "ios/chrome/browser/shared/model/browser/browser_observer.h"
#include "ios/chrome/browser/shared/model/browser/browser_user_data.h"

// Browser agent used to add or cancel a page placeholder for next navigation.
class PagePlaceholderBrowserAgent final
    : public BrowserObserver,
      public BrowserUserData<PagePlaceholderBrowserAgent>,
      public SessionRestorationObserver {
 public:
  ~PagePlaceholderBrowserAgent() final;

  // Used to inform that a new foreground tab is about to be opened.
  void ExpectNewForegroundTab();

  // Adds a page placeholder.
  void AddPagePlaceholder();

  // Cancels the page placeholder.
  void CancelPagePlaceholder();

  // BrowserObserver implementation.
  void BrowserDestroyed(Browser* browser) final;

  // SessionRestorationObserver implementation.
  void WillStartSessionRestoration(Browser* browser) final;
  void SessionRestorationFinished(
      Browser* browser,
      const std::vector<web::WebState*>& restored_web_states) final;

 private:
  friend class BrowserUserData<PagePlaceholderBrowserAgent>;

  explicit PagePlaceholderBrowserAgent(Browser* browser);

  // The Browser this object is attached to.
  raw_ptr<Browser> browser_ = nullptr;

  // True if waiting for a foreground tab due to expectNewForegroundTab.
  bool expecting_foreground_tab_ = false;

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_WEB_PAGE_PLACEHOLDER_BROWSER_AGENT_H_
