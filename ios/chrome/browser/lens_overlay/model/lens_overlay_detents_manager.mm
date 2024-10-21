// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_detents_manager.h"

namespace {

NSString* const kCustomConsentSheetDetentIdentifier =
    @"kCustomConsentSheetDetentIdentifier";

NSString* const kCustomPeakSheetDetentIdentifier =
    @"kCustomPeakSheetDetentIdentifier";

const CGFloat kPeakDetentHeight = 100;
}  // namespace

@interface LensOverlayDetentsManager (Private)

@property(nonatomic, readonly) UISheetPresentationControllerDetent* largeDetent;
@property(nonatomic, readonly)
    UISheetPresentationControllerDetent* mediumDetent;
@property(nonatomic, readonly)
    UISheetPresentationControllerDetent* consentDetent;
@property(nonatomic, readonly) UISheetPresentationControllerDetent* peakDetent;

@end

@implementation LensOverlayDetentsManager {
  __weak UISheetPresentationController* _sheet;
}

- (instancetype)initWithBottomSheet:(UISheetPresentationController*)sheet {
  self = [super init];
  if (self) {
    _sheet = sheet;
  }

  return self;
}

#pragma mark - Public

- (BOOL)isInLargestDetent {
  return [_sheet.selectedDetentIdentifier
      isEqualToString:UISheetPresentationControllerDetentIdentifierLarge];
}

- (BOOL)isPeaking {
  return [_sheet.selectedDetentIdentifier
      isEqualToString:kCustomPeakSheetDetentIdentifier];
}

- (void)adjustDetentsForState:(SheetDetentState)state {
  __weak __typeof(self) weakSelf = self;
  [_sheet animateChanges:^{
    [weakSelf setDetentsForState:state];
  }];
}

- (void)restrictSheetToLargeDetent:(BOOL)restrictToLargeDetent {
  if (restrictToLargeDetent) {
    [self adjustDetentsForState:SheetStateLockedInLargeDetent];
  } else {
    [self adjustDetentsForState:SheetStateUnrestrictedMovement];
  }
}

- (void)requestMaximizeBottomSheet {
  [_sheet animateChanges:^{
    _sheet.selectedDetentIdentifier =
        UISheetPresentationControllerDetentIdentifierLarge;
  }];
}

- (void)requestMinimizeBottomSheet {
  [_sheet animateChanges:^{
    _sheet.selectedDetentIdentifier =
        UISheetPresentationControllerDetentIdentifierMedium;
  }];
}

#pragma mark - Private
- (void)setDetentsForState:(SheetDetentState)state {
  switch (state) {
    case SheetStateUnrestrictedMovement:
      _sheet.detents = @[ self.mediumDetent, self.largeDetent ];
      _sheet.largestUndimmedDetentIdentifier = self.largeDetent.identifier;
      break;
    case SheetStateLockedInLargeDetent:
      _sheet.detents = @[ self.largeDetent ];
      _sheet.largestUndimmedDetentIdentifier = self.largeDetent.identifier;
      _sheet.selectedDetentIdentifier = self.largeDetent.identifier;
      break;
    case SheetStatePeakEnabled:
      _sheet.detents = @[ self.peakDetent ];
      _sheet.largestUndimmedDetentIdentifier = self.peakDetent.identifier;
      _sheet.selectedDetentIdentifier = self.peakDetent.identifier;
      break;
    case SheetStateConsentDialog:
      _sheet.detents = @[ self.consentDetent ];
      _sheet.largestUndimmedDetentIdentifier = self.consentDetent.identifier;
      break;
  }
}

- (UISheetPresentationControllerDetent*)largeDetent {
  return [UISheetPresentationControllerDetent largeDetent];
}

- (UISheetPresentationControllerDetent*)mediumDetent {
  return [UISheetPresentationControllerDetent mediumDetent];
}

- (UISheetPresentationControllerDetent*)consentDetent {
  __weak UIViewController* presentedViewController =
      _sheet.presentedViewController;
  auto consentHeightResolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return presentedViewController.preferredContentSize.height;
  };
  return [UISheetPresentationControllerDetent
      customDetentWithIdentifier:kCustomConsentSheetDetentIdentifier
                        resolver:consentHeightResolver];
}

- (UISheetPresentationControllerDetent*)peakDetent {
  auto peakHeightResolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return kPeakDetentHeight;
  };
  return [UISheetPresentationControllerDetent
      customDetentWithIdentifier:kCustomPeakSheetDetentIdentifier
                        resolver:peakHeightResolver];
}

@end
