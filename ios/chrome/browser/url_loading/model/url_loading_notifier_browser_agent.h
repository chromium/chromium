// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_LOADING_NOTIFIER_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_LOADING_NOTIFIER_BROWSER_AGENT_H_

#import "base/observer_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

class UrlLoadingObserver;

// A class containing static functions to notify observers of url loading
// state change.
class UrlLoadingNotifierBrowserAgent
    : public BrowserUserData<UrlLoadingNotifierBrowserAgent> {
 public:
  // Not copyable or moveable
  UrlLoadingNotifierBrowserAgent(const UrlLoadingNotifierBrowserAgent&) =
      delete;
  UrlLoadingNotifierBrowserAgent& operator=(
      const UrlLoadingNotifierBrowserAgent&) = delete;
  ~UrlLoadingNotifierBrowserAgent() override;

  // Adds `observer` to the list of observers.
  void AddObserver(UrlLoadingObserver* observer);

  // Removes `observer` from the list of observers.
  void RemoveObserver(UrlLoadingObserver* observer);

  // The loader will load `url` in the current tab. Next state will be
  // one of: TabFailedToLoadUrl, TabDidPrerenderUrl,
  // TabDidReloadUrl or TabDidLoadUrl.
  void TabWillLoadUrl(const GURL& url, ui::PageTransition transition_type);

  // The loader didn't succeed loading the requested `url`. Reason
  // can, for example be an incognito mismatch or an induced crash.
  // It is possible that the url was loaded, but in another tab.
  void TabFailedToLoadUrl(const GURL& url, ui::PageTransition transition_type);

  // The loader replaced the load with a prerendering.
  void TabDidPrerenderUrl(const GURL& url, ui::PageTransition transition_type);

  // The loader reloaded the `url` in the current tab.
  void TabDidReloadUrl(const GURL& url, ui::PageTransition transition_type);

  // The loader initiated the `url` loading successfully.
  void TabDidLoadUrl(const GURL& url, ui::PageTransition transition_type);

  // The loader will load `url` in a new tab. `user_initiated` is true of the
  // request is explicitly user initiated, and false otherwise (like the
  // opening on an NTP on startup or requesting the help page). Next state will
  // be NewTabDidLoadUrl.
  void NewTabWillLoadUrl(const GURL& url, bool user_initiated);

  // The loader initiated the `url` loading in a new tab successfully.
  void NewTabDidLoadUrl(const GURL& url, bool user_initiated);

  // The loader will switch to an existing tab with `url` instead of loading it.
  // Next state will be: DidSwitchToTabWithUrl.
  void WillSwitchToTabWithUrl(const GURL& url, int new_web_state_index);

  // The loader switched to an existing tab with `url`.
  void DidSwitchToTabWithUrl(const GURL& url, int new_web_state_index);

 private:
  friend class BrowserUserData<UrlLoadingNotifierBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit UrlLoadingNotifierBrowserAgent(Browser* browser);

  base::ObserverList<UrlLoadingObserver, true> observers_;
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_LOADING_NOTIFIER_BROWSER_AGENT_H_
