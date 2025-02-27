// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_detents_manager.h"

namespace {

NSString* const kCustomConsentSheetDetentIdentifier =
    @"kCustomConsentSheetDetentIdentifier";

NSString* const kTranslateModeMediumDetentIdentifier =
    @"kTranslateModeMediumDetentIdentifier";

NSString* const kCustomPeakSheetDetentIdentifier =
    @"kCustomPeakSheetDetentIdentifier";

// The detent height in points for the 'peak' state of the bottom sheet.
const CGFloat kPeakDetentHeight = 100;

// The percentage of the screen that will be covered by the bottom sheet in
// translate mode.
const CGFloat kTranslateSheetHeightRatio = 0.33;
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
  // The presentation controller that manages the appearance and behavior of
  // the gottom sheet.
  __weak UISheetPresentationController* _sheet;

  // The latest bottom sheet dimension that was reported.
  SheetDimensionState _latestReportedDimension;

  // The base window of the presentation.
  __weak UIWindow* _window;
}

- (instancetype)initWithBottomSheet:(UISheetPresentationController*)sheet
                             window:(UIWindow*)window {
  return [self initWithBottomSheet:sheet
                            window:window
              presentationStrategy:SheetDetentPresentationStategySelection];
}

- (instancetype)initWithBottomSheet:(UISheetPresentationController*)sheet
                             window:(UIWindow*)window
               presentationStrategy:
                   (SheetDetentPresentationStategy)presentationStrategy {
  self = [super init];
  if (self) {
    _sheet = sheet;
    _latestReportedDimension = SheetDimensionStateHidden;
    _window = window;
    _sheet.delegate = self;
    _presentationStrategy = presentationStrategy;
  }

  return self;
}

#pragma mark - Public

- (CGFloat)estimatedMediumDetentHeight {
  switch (_presentationStrategy) {
    case SheetDetentPresentationStategySelection:
      return self.windowHeight / 2;
    case SheetDetentPresentationStategyTranslate:
      return self.windowHeight * kTranslateSheetHeightRatio;
    default:
      return 0;
  }
}

- (SheetDimensionState)sheetDimension {
  NSString* identifier = _sheet.selectedDetentIdentifier;
  BOOL isInMediumDetent = self.isInMediumDetent;
  BOOL isInLargestDetent = self.isInLargeDetent;
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

- (void)setPresentationStrategy:
    (SheetDetentPresentationStategy)presentationStrategy {
  _presentationStrategy = presentationStrategy;
  BOOL unrestrictedMovement = self.isInMediumDetent || self.isInLargeDetent;
  if (unrestrictedMovement) {
    // Refresh the detents presentation for the unrestricted state.
    [self adjustDetentsForState:SheetDetentStateUnrestrictedMovement];
  }
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

- (BOOL)isInMediumDetent {
  NSString* identifier = _sheet.selectedDetentIdentifier;
  return [identifier isEqualToString:
                         UISheetPresentationControllerDetentIdentifierMedium] ||
         [identifier isEqualToString:kTranslateModeMediumDetentIdentifier];
}

- (BOOL)isInLargeDetent {
  NSString* identifier = _sheet.selectedDetentIdentifier;
  return [identifier
      isEqualToString:UISheetPresentationControllerDetentIdentifierLarge];
}

- (CGFloat)windowHeight {
  return _window.frame.size.height;
}

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
  if (_presentationStrategy == SheetDetentPresentationStategySelection) {
    return [UISheetPresentationControllerDetent mediumDetent];
  }

  CGFloat resolvedHeight = self.windowHeight * kTranslateSheetHeightRatio;
  auto heightResolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return resolvedHeight;
  };
  return [UISheetPresentationControllerDetent
      customDetentWithIdentifier:kTranslateModeMediumDetentIdentifier
                        resolver:heightResolver];
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
