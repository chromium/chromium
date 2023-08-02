// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main_content/test/test_main_content_ui_observer.h"

@implementation TestMainContentUIObserver
@synthesize broadcaster = _broadcaster;
@synthesize yOffset = _yOffset;
@synthesize scrolling = _scrolling;
@synthesize dragging = _dragging;

- (void)setBroadcaster:(ChromeBroadcaster*)broadcaster {
  [_broadcaster removeObserver:self
                   forSelector:@selector(broadcastContentScrollOffset:)];
  [_broadcaster removeObserver:self
                   forSelector:@selector(broadcastScrollViewIsScrolling:)];
  [_broadcaster removeObserver:self
                   forSelector:@selector(broadcastScrollViewIsDragging:)];
  _broadcaster = broadcaster;
  [_broadcaster addObserver:self
                forSelector:@selector(broadcastContentScrollOffset:)];
  [_broadcaster addObserver:self
                forSelector:@selector(broadcastScrollViewIsScrolling:)];
  [_broadcaster addObserver:self
                forSelector:@selector(broadcastScrollViewIsDragging:)];
}

- (void)broadcastContentScrollOffset:(CGFloat)offset {
  _yOffset = offset;
}

- (void)broadcastScrollViewIsScrolling:(BOOL)scrolling {
  _scrolling = scrolling;
}

- (void)broadcastScrollViewIsDragging:(BOOL)dragging {
  _dragging = dragging;
}

@end
