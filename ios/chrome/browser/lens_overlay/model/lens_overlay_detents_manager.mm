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

// The detent height in points for the 'peak' state of the bottom sheet.
const CGFloat kPeakDetentHeight = 100.0;

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

  return SheetDimensionState::kHidden;
}

- (void)setPresentationStrategy:
    (SheetDetentPresentationStategy)presentationStrategy {
  _presentationStrategy = presentationStrategy;
  if ([self isInMediumDetent] || [self isInLargeDetent]) {
    // Refresh the detents presentation for the unrestricted state.
    [self adjustDetentsForState:SheetDetentStateUnrestrictedMovement];
  }
}

#pragma mark - Public methods

- (void)adjustDetentsForState:(SheetDetentState)state {
  [_sheet animateChanges:^{
    [self setDetentsForState:state];
  }];
}

- (void)requestMaximizeBottomSheet {
  [_sheet animateChanges:^{
    _sheet.selectedDetentIdentifier =
        UISheetPresentationControllerDetentIdentifierLarge;
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
  return [identifier
      isEqualToString:UISheetPresentationControllerDetentIdentifierLarge];
}

- (CGFloat)windowHeight {
  return _window.frame.size.height;
}

- (void)setDetentsForState:(SheetDetentState)state {
  switch (state) {
    case SheetDetentStateUnrestrictedMovement:
      _sheet.detents = @[ [self mediumDetent], [self largeDetent] ];
      _sheet.largestUndimmedDetentIdentifier = [self largeDetent].identifier;
      _sheet.selectedDetentIdentifier = [self mediumDetent].identifier;
      break;
    case SheetDetentStatePeakEnabled:
      _sheet.detents = @[ [self peakDetent] ];
      _sheet.largestUndimmedDetentIdentifier = kPeakSheetDetentIdentifier;
      _sheet.selectedDetentIdentifier = kPeakSheetDetentIdentifier;
      break;
    case SheetDetentStateConsentDialog:
      _sheet.detents = @[ [self consentDetent] ];
      _sheet.largestUndimmedDetentIdentifier = kConsentSheetDetentIdentifier;
      _sheet.selectedDetentIdentifier = kConsentSheetDetentIdentifier;
      break;
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
  return [UISheetPresentationControllerDetent largeDetent];
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

@end
