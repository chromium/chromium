// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper_observer_bridge.h"

ReaderModeTabHelperObserverBridge::ReaderModeTabHelperObserverBridge(
    id<ReaderModeTabHelperObserving> observer)
    : observer_(observer) {}

ReaderModeTabHelperObserverBridge::~ReaderModeTabHelperObserverBridge() {}

void ReaderModeTabHelperObserverBridge::ReaderModeWebStateDidLoadContent(
    ReaderModeTabHelper* tab_helper,
    web::WebState* web_state) {
  if ([observer_ respondsToSelector:@selector
                 (readerModeWebStateDidLoadContent:webState:)]) {
    [observer_ readerModeWebStateDidLoadContent:tab_helper webState:web_state];
  }
}

void ReaderModeTabHelperObserverBridge::ReaderModeWebStateWillBecomeUnavailable(
    ReaderModeTabHelper* tab_helper,
    web::WebState* web_state,
    ReaderModeDeactivationReason reason) {
  if ([observer_ respondsToSelector:@selector
                 (readerModeWebStateWillBecomeUnavailable:webState:reason:)]) {
    [observer_ readerModeWebStateWillBecomeUnavailable:tab_helper
                                              webState:web_state
                                                reason:reason];
  }
}

void ReaderModeTabHelperObserverBridge::ReaderModeDistillationFailed(
    ReaderModeTabHelper* tab_helper) {
  if ([observer_ respondsToSelector:@selector(readerModeDistillationFailed:)]) {
    [observer_ readerModeDistillationFailed:tab_helper];
  }
}

void ReaderModeTabHelperObserverBridge::ReaderModeTabHelperDestroyed(
    ReaderModeTabHelper* tab_helper,
    web::WebState* web_state) {
  if ([observer_ respondsToSelector:@selector(readerModeTabHelperDestroyed:
                                                                  webState:)]) {
    [observer_ readerModeTabHelperDestroyed:tab_helper webState:web_state];
  }
}
