// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_browser_agent_observer_bridge.h"

ReaderModeBrowserAgentObserverBridge::ReaderModeBrowserAgentObserverBridge(
    id<ReaderModeBrowserAgentObserving> observer)
    : observer_(observer) {}

ReaderModeBrowserAgentObserverBridge::~ReaderModeBrowserAgentObserverBridge() =
    default;

void ReaderModeBrowserAgentObserverBridge::OnReaderModeContentShown(
    ReaderModeBrowserAgent* agent) {
  if ([observer_
          respondsToSelector:@selector(
                                 readerModeBrowserAgent:didShowModeContent:)]) {
    [observer_ readerModeBrowserAgent:agent didShowModeContent:YES];
  }
}

void ReaderModeBrowserAgentObserverBridge::OnReaderModeContentHidden(
    ReaderModeBrowserAgent* agent) {
  if ([observer_
          respondsToSelector:@selector(
                                 readerModeBrowserAgent:didHideModeContent:)]) {
    [observer_ readerModeBrowserAgent:agent didHideModeContent:YES];
  }
}

void ReaderModeBrowserAgentObserverBridge::ReaderModeBrowserAgentDestroyed(
    ReaderModeBrowserAgent* agent) {
  if ([observer_
          respondsToSelector:@selector(readerModeBrowserAgentDestroyed:)]) {
    [observer_ readerModeBrowserAgentDestroyed:agent];
  }
}
