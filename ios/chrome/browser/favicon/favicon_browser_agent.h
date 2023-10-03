// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_FAVICON_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_FAVICON_FAVICON_BROWSER_AGENT_H_

#include "base/memory/raw_ptr.h"
#include "ios/chrome/browser/sessions/session_restoration_observer.h"
#include "ios/chrome/browser/shared/model/browser/browser_observer.h"
#include "ios/chrome/browser/shared/model/browser/browser_user_data.h"

// A BrowserAgent that prepares WebStates to fetch their favicon after
// a session restoration.
class FaviconBrowserAgent final : public BrowserObserver,
                                  public BrowserUserData<FaviconBrowserAgent>,
                                  public SessionRestorationObserver {
 public:
  ~FaviconBrowserAgent() final;

  // BrowserObserver implementation.
  void BrowserDestroyed(Browser* browser) final;

  // SessionRestorationObserver implementation.
  void WillStartSessionRestoration(Browser* browser) final;
  void SessionRestorationFinished(
      Browser* browser,
      const std::vector<web::WebState*>& restored_web_states) final;

 private:
  friend class BrowserUserData<FaviconBrowserAgent>;

  explicit FaviconBrowserAgent(Browser* browser);

  // The Browser this object is attached to.
  raw_ptr<Browser> browser_;

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_FAVICON_FAVICON_BROWSER_AGENT_H_
