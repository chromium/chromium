// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size_broadcasting_util.h"

#import "ios/chrome/browser/broadcaster/ui_bundled/chrome_broadcaster.h"
#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size.h"

void StartBroadcastingToolbarsSize(ToolbarsSize* toolbar,
                                   ChromeBroadcaster* broadcaster) {
  [broadcaster broadcastValue:@"collapsedTopToolbarHeight"
                     ofObject:toolbar
                     selector:@selector(broadcastCollapsedTopToolbarHeight:)];
  [broadcaster broadcastValue:@"expandedTopToolbarHeight"
                     ofObject:toolbar
                     selector:@selector(broadcastExpandedTopToolbarHeight:)];
  [broadcaster
      broadcastValue:@"collapsedBottomToolbarHeight"
            ofObject:toolbar
            selector:@selector(broadcastCollapsedBottomToolbarHeight:)];
  [broadcaster broadcastValue:@"expandedBottomToolbarHeight"
                     ofObject:toolbar
                     selector:@selector(broadcastExpandedBottomToolbarHeight:)];
}

void StopBroadcastingToolbarsSize(ChromeBroadcaster* broadcaster) {
  [broadcaster stopBroadcastingForSelector:@selector
               (broadcastCollapsedTopToolbarHeight:)];
  [broadcaster stopBroadcastingForSelector:@selector
               (broadcastExpandedTopToolbarHeight:)];
  [broadcaster stopBroadcastingForSelector:@selector
               (broadcastCollapsedBottomToolbarHeight:)];
  [broadcaster stopBroadcastingForSelector:@selector
               (broadcastExpandedBottomToolbarHeight:)];
}
