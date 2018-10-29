// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/fullscreen/toolbar_ui_broadcasting_util.h"

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"
#import "ios/chrome/browser/ui/toolbar/fullscreen/toolbar_ui.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void StartBroadcastingToolbarUI(id<ToolbarUI> toolbar,
                                ChromeBroadcaster* broadcaster) {
  [broadcaster broadcastValue:@"collapsedHeight"
                     ofObject:toolbar
                     selector:@selector(broadcastCollapsedToolbarHeight:)];
  [broadcaster broadcastValue:@"expandedHeight"
                     ofObject:toolbar
                     selector:@selector(broadcastExpandedToolbarHeight:)];
  [broadcaster broadcastValue:@"bottomToolbarHeight"
                     ofObject:toolbar
                     selector:@selector(broadcastBottomToolbarHeight:)];
}

void StopBroadcastingToolbarUI(ChromeBroadcaster* broadcaster) {
  [broadcaster
      stopBroadcastingForSelector:@selector(broadcastCollapsedToolbarHeight:)];
  [broadcaster
      stopBroadcastingForSelector:@selector(broadcastExpandedToolbarHeight:)];
  [broadcaster
      stopBroadcastingForSelector:@selector(broadcastBottomToolbarHeight:)];
}
