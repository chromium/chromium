// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main_content/main_content_ui_broadcasting_util.h"

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"
#import "ios/chrome/browser/ui/main_content/main_content_ui.h"
#import "ios/chrome/browser/ui/main_content/main_content_ui_state.h"

void StartBroadcastingMainContentUI(id<MainContentUI> main_content,
                                    ChromeBroadcaster* broadcaster) {
  [broadcaster broadcastValue:@"scrollViewSize"
                     ofObject:main_content.mainContentUIState
                     selector:@selector(broadcastScrollViewSize:)];
  [broadcaster broadcastValue:@"contentSize"
                     ofObject:main_content.mainContentUIState
                     selector:@selector(broadcastScrollViewContentSize:)];
  [broadcaster broadcastValue:@"contentInset"
                     ofObject:main_content.mainContentUIState
                     selector:@selector(broadcastScrollViewContentInset:)];
  [broadcaster broadcastValue:@"yContentOffset"
                     ofObject:main_content.mainContentUIState
                     selector:@selector(broadcastContentScrollOffset:)];
  [broadcaster broadcastValue:@"scrolling"
                     ofObject:main_content.mainContentUIState
                     selector:@selector(broadcastScrollViewIsScrolling:)];
  [broadcaster broadcastValue:@"zooming"
                     ofObject:main_content.mainContentUIState
                     selector:@selector(broadcastScrollViewIsZooming:)];
  [broadcaster broadcastValue:@"dragging"
                     ofObject:main_content.mainContentUIState
                     selector:@selector(broadcastScrollViewIsDragging:)];
}

void StopBroadcastingMainContentUI(ChromeBroadcaster* broadcaster) {
  [broadcaster stopBroadcastingForSelector:@selector(broadcastScrollViewSize:)];
  [broadcaster
      stopBroadcastingForSelector:@selector(broadcastScrollViewContentSize:)];
  [broadcaster
      stopBroadcastingForSelector:@selector(broadcastScrollViewContentInset:)];
  [broadcaster
      stopBroadcastingForSelector:@selector(broadcastContentScrollOffset:)];
  [broadcaster
      stopBroadcastingForSelector:@selector(broadcastScrollViewIsScrolling:)];
  [broadcaster
      stopBroadcastingForSelector:@selector(broadcastScrollViewIsZooming:)];
  [broadcaster
      stopBroadcastingForSelector:@selector(broadcastScrollViewIsDragging:)];
}
