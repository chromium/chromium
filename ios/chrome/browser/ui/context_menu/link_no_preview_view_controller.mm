// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/context_menu/link_no_preview_view_controller.h"

#import "ios/chrome/browser/ui/context_menu/link_no_preview_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation LinkNoPreviewViewController

- (void)loadView {
  // TODO(crbug.com/1237933): Pass title and subtitle.
  self.view = [[LinkNoPreviewView alloc] initWithTitle:nil subtitle:nil];
}

- (void)viewDidLayoutSubviews {
  self.preferredContentSize = [self computePreferredContentSize];
}

- (CGSize)computePreferredContentSize {
  CGFloat width = self.view.bounds.size.width;
  CGSize minimalSize =
      [self.view systemLayoutSizeFittingSize:CGSizeMake(width, 0)];
  return CGSizeMake(width, minimalSize.height);
}

@end
