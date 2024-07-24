// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/ui_bundled/tab_strip_container_view.h"

#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/tabs/ui_bundled/tab_strip_view.h"

@implementation TabStripContainerView

- (UIView*)screenshotForAnimation {
  // The tab strip snapshot should have a clear background color. When using
  // smooth scrolling, the background color is black, because the web content
  // extends behind the tab strip. Switch out the background color for the
  // snapshot and restore it afterwards.
  UIColor* backgroundColor = self.tabStripView.backgroundColor;
  self.tabStripView.backgroundColor = UIColor.clearColor;
  UIView* tabStripSnapshot =
      [self.tabStripView snapshotViewAfterScreenUpdates:YES];
  self.tabStripView.backgroundColor = backgroundColor;
  tabStripSnapshot.transform =
      [self adjustTransformForRTL:tabStripSnapshot.transform];
  return tabStripSnapshot;
}

- (CGAffineTransform)adjustTransformForRTL:(CGAffineTransform)transform {
  if (!UseRTLLayout()) {
    return transform;
  }
  return CGAffineTransformConcat(transform, CGAffineTransformMakeScale(-1, 1));
}

@end
