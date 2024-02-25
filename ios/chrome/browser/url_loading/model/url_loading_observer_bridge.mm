// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/model/url_loading_observer_bridge.h"

UrlLoadingObserverBridge::UrlLoadingObserverBridge(
    id<URLLoadingObserving> owner)
    : owner_(owner) {}

UrlLoadingObserverBridge::~UrlLoadingObserverBridge() {
  CHECK(!IsInObserverList())
      << "UrlLoadingObserverBridge needs to be removed from "
         "observer list before their destruction.";
}

void UrlLoadingObserverBridge::TabWillLoadUrl(
    const GURL& url,
    ui::PageTransition transition_type) {
  if ([owner_ respondsToSelector:@selector(tabWillLoadURL:transitionType:)]) {
    [owner_ tabWillLoadURL:url transitionType:transition_type];
  }
}

void UrlLoadingObserverBridge::TabFailedToLoadUrl(
    const GURL& url,
    ui::PageTransition transition_type) {
  if ([owner_ respondsToSelector:@selector(tabFailedToLoadURL:
                                               transitionType:)]) {
    [owner_ tabFailedToLoadURL:url transitionType:transition_type];
  }
}

void UrlLoadingObserverBridge::TabDidPrerenderUrl(
    const GURL& url,
    ui::PageTransition transition_type) {
  if ([owner_ respondsToSelector:@selector(tabDidPrerenderURL:
                                               transitionType:)]) {
    [owner_ tabDidPrerenderURL:url transitionType:transition_type];
  }
}

void UrlLoadingObserverBridge::TabDidReloadUrl(
    const GURL& url,
    ui::PageTransition transition_type) {
  if ([owner_ respondsToSelector:@selector(tabDidReloadURL:transitionType:)]) {
    [owner_ tabDidReloadURL:url transitionType:transition_type];
  }
}

void UrlLoadingObserverBridge::TabDidLoadUrl(
    const GURL& url,
    ui::PageTransition transition_type) {
  if ([owner_ respondsToSelector:@selector(tabDidLoadURL:transitionType:)]) {
    [owner_ tabDidLoadURL:url transitionType:transition_type];
  }
}

void UrlLoadingObserverBridge::NewTabWillLoadUrl(const GURL& url,
                                                 bool user_initiated) {
  if ([owner_ respondsToSelector:@selector(newTabWillLoadURL:
                                             isUserInitiated:)]) {
    [owner_ newTabWillLoadURL:url isUserInitiated:user_initiated];
  }
}

void UrlLoadingObserverBridge::NewTabDidLoadUrl(const GURL& url,
                                                bool user_initiated) {
  if ([owner_ respondsToSelector:@selector(newTabDidLoadURL:
                                            isUserInitiated:)]) {
    [owner_ newTabDidLoadURL:url isUserInitiated:user_initiated];
  }
}

void UrlLoadingObserverBridge::WillSwitchToTabWithUrl(const GURL& url,
                                                      int new_web_state_index) {
  if ([owner_ respondsToSelector:@selector(willSwitchToTabWithURL:
                                                 newWebStateIndex:)]) {
    [owner_ willSwitchToTabWithURL:url newWebStateIndex:new_web_state_index];
  }
}

void UrlLoadingObserverBridge::DidSwitchToTabWithUrl(const GURL& url,
                                                     int new_web_state_index) {
  if ([owner_ respondsToSelector:@selector(didSwitchToTabWithURL:
                                                newWebStateIndex:)]) {
    [owner_ didSwitchToTabWithURL:url newWebStateIndex:new_web_state_index];
  }
}
