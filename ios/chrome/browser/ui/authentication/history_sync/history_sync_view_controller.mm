// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_view_controller.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation HistorySyncViewController

@dynamic delegate;

- (void)viewDidLoad {
  self.view.accessibilityIdentifier = kHistorySyncViewAccessibilityIdentifier;
  self.shouldHideBanner = YES;
  self.hasAvatarImage = YES;
  // TODO(crbug.com/1442218): Replace these temporary strings with the
  // definitive ones.
  self.titleText = @"** Save Time, Type Less **";
  self.subtitleText = @"** To quickly get back to sites you've visited, sync "
                      @"your tabs and history **";
  self.primaryActionString = @"** Yes, I'm in **";
  self.secondaryActionString = @"** No, thanks **";
  [super viewDidLoad];
}

#pragma mark - HistorySyncConsumer

- (void)setPrimaryIdentityAvatarImage:(UIImage*)primaryIdentityAvatarImage {
  self.avatarImage = primaryIdentityAvatarImage;
}

- (void)setPrimaryIdentityAvatarAccessibilityLabel:
    (NSString*)primaryIdentityAvatarAccessibilityLabel {
  self.avatarAccessibilityLabel = primaryIdentityAvatarAccessibilityLabel;
}

@end
