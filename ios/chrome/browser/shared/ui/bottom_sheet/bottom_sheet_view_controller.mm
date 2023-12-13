// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/bottom_sheet/bottom_sheet_view_controller.h"

#import <Foundation/Foundation.h>

namespace {

// Custom radius for the half sheet presentation.
CGFloat const kHalfSheetCornerRadius = 20;

// Custom height for the gradient view of the bottom sheet.
CGFloat const kCustomGradientViewHeight = 30;

// Custom detent identifier for when the bottom sheet is minimized.
NSString* const kCustomMinimizedDetentIdentifier = @"customMinimizedDetent";

// Custom detent identifier for when the bottom sheet is expanded.
NSString* const kCustomExpandedDetentIdentifier = @"customExpandedDetent";

}  // namespace

@implementation BottomSheetViewController {
}

- (void)viewDidLoad {
  self.alwaysShowImage = YES;
  self.customGradientViewHeight = kCustomGradientViewHeight;
  [super viewDidLoad];
  [self displayGradientView:NO];
  [self setUpBottomSheetPresentationController];
  [self setUpBottomSheetDetents];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  // Update the custom detent with the correct initial height when trait
  // collection changed (for example when the user uses large font).
  if (self.traitCollection.preferredContentSizeCategory !=
      previousTraitCollection.preferredContentSizeCategory) {
    UISheetPresentationController* presentationController =
        self.sheetPresentationController;
    if (@available(iOS 16, *)) {
      CGFloat bottomSheetHeight = [self preferredHeightForContent];
      auto resolver = ^CGFloat(
          id<UISheetPresentationControllerDetentResolutionContext> context) {
        return bottomSheetHeight;
      };

      UISheetPresentationControllerDetent* customDetent =
          [UISheetPresentationControllerDetent
              customDetentWithIdentifier:kCustomMinimizedDetentIdentifier
                                resolver:resolver];
      presentationController.detents = @[ customDetent ];
      presentationController.selectedDetentIdentifier =
          kCustomMinimizedDetentIdentifier;
    }
  }
}

- (void)expandBottomSheet {
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  if (@available(iOS 16, *)) {
    // Expand to custom size (only available for iOS 16+).
    CGFloat fullHeight = [self preferredHeightForContent];
    auto resolver = ^CGFloat(
        id<UISheetPresentationControllerDetentResolutionContext> context) {
      BOOL tooLarge = (fullHeight > context.maximumDetentValue);
      [self displayGradientView:tooLarge];
      return tooLarge ? context.maximumDetentValue : fullHeight;
    };
    UISheetPresentationControllerDetent* customDetentExpand =
        [UISheetPresentationControllerDetent
            customDetentWithIdentifier:kCustomExpandedDetentIdentifier
                              resolver:resolver];
    NSMutableArray* currentDetents =
        [presentationController.detents mutableCopy];
    [currentDetents addObject:customDetentExpand];
    presentationController.detents = currentDetents;
    [presentationController animateChanges:^{
      presentationController.selectedDetentIdentifier =
          kCustomExpandedDetentIdentifier;
    }];
  } else {
    // Expand to large detent.
    [presentationController animateChanges:^{
      presentationController.selectedDetentIdentifier =
          UISheetPresentationControllerDetentIdentifierLarge;
    }];
  }
}

- (void)setUpBottomSheetPresentationController {
  self.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  presentationController.preferredCornerRadius = kHalfSheetCornerRadius;
}

- (void)setUpBottomSheetDetents {
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  if (@available(iOS 16, *)) {
    CGFloat bottomSheetHeight = [self preferredHeightForContent];
    auto resolver = ^CGFloat(
        id<UISheetPresentationControllerDetentResolutionContext> context) {
      return bottomSheetHeight;
    };
    UISheetPresentationControllerDetent* customDetent =
        [UISheetPresentationControllerDetent
            customDetentWithIdentifier:kCustomMinimizedDetentIdentifier
                              resolver:resolver];
    presentationController.detents = @[ customDetent ];
    presentationController.selectedDetentIdentifier =
        kCustomMinimizedDetentIdentifier;
  } else {
    presentationController.detents = @[
      [UISheetPresentationControllerDetent mediumDetent],
      [UISheetPresentationControllerDetent largeDetent]
    ];
    presentationController.selectedDetentIdentifier =
        UISheetPresentationControllerDetentIdentifierMedium;
  }
}

@end
