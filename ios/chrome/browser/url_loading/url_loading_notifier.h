// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_NOTIFIER_H_
#define IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_NOTIFIER_H_

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

class UrlLoadingObserverBridge;

// A class containing static functions to notify observers of url loading
// state change.
class UrlLoadingNotifier : public KeyedService {
 public:
  // Creates a UrlLoadingNotifier.
  explicit UrlLoadingNotifier();
  ~UrlLoadingNotifier() override;

  // Adds |observer| to the list of observers.
  void AddObserver(UrlLoadingObserverBridge* observer);

  // Removes |observer| from the list of observers.
  void RemoveObserver(UrlLoadingObserverBridge* observer);

  // The loader will load |url| in the current tab. Next state will be
  // one of: TabFailedToLoadUrl, TabDidPrerenderUrl,
  // TabDidReloadUrl or TabDidLoadUrl.
  void TabWillLoadUrl(const GURL& url, ui::PageTransition transition_type);

  // The loader didn't succeed loading the requested |url|. Reason
  // can, for example be an incognito mismatch or an induced crash.
  // It is possible that the url was loaded, but in another tab.
  void TabFailedToLoadUrl(const GURL& url, ui::PageTransition transition_type);

  // The loader replaced the load with a prerendering.
  void TabDidPrerenderUrl(const GURL& url, ui::PageTransition transition_type);

  // The loader reloaded the |url| in the current tab.
  void TabDidReloadUrl(const GURL& url, ui::PageTransition transition_type);

  // The loader initiated the |url| loading successfully.
  void TabDidLoadUrl(const GURL& url, ui::PageTransition transition_type);

  // The loader will load |url| in a new tab. |user_initiated| is true of the
  // request is explicitly user initiated, and false otherwise (like the
  // opening on an NTP on startup or requesting the help page). Next state will
  // be NewTabDidLoadUrl.
  void NewTabWillLoadUrl(const GURL& url, bool user_initiated);

  // The loader initiated the |url| loading in a new tab successfully.
  void NewTabDidLoadUrl(const GURL& url, bool user_initiated);

  // The loader will switch to an existing tab with |URL| instead of loading it.
  // Next state will be: DidSwitchToTabWithUrl.
  void WillSwitchToTabWithUrl(const GURL& url, int new_web_state_index);

  // The loader switched to an existing tab with |URL|.
  void DidSwitchToTabWithUrl(const GURL& url, int new_web_state_index);

 private:
  base::ObserverList<UrlLoadingObserverBridge>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(UrlLoadingNotifier);
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_NOTIFIER_H_
