// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"

#import "ios/chrome/browser/url_loading/model/url_loading_observer.h"

BROWSER_USER_DATA_KEY_IMPL(UrlLoadingNotifierBrowserAgent)

UrlLoadingNotifierBrowserAgent::UrlLoadingNotifierBrowserAgent(
    Browser* browser) {}

UrlLoadingNotifierBrowserAgent::~UrlLoadingNotifierBrowserAgent() {}

void UrlLoadingNotifierBrowserAgent::AddObserver(UrlLoadingObserver* observer) {
  observers_.AddObserver(observer);
}

void UrlLoadingNotifierBrowserAgent::RemoveObserver(
    UrlLoadingObserver* observer) {
  observers_.RemoveObserver(observer);
}

void UrlLoadingNotifierBrowserAgent::TabWillLoadUrl(
    const GURL& url,
    ui::PageTransition transition_type) {
  for (auto& observer : observers_) {
    observer.TabWillLoadUrl(url, transition_type);
  }
}

void UrlLoadingNotifierBrowserAgent::TabFailedToLoadUrl(
    const GURL& url,
    ui::PageTransition transition_type) {
  for (auto& observer : observers_) {
    observer.TabFailedToLoadUrl(url, transition_type);
  }
}

void UrlLoadingNotifierBrowserAgent::TabDidPrerenderUrl(
    const GURL& url,
    ui::PageTransition transition_type) {
  for (auto& observer : observers_) {
    observer.TabDidPrerenderUrl(url, transition_type);
  }
}

void UrlLoadingNotifierBrowserAgent::TabDidReloadUrl(
    const GURL& url,
    ui::PageTransition transition_type) {
  for (auto& observer : observers_) {
    observer.TabDidReloadUrl(url, transition_type);
  }
}

void UrlLoadingNotifierBrowserAgent::TabDidLoadUrl(
    const GURL& url,
    ui::PageTransition transition_type) {
  for (auto& observer : observers_) {
    observer.TabDidLoadUrl(url, transition_type);
  }
}

void UrlLoadingNotifierBrowserAgent::NewTabWillLoadUrl(const GURL& url,
                                                       bool user_initiated) {
  for (auto& observer : observers_) {
    observer.NewTabWillLoadUrl(url, user_initiated);
  }
}

void UrlLoadingNotifierBrowserAgent::NewTabDidLoadUrl(const GURL& url,
                                                      bool user_initiated) {
  for (auto& observer : observers_) {
    observer.NewTabDidLoadUrl(url, user_initiated);
  }
}

void UrlLoadingNotifierBrowserAgent::WillSwitchToTabWithUrl(
    const GURL& url,
    int new_web_state_index) {
  for (auto& observer : observers_) {
    observer.WillSwitchToTabWithUrl(url, new_web_state_index);
  }
}

void UrlLoadingNotifierBrowserAgent::DidSwitchToTabWithUrl(
    const GURL& url,
    int new_web_state_index) {
  for (auto& observer : observers_) {
    observer.DidSwitchToTabWithUrl(url, new_web_state_index);
  }
}
