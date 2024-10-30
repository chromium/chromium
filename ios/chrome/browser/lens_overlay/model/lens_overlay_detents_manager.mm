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

@interface LensOverlayDetentsManager (Private) <
    UISheetPresentationControllerDelegate>

@property(nonatomic, readonly) UISheetPresentationControllerDetent* largeDetent;
@property(nonatomic, readonly)
    UISheetPresentationControllerDetent* mediumDetent;
@property(nonatomic, readonly)
    UISheetPresentationControllerDetent* consentDetent;
@property(nonatomic, readonly) UISheetPresentationControllerDetent* peakDetent;

@end

@implementation LensOverlayDetentsManager {
  __weak UISheetPresentationController* _sheet;
  SheetDimensionState _latestReportedDimension;
}

- (instancetype)initWithBottomSheet:(UISheetPresentationController*)sheet {
  self = [super init];
  if (self) {
    _sheet = sheet;
    _latestReportedDimension = SheetDimensionStateHidden;
    _sheet.delegate = self;
  }

  return self;
}

#pragma mark - Public

- (SheetDimensionState)sheetDimension {
  NSString* identifier = _sheet.selectedDetentIdentifier;
  BOOL isInMediumDetent = [identifier
      isEqualToString:UISheetPresentationControllerDetentIdentifierMedium];
  BOOL isInLargestDetent = [identifier
      isEqualToString:UISheetPresentationControllerDetentIdentifierLarge];
  BOOL isPeaking =
      [identifier isEqualToString:kCustomPeakSheetDetentIdentifier];
  BOOL isConsent =
      [identifier isEqualToString:kCustomConsentSheetDetentIdentifier];
  if (isInLargestDetent) {
    return SheetDimensionStateLarge;
  } else if (isInMediumDetent) {
    return SheetDimensionStateMedium;
  } else if (isPeaking) {
    return SheetDimensionStatePeaking;
  } else if (isConsent) {
    return SheetDimensionStateConsent;
  } else {
    return SheetDimensionStateHidden;
  }
}

- (void)adjustDetentsForState:(SheetDetentState)state {
  __weak __typeof(self) weakSelf = self;
  [_sheet animateChanges:^{
    [weakSelf setDetentsForState:state];
  }];
}

- (void)requestMaximizeBottomSheet {
  [_sheet animateChanges:^{
    _sheet.selectedDetentIdentifier =
        UISheetPresentationControllerDetentIdentifierLarge;
  }];
  [self reportDimensionChangeIfNeeded];
}

- (void)requestMinimizeBottomSheet {
  [_sheet animateChanges:^{
    _sheet.selectedDetentIdentifier =
        UISheetPresentationControllerDetentIdentifierMedium;
  }];
  [self reportDimensionChangeIfNeeded];
}

#pragma mark - UISheetPresentationControllerDelegate

- (void)sheetPresentationControllerDidChangeSelectedDetentIdentifier:
    (UISheetPresentationController*)sheetPresentationController {
  [self reportDimensionChangeIfNeeded];
}

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  if (!_observer) {
    return YES;
  }
  return [_observer bottomSheetShouldDismissFromState:self.sheetDimension];
}

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  _sheet.selectedDetentIdentifier = nil;
  [_observer onBottomSheetDimensionStateChanged:SheetDimensionStateHidden];
}

#pragma mark - Private
- (void)setDetentsForState:(SheetDetentState)state {
  switch (state) {
    case SheetDetentStateUnrestrictedMovement:
      _sheet.detents = @[ self.mediumDetent, self.largeDetent ];
      _sheet.largestUndimmedDetentIdentifier = self.largeDetent.identifier;
      _sheet.selectedDetentIdentifier = self.mediumDetent.identifier;
      break;
    case SheetDetentStatePeakEnabled:
      _sheet.detents = @[ self.peakDetent ];
      _sheet.largestUndimmedDetentIdentifier = self.peakDetent.identifier;
      _sheet.selectedDetentIdentifier = self.peakDetent.identifier;
      break;
    case SheetDetentStateConsentDialog:
      _sheet.detents = @[ self.consentDetent ];
      _sheet.largestUndimmedDetentIdentifier = self.consentDetent.identifier;
      _sheet.selectedDetentIdentifier = self.consentDetent.identifier;
      break;
  }

  [self reportDimensionChangeIfNeeded];
}

- (void)reportDimensionChangeIfNeeded {
  if (self.sheetDimension != _latestReportedDimension) {
    [_observer onBottomSheetDimensionStateChanged:self.sheetDimension];
    _latestReportedDimension = self.sheetDimension;
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
