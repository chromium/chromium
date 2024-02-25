// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_LOADING_OBSERVER_H_
#define IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_LOADING_OBSERVER_H_

#import "base/observer_list_types.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

// Observer used to update listeners of change of state in url loading.
class UrlLoadingObserver : public base::CheckedObserver {
 public:
  UrlLoadingObserver(const UrlLoadingObserver&) = delete;
  UrlLoadingObserver& operator=(const UrlLoadingObserver&) = delete;

  ~UrlLoadingObserver() override;

  // The loader will load `URL` in the current tab. Next state will be
  // one of: tabFailedToLoadURL, tabDidPrerenderURL,
  // tabDidReloadURL or tabDidLoadURL.
  virtual void TabWillLoadUrl(const GURL& url,
                              ui::PageTransition transition_type) {}

  // The loader didn't succeed loading the requested `URL`. Reason
  // can, for example be an incognito mismatch or an induced crash.
  // It is possible that the url was loaded, but in another tab.
  virtual void TabFailedToLoadUrl(const GURL& url,
                                  ui::PageTransition transition_type) {}

  // The loader replaced the load with a prerendering.
  virtual void TabDidPrerenderUrl(const GURL& url,
                                  ui::PageTransition transition_type) {}

  // The loader reloaded the `URL` in the current tab.
  virtual void TabDidReloadUrl(const GURL& url,
                               ui::PageTransition transition_type) {}

  // The loader initiated the `url` loading successfully.
  virtual void TabDidLoadUrl(const GURL& url,
                             ui::PageTransition transition_type) {}

  // The loader will load `URL` in a new tab. Next state will be:
  // newTabDidLoadURL.
  virtual void NewTabWillLoadUrl(const GURL& url, bool user_initiated) {}

  // The loader initiated the `URL` loading in a new tab successfully.
  virtual void NewTabDidLoadUrl(const GURL& url, bool user_initiated) {}

  // The loader will switch to an existing tab with `URL` instead of loading it.
  // Next state will be: didSwitchToTabWithURL.
  virtual void WillSwitchToTabWithUrl(const GURL& url,
                                      int new_web_state_index) {}

  // The loader switched to an existing tab with `URL`.
  virtual void DidSwitchToTabWithUrl(const GURL& url, int new_web_state_index) {
  }

 protected:
  UrlLoadingObserver();
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_LOADING_OBSERVER_H_
