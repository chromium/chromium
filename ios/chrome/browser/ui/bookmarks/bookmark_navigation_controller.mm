// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller.h"

#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BookmarkNavigationController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = UIColor.cr_systemBackgroundColor;
}

- (BOOL)disablesAutomaticKeyboardDismissal {
  // This allows us to hide the keyboard when controllers are being displayed in
  // a modal form sheet on the iPad.
  return NO;
}

#pragma mark - UIResponder

- (BOOL)canBecomeFirstResponder {
  return YES;
}

@end
