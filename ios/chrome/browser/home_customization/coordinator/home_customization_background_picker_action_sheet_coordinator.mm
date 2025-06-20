// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_picker_action_sheet_coordinator.h"

#import "components/image_fetcher/core/image_fetcher_service.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_color_picker_mediator.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_photo_picker_coordinator.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_picker_action_sheet_mediator.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_preset_gallery_picker_mediator.h"
#import "ios/chrome/browser/home_customization/model/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/model/home_background_image_service.h"
#import "ios/chrome/browser/home_customization/model/home_background_image_service_factory.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_color_picker_mutator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_color_picker_view_controller.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_photo_library_picker_view_controller.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_picker_action_sheet_presentation_delegate.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_preset_gallery_picker_view_controller.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_logo_vendor_provider.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/image_fetcher/model/image_fetcher_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The height of the menu's initial detent, which roughly represents a header
// and 3 cells.
CGFloat const kInitialDetentHeight = 350;

// The corner radius of the customization menu sheet.
CGFloat const kSheetCornerRadius = 30;

}  // namespace

@interface HomeCustomizationBackgroundPickerActionSheetCoordinator () <
    UIAdaptivePresentationControllerDelegate,
    HomeCustomizationLogoVendorProvider,
    HomeCustomizationBackgroundPickerActionSheetPresentationDelegate> {
  // The mediator of the background picker action sheet.
  HomeCustomizationBackgroundPickerActionSheetMediator* _mediator;

  // The mediator for the color picker.
  HomeCustomizationBackgroundColorPickerMediator*
      _backgroundColorPickerMediator;

  // The mediator for the background preset gallery picker.
  HomeCustomizationBackgroundPresetGalleryPickerMediator*
      _backgroundPresetGalleryPickerMediator;

  // The coordinator for the photo picker.
  HomeCustomizationBackgroundPhotoPickerCoordinator* _photoPickerCoordinator;

  // The main view controller presented by the base view controller.
  UIViewController* _mainViewController;

  // Stores the most recently applied background configuration.
  id<BackgroundCustomizationConfiguration> _lastAppliedBackgroundConfiguration;
}

@end

@implementation HomeCustomizationBackgroundPickerActionSheetCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController
                                   browser:browser
                                     title:nil
                                   message:nil
                             barButtonItem:nil];
  return self;
}

- (void)start {
  __weak __typeof(self) weakSelf = self;
  image_fetcher::ImageFetcherService* imageFetcherService =
      ImageFetcherServiceFactory::GetForProfile(self.profile);
  HomeBackgroundImageService* homeBackgroundImageService =
      HomeBackgroundImageServiceFactory::GetForProfile(self.profile);

  _mediator =
      [[HomeCustomizationBackgroundPickerActionSheetMediator alloc] init];
  _backgroundColorPickerMediator =
      [[HomeCustomizationBackgroundColorPickerMediator alloc] init];
  _backgroundPresetGalleryPickerMediator =
      [[HomeCustomizationBackgroundPresetGalleryPickerMediator alloc]
          initWithImageFetcherService:imageFetcherService
           homeBackgroundImageService:homeBackgroundImageService];

  [self
      addItemWithTitle:
          l10n_util::GetNSStringWithFixup(
              IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_PICKER_PRESET_GALLERY_TITLE)
                action:^{
                  [weakSelf presentPickerWithStyle:
                                HomeCustomizationBackgroundStyle::kPreset];
                }
                 style:UIAlertActionStyleDefault];

  [self
      addItemWithTitle:
          l10n_util::GetNSStringWithFixup(
              IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_PICKER_PHOTO_LIBRARY_TITLE)
                action:^{
                  [weakSelf presentPhotoLibraryPicker];
                }
                 style:UIAlertActionStyleDefault];

  [self addItemWithTitle:
            l10n_util::GetNSStringWithFixup(
                IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_PICKER_COLOR_TITLE)
                  action:^{
                    [weakSelf presentPickerWithStyle:
                                  HomeCustomizationBackgroundStyle::kColor];
                  }
                   style:UIAlertActionStyleDefault];

  [super start];
}

- (void)stop {
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  _backgroundColorPickerMediator = nil;
  _backgroundPresetGalleryPickerMediator = nil;
  if (_photoPickerCoordinator) {
    [_photoPickerCoordinator stop];
    _photoPickerCoordinator = nil;
  }
  [super stop];
}

#pragma mark - HomeCustomizationLogoVendorProvider

- (id<LogoVendor>)provideLogoVendor {
  return ios::provider::CreateLogoVendor(
      self.browser, self.browser->GetWebStateList()->GetActiveWebState());
}

#pragma mark - HomeCustomizationBackgroundPickerActionSheetPresentationDelegate

- (void)applyBackgroundForConfiguration:
    (id<BackgroundCustomizationConfiguration>)backgroundConfiguration {
  _lastAppliedBackgroundConfiguration = backgroundConfiguration;
  [_mediator applyBackgroundForConfiguration:backgroundConfiguration];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self dismissMenu];
}

#pragma mark - Private functions

// Presents the background customization picker based on the given type.
- (void)presentPickerWithStyle:(HomeCustomizationBackgroundStyle)pickerStyle {
  switch (pickerStyle) {
    case HomeCustomizationBackgroundStyle::kDefault:
      // Do nothing.
      break;
    case HomeCustomizationBackgroundStyle::kColor:
      _mainViewController = [self createColorPickerViewController];
      _backgroundColorPickerMediator.consumer = (id)_mainViewController;
      [_backgroundColorPickerMediator configureColorPalettes];
      break;
    case HomeCustomizationBackgroundStyle::kPreset:
      _mainViewController = [self createPresetGalleryPickerViewController];
      _backgroundPresetGalleryPickerMediator.consumer = (id)_mainViewController;
      [_backgroundPresetGalleryPickerMediator loadBackgroundConfigurations];
      break;
  }

  _mainViewController.modalPresentationStyle = UIModalPresentationFormSheet;

  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:_mainViewController];

  navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  navigationController.presentationController.delegate = self;

  // Configure the presentation controller with a custom initial detent.
  UISheetPresentationController* presentationController =
      navigationController.sheetPresentationController;

  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                           target:self
                           action:@selector(dismissMenu)];
  _mainViewController.navigationItem.rightBarButtonItem = dismissButton;
  _mainViewController.navigationItem.leftBarButtonItem = nil;

  presentationController.preferredCornerRadius = kSheetCornerRadius;

  auto detentResolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return kInitialDetentHeight;
  };
  UISheetPresentationControllerDetent* initialDetent =
      [UISheetPresentationControllerDetent
          customDetentWithIdentifier:kBottomSheetDetentIdentifier
                            resolver:detentResolver];

  NSMutableArray<UISheetPresentationControllerDetent*>* detents =
      [NSMutableArray array];
  [detents addObject:initialDetent];

  // The preset gallery can be expanded full screen and therefore a grabber is
  // shown.
  if (pickerStyle == HomeCustomizationBackgroundStyle::kPreset) {
    presentationController.prefersGrabberVisible = YES;
    [detents addObject:[UISheetPresentationControllerDetent largeDetent]];
  }
  presentationController.detents = detents;

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

// Creates the view controller for picking a solid background color.
- (HomeCustomizationBackgroundColorPickerViewController*)
    createColorPickerViewController {
  HomeCustomizationBackgroundColorPickerViewController* mainViewController =
      [[HomeCustomizationBackgroundColorPickerViewController alloc] init];
  mainViewController.presentationDelegate = self;
  mainViewController.mutator = _backgroundColorPickerMediator;
  return mainViewController;
}

// Creates the view controller for picking a background image from a preselected
// gallery collection.
- (HomeCustomizationBackgroundPresetGalleryPickerViewController*)
    createPresetGalleryPickerViewController {
  HomeCustomizationBackgroundPresetGalleryPickerViewController*
      mainViewController =
          [[HomeCustomizationBackgroundPresetGalleryPickerViewController alloc]
              init];
  mainViewController.logoVendorProvider = self;
  mainViewController.presentationDelegate = self;
  mainViewController.mutator = _backgroundPresetGalleryPickerMediator;
  return mainViewController;
}

// Presents the view controller for selecting a background photo
// from the device's photo library.
- (void)presentPhotoLibraryPicker {
  // Create and start the photo picker coordinator
  _photoPickerCoordinator =
      [[HomeCustomizationBackgroundPhotoPickerCoordinator alloc]
          initWithBaseViewController:self.baseViewController
                             browser:self.browser];
  [_photoPickerCoordinator start];
}

// Dismisses the customization menu.
- (void)dismissMenu {
  if (!_mainViewController) {
    return;
  }

  // Add the last applied background to the recently used list only when the
  // main view controller is dismissed. This avoids adding every background
  // the user tapped and instead keeps only the final selection.
  if (_lastAppliedBackgroundConfiguration) {
    [_mediator addBackgroundToRecentlyUsed:_lastAppliedBackgroundConfiguration];
  }

  [_mainViewController dismissViewControllerAnimated:YES completion:nil];
}

@end
