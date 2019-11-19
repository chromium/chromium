// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/url_loading_notifier.h"

#import "ios/chrome/browser/url_loading/url_loading_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UrlLoadingNotifier::UrlLoadingNotifier() {}
UrlLoadingNotifier::~UrlLoadingNotifier() {}

void UrlLoadingNotifier::AddObserver(UrlLoadingObserverBridge* observer) {
  observers_.AddObserver(observer);
}

void UrlLoadingNotifier::RemoveObserver(UrlLoadingObserverBridge* observer) {
  observers_.RemoveObserver(observer);
}

void UrlLoadingNotifier::TabWillLoadUrl(const GURL& url,
                                        ui::PageTransition transition_type) {
  for (auto& observer : observers_)
    observer.TabWillLoadUrl(url, transition_type);
}

void UrlLoadingNotifier::TabFailedToLoadUrl(
    const GURL& url,
    ui::PageTransition transition_type) {
  for (auto& observer : observers_)
    observer.TabFailedToLoadUrl(url, transition_type);
}

void UrlLoadingNotifier::TabDidPrerenderUrl(
    const GURL& url,
    ui::PageTransition transition_type) {
  for (auto& observer : observers_)
    observer.TabDidPrerenderUrl(url, transition_type);
}

void UrlLoadingNotifier::TabDidReloadUrl(const GURL& url,
                                         ui::PageTransition transition_type) {
  for (auto& observer : observers_)
    observer.TabDidReloadUrl(url, transition_type);
}

void UrlLoadingNotifier::TabDidLoadUrl(const GURL& url,
                                       ui::PageTransition transition_type) {
  for (auto& observer : observers_)
    observer.TabDidLoadUrl(url, transition_type);
}

void UrlLoadingNotifier::NewTabWillLoadUrl(const GURL& url,
                                           bool user_initiated) {
  for (auto& observer : observers_)
    observer.NewTabWillLoadUrl(url, user_initiated);
}

void UrlLoadingNotifier::NewTabDidLoadUrl(const GURL& url,
                                          bool user_initiated) {
  for (auto& observer : observers_)
    observer.NewTabDidLoadUrl(url, user_initiated);
}

void UrlLoadingNotifier::WillSwitchToTabWithUrl(const GURL& url,
                                                int new_web_state_index) {
  for (auto& observer : observers_)
    observer.WillSwitchToTabWithUrl(url, new_web_state_index);
}

void UrlLoadingNotifier::DidSwitchToTabWithUrl(const GURL& url,
                                               int new_web_state_index) {
  for (auto& observer : observers_)
    observer.DidSwitchToTabWithUrl(url, new_web_state_index);
}
