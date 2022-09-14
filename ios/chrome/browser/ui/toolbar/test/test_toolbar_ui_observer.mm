// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/test/test_toolbar_ui_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TestToolbarUIObserver
@synthesize broadcaster = _broadcaster;
@synthesize collapsedHeight = _collapsedHeight;
@synthesize expandedHeight = _expandedHeight;
@synthesize bottomToolbarHeight = _bottomToolbarHeight;

- (void)setBroadcaster:(ChromeBroadcaster*)broadcaster {
  [_broadcaster removeObserver:self
                   forSelector:@selector(broadcastCollapsedToolbarHeight:)];
  [_broadcaster removeObserver:self
                   forSelector:@selector(broadcastExpandedToolbarHeight:)];
  [_broadcaster removeObserver:self
                   forSelector:@selector(broadcastBottomToolbarHeight:)];
  _broadcaster = broadcaster;
  [_broadcaster addObserver:self
                forSelector:@selector(broadcastCollapsedToolbarHeight:)];
  [_broadcaster addObserver:self
                forSelector:@selector(broadcastExpandedToolbarHeight:)];
  [_broadcaster addObserver:self
                forSelector:@selector(broadcastBottomToolbarHeight:)];
}

- (void)broadcastCollapsedToolbarHeight:(CGFloat)toolbarHeight {
  _collapsedHeight = toolbarHeight;
}

- (void)broadcastExpandedToolbarHeight:(CGFloat)toolbarHeight {
  _expandedHeight = toolbarHeight;
}

- (void)broadcastBottomToolbarHeight:(CGFloat)toolbarHeight {
  _bottomToolbarHeight = toolbarHeight;
}

@end
