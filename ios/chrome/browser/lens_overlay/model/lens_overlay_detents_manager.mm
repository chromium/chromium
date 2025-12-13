// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_detents_manager.h"

#import "base/ios/block_types.h"
#import "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_bottom_sheet_detents_interactor.h"

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

// The identifier for the custom large and medium detents.
NSString* const kCustomLargeDetentIdentifier = @"kCustomLargeDetentIdentifier";
NSString* const kCustomMediumDetentIdentifier =
    @"kCustomMediumDetentIdentifier";

// The detent height in points for the 'peak' state of the bottom sheet.
const CGFloat kPeakDetentHeight = 100.0;

// TODO(crbug.com/408391355): Remove once real value is surfaced to Chromium.
// Height of the HUD elements header bar in the selection UI.
const CGFloat kHUDHeaderHeight = 54.0;

// The ammount of obstruction (in points) on top of the HUD header.
const CGFloat kHUDObstructionAmmountSystemDetents = 8.0;
const CGFloat kHUDObstructionAmmountLensOverlayBottomSheet = 36.0;

// The percentage of the screen that will be covered by the bottom sheet in
// selection mode.
const CGFloat kSelectionSheetHeightRatio = 0.5;
// The percentage of the screen that will be covered by the bottom sheet in
// translate mode.
const CGFloat kTranslateSheetHeightRatio = 0.33;

}  // namespace

@interface LensOverlayDetentsManager () <UISheetPresentationControllerDelegate,
                                         LensOverlayBottomSheetDetentsDelegate>

// The interactor for the detents managed by this instance.
@property(nonatomic, strong)
    LensOverlayBottomSheetDetentInteractor* sheetDetentInteractor;

// Whether the bottom sheet being managed is in the medium detent dimension.
- (BOOL)isInMediumDetent;

// Whether the bottom sheet being managed is in the large detent dimension.
- (BOOL)isInLargeDetent;

// The height of the base window of the presentation
- (CGFloat)windowHeight;

// Reports to the delegate and logs metrics as necessary.
// Pass `isUserGestureInitiated` when the change is due to a user gesture.
- (void)reportDimensionChangeIfNeeded:(BOOL)isUserGestureInitiated;

// A detent for the sheet that’s approximately the full height of the screen
// (excluding the top safe area which is not covered).
- (LensOverlayBottomSheetDetentProxy*)largeDetent;

// A detent for the sheet that’s approximately half the height of the screen
// when presenting for the selection filter and on third of the screen for
// the translation filter.
- (LensOverlayBottomSheetDetentProxy*)mediumDetent;

// The detent in which to present the consent dialog.
- (LensOverlayBottomSheetDetentProxy*)consentDetent;

// The detent of the peak state that covers a small portion of the screen,
// allowing most of the content behind the sheet to be visible.
- (LensOverlayBottomSheetDetentProxy*)peakDetent;

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
    _sheetDetentInteractor = [[LensOverlayBottomSheetDetentInteractor alloc]
        initWithSystemSheetPresentationController:sheet];
    _sheet.delegate = self;
    _latestReportedDimension = SheetDimensionState::kHidden;
    _window = window;
    _presentationStrategy = presentationStrategy;
  }

  return self;
}

- (instancetype)initWithLensOverlayBottomSheet:
                    (id<LensOverlayBottomSheet>)lensOverlayBottomSheet
                                        window:(UIWindow*)window
                          presentationStrategy:(SheetDetentPresentationStategy)
                                                   presentationStrategy {
  self = [super init];
  if (self) {
    _sheetDetentInteractor = [[LensOverlayBottomSheetDetentInteractor alloc]
        initWithLensOverlayBottomSheet:lensOverlayBottomSheet];
    lensOverlayBottomSheet.detentsDelegate = self;
    _latestReportedDimension = SheetDimensionState::kHidden;
    _window = window;
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

  NSString* identifier = self.sheetDetentInteractor.selectedDetentIdentifier;
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

- (void)adjustDetentsForState:(SheetDetentState)state animated:(BOOL)animated {
  switch (state) {
    case SheetDetentStateUnrestrictedMovement:
      self.sheetDetentInteractor.detents =
          @[ [self mediumDetent], [self largeDetent] ];
      [self.sheetDetentInteractor
          setLargestUndimmedDetentIdentifier:kCustomLargeDetentIdentifier];
      [self.sheetDetentInteractor
          setSelectedDetentIdentifier:[self mediumDetent].identifier
                             animated:animated];
      break;
    case SheetDetentStatePeakEnabled:
      self.sheetDetentInteractor.detents = @[ [self peakDetent] ];
      [self.sheetDetentInteractor
          setLargestUndimmedDetentIdentifier:kPeakSheetDetentIdentifier];
      [self.sheetDetentInteractor
          setSelectedDetentIdentifier:kPeakSheetDetentIdentifier
                             animated:animated];
      break;
    case SheetDetentStateConsentDialog:
      self.sheetDetentInteractor.detents = @[ [self consentDetent] ];
      [self.sheetDetentInteractor
          setSelectedDetentIdentifier:kConsentSheetDetentIdentifier
                             animated:animated];
      break;
    case SheetDetentStateInfoMessage:
      self.sheetDetentInteractor.detents = @[ [self infoMessageDetent] ];
      [self.sheetDetentInteractor
          setLargestUndimmedDetentIdentifier:kInfoMessageSheetDetentIdentifier];
      [self.sheetDetentInteractor
          setSelectedDetentIdentifier:kInfoMessageSheetDetentIdentifier
                             animated:animated];
  }

  [self reportDimensionChangeIfNeeded:NO];
}

- (void)adjustDetentsForState:(SheetDetentState)state {
  [self adjustDetentsForState:state animated:YES];
}

- (void)requestMaximizeBottomSheet {
  [self.sheetDetentInteractor
      setSelectedDetentIdentifier:kCustomLargeDetentIdentifier
                         animated:YES];
  [self reportDimensionChangeIfNeeded:NO];
}

- (void)requestMinimizeBottomSheet {
  [self.sheetDetentInteractor
      setSelectedDetentIdentifier:kCustomMediumDetentIdentifier
                         animated:YES];
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
  [self.sheetDetentInteractor setSelectedDetentIdentifier:nil animated:NO];
  [_delegate lensOverlayDetentsManagerDidChangeDimensionState:self];
}

#pragma mark - LensOverlayBottomSheetDetentsDelegate

- (void)lensOverlayBottomSheetDidChangeSelectedDetentIdentifier:
    (id<LensOverlayBottomSheet>)bottomSheetPresenter {
  [self reportDimensionChangeIfNeeded:YES];
}

- (BOOL)lensOverlayBottomSheetShouldDismiss:
    (id<LensOverlayBottomSheet>)bottomSheet {
  UMA_HISTOGRAM_ENUMERATION("Lens.BottomSheet.PositionAfterSwipe",
                            SheetDimensionState::kHidden);
  if (!_delegate) {
    return YES;
  }
  return [_delegate lensOverlayDetentsManagerShouldDismissBottomSheet:self];
}

#pragma mark - Private

- (BOOL)isInMediumDetent {
  NSString* identifier = self.sheetDetentInteractor.selectedDetentIdentifier;
  return [identifier isEqualToString:kCustomMediumDetentIdentifier] ||
         [identifier isEqualToString:kTranslateModeMediumDetentIdentifier];
}

- (BOOL)isInLargeDetent {
  NSString* identifier = self.sheetDetentInteractor.selectedDetentIdentifier;
  return [identifier isEqualToString:kCustomLargeDetentIdentifier];
}

- (CGFloat)windowHeight {
  return _window.frame.size.height;
}

- (CGFloat)windowSafeAreaHeight {
  return _window.safeAreaLayoutGuide.layoutFrame.size.height;
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

- (LensOverlayBottomSheetDetentProxy*)largeDetent {
  __weak __typeof(self) weakSelf = self;
  return [self.sheetDetentInteractor
      detentWithIdentifier:kCustomLargeDetentIdentifier
            heightResolver:^{
              CGFloat obstruction =
                  weakSelf.sheetDetentInteractor.usesSystemPresentation
                      ? kHUDObstructionAmmountSystemDetents
                      : kHUDObstructionAmmountLensOverlayBottomSheet;

              return MAX([weakSelf windowSafeAreaHeight] - kHUDHeaderHeight +
                             obstruction,
                         0);
            }];
}

- (LensOverlayBottomSheetDetentProxy*)mediumDetent {
  NSString* identifier =
      _presentationStrategy == SheetDetentPresentationStategySelection
          ? kCustomMediumDetentIdentifier
          : kTranslateModeMediumDetentIdentifier;

  CGFloat ratio =
      _presentationStrategy == SheetDetentPresentationStategySelection
          ? kSelectionSheetHeightRatio
          : kTranslateSheetHeightRatio;

  CGFloat resolvedHeight = [self windowHeight] * ratio;

  return [self.sheetDetentInteractor detentWithIdentifier:identifier
                                                   height:resolvedHeight];
}

- (LensOverlayBottomSheetDetentProxy*)consentDetent {
  __weak UIViewController* presentedViewController =
      _sheet.presentedViewController;

  return [self.sheetDetentInteractor
      detentWithIdentifier:kConsentSheetDetentIdentifier
            heightResolver:^{
              return presentedViewController.preferredContentSize.height;
            }];
}

- (LensOverlayBottomSheetDetentProxy*)peakDetent {
  return [self.sheetDetentInteractor
      detentWithIdentifier:kPeakSheetDetentIdentifier
                    height:kPeakDetentHeight];
}

- (LensOverlayBottomSheetDetentProxy*)infoMessageDetent {
  __weak __typeof(self) weakSelf = self;
  return [self.sheetDetentInteractor
      detentWithIdentifier:kInfoMessageSheetDetentIdentifier
            heightResolver:^{
              return weakSelf.infoMessageHeight;
            }];
}

@end
