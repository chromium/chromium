// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/test/test_toolbar_ui_observer.h"

@implementation TestToolbarUIObserver
@synthesize broadcaster = _broadcaster;
@synthesize collapsedTopToolbarHeight = _collapsedTopToolbarHeight;
@synthesize expandedTopToolbarHeight = _expandedTopToolbarHeight;
@synthesize expandedBottomToolbarHeight = _expandedBottomToolbarHeight;
@synthesize collapsedBottomToolbarHeight = _collapsedBottomToolbarHeight;

- (void)setBroadcaster:(ChromeBroadcaster*)broadcaster {
  [_broadcaster removeObserver:self
                   forSelector:@selector(broadcastCollapsedTopToolbarHeight:)];
  [_broadcaster removeObserver:self
                   forSelector:@selector(broadcastExpandedTopToolbarHeight:)];
  [_broadcaster
      removeObserver:self
         forSelector:@selector(broadcastExpandedBottomToolbarHeight:)];
  [_broadcaster
      removeObserver:self
         forSelector:@selector(broadcastCollapsedBottomToolbarHeight:)];
  _broadcaster = broadcaster;
  [_broadcaster addObserver:self
                forSelector:@selector(broadcastCollapsedTopToolbarHeight:)];
  [_broadcaster addObserver:self
                forSelector:@selector(broadcastExpandedTopToolbarHeight:)];
  [_broadcaster addObserver:self
                forSelector:@selector(broadcastExpandedBottomToolbarHeight:)];
  [_broadcaster addObserver:self
                forSelector:@selector(broadcastCollapsedBottomToolbarHeight:)];
}

- (void)broadcastCollapsedTopToolbarHeight:(CGFloat)toolbarHeight {
  _collapsedTopToolbarHeight = toolbarHeight;
}

- (void)broadcastExpandedTopToolbarHeight:(CGFloat)toolbarHeight {
  _expandedTopToolbarHeight = toolbarHeight;
}

- (void)broadcastCollapsedBottomToolbarHeight:(CGFloat)toolbarHeight {
  _collapsedBottomToolbarHeight = toolbarHeight;
}

- (void)broadcastExpandedBottomToolbarHeight:(CGFloat)toolbarHeight {
  _expandedBottomToolbarHeight = toolbarHeight;
}

@end
