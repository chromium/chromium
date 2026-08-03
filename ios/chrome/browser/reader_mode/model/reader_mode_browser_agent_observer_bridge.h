// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_BROWSER_AGENT_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_BROWSER_AGENT_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/reader_mode/model/reader_mode_browser_agent.h"

// Objective-C protocol mirroring ReaderModeBrowserAgent::Observer.
@protocol ReaderModeBrowserAgentObserving <NSObject>
@optional
- (void)readerModeBrowserAgent:(ReaderModeBrowserAgent*)agent
            didShowModeContent:(BOOL)animated;
- (void)readerModeBrowserAgent:(ReaderModeBrowserAgent*)agent
            didHideModeContent:(BOOL)animated;
- (void)readerModeBrowserAgentDestroyed:(ReaderModeBrowserAgent*)agent;
@end

// Observer bridge to forward C++ ReaderModeBrowserAgent events to Objective-C
// observer.
class ReaderModeBrowserAgentObserverBridge final
    : public ReaderModeBrowserAgent::Observer {
 public:
  explicit ReaderModeBrowserAgentObserverBridge(
      id<ReaderModeBrowserAgentObserving> observer);

  ReaderModeBrowserAgentObserverBridge(
      const ReaderModeBrowserAgentObserverBridge&) = delete;
  ReaderModeBrowserAgentObserverBridge& operator=(
      const ReaderModeBrowserAgentObserverBridge&) = delete;

  ~ReaderModeBrowserAgentObserverBridge() final;

  // ReaderModeBrowserAgent::Observer implementation.
  void OnReaderModeContentShown(ReaderModeBrowserAgent* agent) final;
  void OnReaderModeContentHidden(ReaderModeBrowserAgent* agent) final;
  void ReaderModeBrowserAgentDestroyed(ReaderModeBrowserAgent* agent) final;

 private:
  __weak id<ReaderModeBrowserAgentObserving> observer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_BROWSER_AGENT_OBSERVER_BRIDGE_H_
