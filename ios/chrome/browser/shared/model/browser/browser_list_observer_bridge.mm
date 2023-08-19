// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/browser_list_observer_bridge.h"

BrowserListObserverBridge::BrowserListObserverBridge(
    id<BrowserListObserver> observer)
    : observer_(observer) {}

void BrowserListObserverBridge::OnBrowserAdded(const BrowserList* browser_list,
                                               Browser* browser) {
  if ([observer_ respondsToSelector:@selector(browserList:browserAdded:)]) {
    [observer_ browserList:browser_list browserAdded:browser];
  }
}
void BrowserListObserverBridge::OnIncognitoBrowserAdded(
    const BrowserList* browser_list,
    Browser* browser) {
  if ([observer_ respondsToSelector:@selector(browserList:
                                        incognitoBrowserAdded:)]) {
    [observer_ browserList:browser_list incognitoBrowserAdded:browser];
  }
}
void BrowserListObserverBridge::OnBrowserRemoved(
    const BrowserList* browser_list,
    Browser* browser) {
  if ([observer_ respondsToSelector:@selector(browserList:browserRemoved:)]) {
    [observer_ browserList:browser_list browserRemoved:browser];
  }
}
void BrowserListObserverBridge::OnIncognitoBrowserRemoved(
    const BrowserList* browser_list,
    Browser* browser) {
  if ([observer_ respondsToSelector:@selector(browserList:
                                        incognitoBrowserRemoved:)]) {
    [observer_ browserList:browser_list incognitoBrowserRemoved:browser];
  }
}
void BrowserListObserverBridge::OnBrowserListShutdown(
    BrowserList* browser_list) {
  if ([observer_ respondsToSelector:@selector(browserListWillShutdown:)]) {
    [observer_ browserListWillShutdown:browser_list];
  }
}
