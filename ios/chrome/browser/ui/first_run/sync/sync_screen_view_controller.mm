// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/sync/sync_screen_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SyncScreenViewController
@dynamic delegate;

- (void)viewDidLoad {
  // TODO(crbug.com/1189840): set strings and images to the view.
  self.titleText = @"Test Sync Screen";
  self.primaryActionString = @"Test Continue Button";
  [super viewDidLoad];
}

@end
