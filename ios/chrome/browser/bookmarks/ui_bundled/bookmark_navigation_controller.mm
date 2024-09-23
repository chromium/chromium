// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_navigation_controller.h"

#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_ui_constants.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_utils_ios.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation BookmarkNavigationController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.navigationBar.accessibilityIdentifier = kBookmarkNavigationBarIdentifier;
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
