// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/ui/parent_access_bottom_sheet_view_controller.h"

#import "base/functional/callback.h"
#import "ios/chrome/browser/supervised_user/ui/parent_access_view_controller_delegate.h"

@implementation ParentAccessBottomSheetViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.topAlignedLayout = YES;
  self.scrollEnabled = NO;

  [super viewDidLoad];

  __weak __typeof(self) weakSelf = self;
  [self.delegate
      handleParentAccessRequest:base::BindOnce(^(NSURL* URL, NSError* error) {
        [weakSelf displayParentAccessWidgetWithURL:URL error:error];
      })];
}

#pragma mark - Private

// Embeds the parent access widget in a bottom sheet displayed to the user.
- (void)displayParentAccessWidgetWithURL:(NSURL*)URL error:(NSError*)error {
  // TODO(crbug.com/384517702): Embed the parent access widget in the
  // bottomsheet using the valid URL.
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  presentationController.detents =
      @[ [UISheetPresentationControllerDetent mediumDetent] ];
  presentationController.selectedDetentIdentifier =
      UISheetPresentationControllerDetentIdentifierMedium;
}

@end
