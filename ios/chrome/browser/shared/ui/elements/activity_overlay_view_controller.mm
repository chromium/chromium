// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/activity_overlay_view_controller.h"

#import "ios/chrome/browser/shared/ui/elements/activity_overlay_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ActivityOverlayViewController

- (void)loadView {
  self.view = [[ActivityOverlayView alloc] init];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
}

@end
