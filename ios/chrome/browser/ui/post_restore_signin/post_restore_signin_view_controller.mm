// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/post_restore_signin/post_restore_signin_view_controller.h"

#import <Foundation/Foundation.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PostRestoreSignInViewController

#pragma mark - Public

- (void)loadView {
  self.imageHasFixedSize = YES;
  self.customSpacingAfterImage = 30;
  self.showDismissBarButton = NO;
  self.dismissBarButtonSystemItem = UIBarButtonSystemItemDone;

  if (@available(iOS 15, *)) {
    self.titleTextStyle = UIFontTextStyleTitle2;
    self.topAlignedLayout = YES;
  }

  [super loadView];
}

@end
