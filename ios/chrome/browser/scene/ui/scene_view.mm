// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/ui/scene_view.h"

#import "ios/chrome/browser/scene/ui/scene_view_delegate.h"

@implementation SceneView

- (void)didMoveToWindow {
  [super didMoveToWindow];
  [self.delegate sceneViewDidMoveToWindow:self];
}

@end
