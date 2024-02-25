// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main_content/test/test_main_content_ui_observer.h"

@implementation TestMainContentUIObserver
@synthesize broadcaster = _broadcaster;
@synthesize yOffset = _yOffset;

- (void)setBroadcaster:(ChromeBroadcaster*)broadcaster {
  [_broadcaster removeObserver:self
                   forSelector:@selector(broadcastContentScrollOffset:)];
  _broadcaster = broadcaster;
  [_broadcaster addObserver:self
                forSelector:@selector(broadcastContentScrollOffset:)];
}

- (void)broadcastContentScrollOffset:(CGFloat)offset {
  _yOffset = offset;
}

@end
