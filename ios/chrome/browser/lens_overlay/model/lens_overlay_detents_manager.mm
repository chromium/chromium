// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_detents_manager.h"

#import "base/metrics/histogram_macros.h"

namespace {

NSString* const kConsentSheetDetentIdentifier =
    @"kConsentSheetDetentIdentifier";

// The identifier for the medium detent when presenting in the translate filter
// strategy.
NSString* const kTranslateModeMediumDetentIdentifier =
    @"kTranslateModeMediumDetentIdentifier";

// The identifier for the peak detent.
NSString* const kPeakSheetDetentIdentifier = @"kPeakSheetDetentIdentifier";

// The identifier for the info message state.
NSString* const kInfoMessageSheetDetentIdentifier =
    @"kInfoMessageSheetDetentIdentifier";

// The identifier for the custom large detent.
NSString* const kCustomLargeDetentIdentifier = @"kCustomLargeDetentIdentifier";

// The detent height in points for the 'peak' state of the bottom sheet.
const CGFloat kPeakDetentHeight = 100.0;

// TODO(crbug.com/408391355): Remove once real value is surfaced to Chromium.
// Height of the HUD elements header bar in the selection UI.
const CGFloat kHUDHeaderHeight = 54.0;

// The ammount of obstruction (in points) on top of the HUD header.
const CGFloat kHUDObstructionAmmount = 8.0;

// The percentage of the screen that will be covered by the bottom sheet in
// translate mode.
const CGFloat kTranslateSheetHeightRatio = 0.33;

}  // namespace

@interface LensOverlayDetentsManager () <UISheetPresentationControllerDelegate>

// Whether the bottom sheet being managed is in the medium detent dimension.
- (BOOL)isInMediumDetent;

// Whether the bottom sheet being managed is in the large detent dimension.
- (BOOL)isInLargeDetent;

// The height of The base window of the presentation
- (CGFloat)windowHeight;

// Changes the current set of available detents for a given a desired sheet
// state. Also notifies the delegate of any change in detents.
- (void)setDetentsForState:(SheetDetentState)state;

// Reports to the delegate and logs metrics as necessary.
// Pass `isUserGestureInitiated` when the change is due to a user gesture.
- (void)reportDimensionChangeIfNeeded:(BOOL)isUserGestureInitiated;

// A detent for the sheet that’s approximately the full height of the screen
// (excluding the top safe area which is not covered).
- (UISheetPresentationControllerDetent*)largeDetent;

// A detent for the sheet that’s approximately half the height of the screen
// when presenting for the selection filter and on third of the screen for
// the translation filter.
- (UISheetPresentationControllerDetent*)mediumDetent;

// The detent in which to present the consent dialog.
- (UISheetPresentationControllerDetent*)consentDetent;

// The detent of the peak state that covers a small portion of the screen,
// allowing most of the content behind the sheet to be visible.
- (UISheetPresentationControllerDetent*)peakDetent;

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
    _latestReportedDimension = SheetDimensionState::kHidden;
    _window = window;
    _sheet.delegate = self;
    _presentationStrategy = presentationStrategy;
  }

  return self;
}

#pragma mark - Public properties

- (CGFloat)estimatedMediumDetentHeight {
  switch (_presentationStrategy) {
    case SheetDetentPresentationStategySelection:
      return [self windowHeight] / 2;
    case SheetDetentPresentationStategyTranslate:
      return [self windowHeight] * kTranslateSheetHeightRatio;
    default:
      return 0;
  }
}

- (SheetDimensionState)sheetDimension {
  if ([self isInLargeDetent]) {
    return SheetDimensionState::kLarge;
  }
  if ([self isInMediumDetent]) {
    return SheetDimensionState::kMedium;
  }

  NSString* identifier = _sheet.selectedDetentIdentifier;
  if ([identifier isEqualToString:kPeakSheetDetentIdentifier]) {
    return SheetDimensionState::kPeaking;
  }

  if ([identifier isEqualToString:kConsentSheetDetentIdentifier]) {
    return SheetDimensionState::kConsent;
  }

  if ([identifier isEqualToString:kInfoMessageSheetDetentIdentifier]) {
    return SheetDimensionState::kInfoMessage;
  }

  return SheetDimensionState::kHidden;
}

- (void)setPresentationStrategy:
    (SheetDetentPresentationStategy)presentationStrategy {
  _presentationStrategy = presentationStrategy;
  [self adjustDetentsForState:SheetDetentStateUnrestrictedMovement];
}

#pragma mark - Public methods

- (void)adjustDetentsForState:(SheetDetentState)state {
  [_sheet animateChanges:^{
    [self setDetentsForState:state];
  }];
}

- (void)requestMaximizeBottomSheet {
  [_sheet animateChanges:^{
    _sheet.selectedDetentIdentifier = kCustomLargeDetentIdentifier;
  }];
  [self reportDimensionChangeIfNeeded:NO];
}

- (void)requestMinimizeBottomSheet {
  [_sheet animateChanges:^{
    _sheet.selectedDetentIdentifier =
        UISheetPresentationControllerDetentIdentifierMedium;
  }];
  [self reportDimensionChangeIfNeeded:NO];
}

#pragma mark - UISheetPresentationControllerDelegate

- (void)sheetPresentationControllerDidChangeSelectedDetentIdentifier:
    (UISheetPresentationController*)sheetPresentationController {
  [self reportDimensionChangeIfNeeded:YES];
}

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  UMA_HISTOGRAM_ENUMERATION("Lens.BottomSheet.PositionAfterSwipe",
                            SheetDimensionState::kHidden);

  if (!_delegate) {
    return YES;
  }
  return [_delegate lensOverlayDetentsManagerShouldDismissBottomSheet:self];
}

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  _sheet.selectedDetentIdentifier = nil;
  [_delegate lensOverlayDetentsManagerDidChangeDimensionState:self];
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
  return [identifier isEqualToString:kCustomLargeDetentIdentifier];
}

- (CGFloat)windowHeight {
  return _window.frame.size.height;
}

- (CGFloat)windowSafeAreaHeight {
  return _window.safeAreaLayoutGuide.layoutFrame.size.height;
}

- (void)setDetentsForState:(SheetDetentState)state {
  switch (state) {
    case SheetDetentStateUnrestrictedMovement:
      _sheet.detents = @[ [self mediumDetent], [self largeDetent] ];
      _sheet.largestUndimmedDetentIdentifier = kCustomLargeDetentIdentifier;
      _sheet.selectedDetentIdentifier = [self mediumDetent].identifier;
      break;
    case SheetDetentStatePeakEnabled:
      _sheet.detents = @[ [self peakDetent] ];
      _sheet.largestUndimmedDetentIdentifier = kPeakSheetDetentIdentifier;
      _sheet.selectedDetentIdentifier = kPeakSheetDetentIdentifier;
      break;
    case SheetDetentStateConsentDialog:
      _sheet.detents = @[ [self consentDetent] ];
      _sheet.selectedDetentIdentifier = kConsentSheetDetentIdentifier;
      break;
    case SheetDetentStateInfoMessage:
      _sheet.detents = @[ [self infoMessageDetent] ];
      _sheet.largestUndimmedDetentIdentifier =
          kInfoMessageSheetDetentIdentifier;
      _sheet.selectedDetentIdentifier = kInfoMessageSheetDetentIdentifier;
  }

  [self reportDimensionChangeIfNeeded:NO];
}

// Reports to the delegate and logs metrics as necessary.
- (void)reportDimensionChangeIfNeeded:(BOOL)isUserGestureInitiated {
  // Maintain a strong reference to self throughout this method to prevent
  // premature deallocation. `lensOverlayDetentsManagerDidChangeDimensionState`
  // could trigger self's deallocation if the dimension state change results in
  // the overlay being hidden. (see https://crbug.com/403224762).
  LensOverlayDetentsManager* strongSelf = self;

  if (self.sheetDimension != _latestReportedDimension) {
    if (isUserGestureInitiated) {
      UMA_HISTOGRAM_ENUMERATION("Lens.BottomSheet.PositionAfterSwipe",
                                self.sheetDimension);
    }

    _latestReportedDimension = strongSelf.sheetDimension;
    [_delegate lensOverlayDetentsManagerDidChangeDimensionState:strongSelf];
  }
}

- (UISheetPresentationControllerDetent*)largeDetent {
  __weak __typeof(self) weakSelf = self;
  auto heightResolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return MAX([weakSelf windowSafeAreaHeight] - kHUDHeaderHeight +
                   kHUDObstructionAmmount,
               0);
  };

  return [UISheetPresentationControllerDetent
      customDetentWithIdentifier:kCustomLargeDetentIdentifier
                        resolver:heightResolver];
}

- (UISheetPresentationControllerDetent*)mediumDetent {
  if (_presentationStrategy == SheetDetentPresentationStategySelection) {
    return [UISheetPresentationControllerDetent mediumDetent];
  }

  CGFloat resolvedHeight = [self windowHeight] * kTranslateSheetHeightRatio;
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
      customDetentWithIdentifier:kConsentSheetDetentIdentifier
                        resolver:consentHeightResolver];
}

- (UISheetPresentationControllerDetent*)peakDetent {
  auto peakHeightResolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return kPeakDetentHeight;
  };
  return [UISheetPresentationControllerDetent
      customDetentWithIdentifier:kPeakSheetDetentIdentifier
                        resolver:peakHeightResolver];
}

- (UISheetPresentationControllerDetent*)infoMessageDetent {
  __weak __typeof(self) weakSelf = self;
  auto infoMessageHeightResolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return weakSelf.infoMessageHeight;
  };
  return [UISheetPresentationControllerDetent
      customDetentWithIdentifier:kInfoMessageSheetDetentIdentifier
                        resolver:infoMessageHeightResolver];
}

@end
