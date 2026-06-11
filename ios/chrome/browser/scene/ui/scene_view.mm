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

- (UIEdgeInsets)safeAreaInsets {
  UIEdgeInsets insets = [super safeAreaInsets];
  // If the device is in landscape on an iPhone (compact height), there are no
  // physical cutouts or status bars at the top of the screen. On iOS 26.0
  // specifically, the window's safeAreaInsets.top is incorrectly left at 20pt;
  // we manually override it to 0. This is skipped on iOS 26.1+ and iPad where
  // the layout engine correctly handles the inset.
  if (@available(iOS 26.0, *)) {
    if (!@available(iOS 26.1, *)) {
      if (insets.top > 0 && self.traitCollection.verticalSizeClass ==
                                UIUserInterfaceSizeClassCompact) {
        insets.top = 0;
      }
    }
  }
  return insets;
}

@end
