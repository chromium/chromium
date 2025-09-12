// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_picker_action_sheet_coordinator.h"

#import "base/apple/foundation_util.h"
#import "components/image_fetcher/core/image_fetcher_service.h"
#import "ios/chrome/browser/google/model/google_logo_service_factory.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_color_picker_mediator.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_configuration_mediator.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_photo_picker_coordinator.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_factory.h"
#import "ios/chrome/browser/home_customization/model/home_background_image_service.h"
#import "ios/chrome/browser/home_customization/model/home_background_image_service_factory.h"
#import "ios/chrome/browser/home_customization/ui/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_color_picker_view_controller.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_photo_library_picker_view_controller.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_picker_presentation_delegate.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_preset_gallery_picker_view_controller.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/image_fetcher/model/image_fetcher_service_factory.h"
#import "ios/chrome/browser/ntp/search_engine_logo/mediator/search_engine_logo_mediator.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/grit/ios_strings.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/strings/grit/ui_strings.h"

namespace {

// The height of the menu's initial detent, which roughly represents a header
// and 3 cells.
CGFloat const kInitialDetentHeight = 350;

// The corner radius of the customization menu sheet.
CGFloat const kSheetCornerRadius = 30;

}  // namespace

@interface HomeCustomizationBackgroundPickerActionSheetCoordinator () <
    HomeCustomizationBackgroundPhotoPickerCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate> {
  // The mediator for the color picker.
  HomeCustomizationBackgroundColorPickerMediator*
      _backgroundColorPickerMediator;

  // The mediator for the background preset gallery picker.
  HomeCustomizationBackgroundConfigurationMediator*
      _backgroundConfigurationMediator;

  // The coordinator for the photo picker.
  HomeCustomizationBackgroundPhotoPickerCoordinator* _photoPickerCoordinator;

  // The main view controller presented by the base view controller.
  UIViewController* _mainViewController;

  // The view to which the action sheet popover should be anchored.
  UIView* _sourceView;
}

@end

@implementation HomeCustomizationBackgroundPickerActionSheetCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                sourceView:(UIView*)sourceView {
  self = [super initWithBaseViewController:viewController
                                   browser:browser
                                     title:nil
                                   message:nil
                             barButtonItem:nil];

  if (self) {
    _sourceView = sourceView;
  }
  return self;
}

- (void)start {
  __weak __typeof(self) weakSelf = self;
  image_fetcher::ImageFetcherService* imageFetcherService =
      ImageFetcherServiceFactory::GetForProfile(self.profile);
  HomeBackgroundImageService* homeBackgroundImageService =
      HomeBackgroundImageServiceFactory::GetForProfile(self.profile);
  HomeBackgroundCustomizationService* homeBackgroundCustomizationService =
      HomeBackgroundCustomizationServiceFactory::GetForProfile(self.profile);

  _backgroundColorPickerMediator =
      [[HomeCustomizationBackgroundColorPickerMediator alloc]
          initWithBackgroundCustomizationService:
              homeBackgroundCustomizationService];
  _backgroundConfigurationMediator =
      [[HomeCustomizationBackgroundConfigurationMediator alloc]
          initWithBackgroundCustomizationService:
              homeBackgroundCustomizationService
                             imageFetcherService:imageFetcherService
                      homeBackgroundImageService:homeBackgroundImageService
                        userUploadedImageManager:nil];
  _backgroundConfigurationMediator.delegate = self.presentationDelegate;

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
                  [weakSelf
                      presentPickerWithStyle:HomeCustomizationBackgroundStyle::
                                                 kUserUploaded];
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

  [self addItemWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)
                  action:^{
                    [weakSelf alertControllerDidCancel];
                  }
                   style:UIAlertActionStyleCancel];

  // On iPad, an action sheet is presented as a popover and needs a source view
  // to anchor to, otherwise it will crash.
  if (self.alertController.popoverPresentationController) {
    UIView* presentingView = self.baseViewController.view;
    self.alertController.popoverPresentationController.sourceView =
        presentingView;
    self.alertController.popoverPresentationController.sourceRect =
        [presentingView convertRect:_sourceView.bounds fromView:_sourceView];
  }
  [super start];
}

- (void)stop {
  [_backgroundConfigurationMediator saveCurrentTheme];

  [_mainViewController dismissViewControllerAnimated:YES completion:nil];

  _backgroundColorPickerMediator = nil;
  _backgroundConfigurationMediator = nil;
  if (_photoPickerCoordinator) {
    [_photoPickerCoordinator stop];
    _photoPickerCoordinator = nil;
  }
  [super stop];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  if (_backgroundConfigurationMediator.themeHasChanged) {
    [self.presentationDelegate dismissBackgroundPicker];
  } else {
    // Cancel theme selection just in case.
    [_backgroundConfigurationMediator cancelThemeSelection];
    [self.presentationDelegate cancelBackgroundPicker];
  }
}

#pragma mark - HomeCustomizationBackgroundPhotoPickerCoordinatorDelegate

- (void)photoPickerCoordinatorDidCancel:
    (HomeCustomizationBackgroundPhotoPickerCoordinator*)coordinator {
  [_photoPickerCoordinator stop];
  _photoPickerCoordinator = nil;

  [self.presentationDelegate cancelBackgroundPicker];
}

- (void)photoPickerCoordinatorDidFinish:
    (HomeCustomizationBackgroundPhotoPickerCoordinator*)coordinator {
  [_photoPickerCoordinator stop];
  _photoPickerCoordinator = nil;

  [self.presentationDelegate dismissBackgroundPicker];
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
      _backgroundConfigurationMediator.consumer = (id)_mainViewController;
      [_backgroundColorPickerMediator configureBackgroundConfigurations];
      break;
    case HomeCustomizationBackgroundStyle::kPreset:
      _mainViewController = [self createPresetGalleryPickerViewController];
      _backgroundConfigurationMediator.configurationConsumer =
          (id)_mainViewController;
      _backgroundConfigurationMediator.consumer = (id)_mainViewController;
      [_backgroundConfigurationMediator loadGalleryBackgroundConfigurations];
      break;
    case HomeCustomizationBackgroundStyle::kUserUploaded:
      // Create and start the photo picker coordinator.
      _photoPickerCoordinator =
          [[HomeCustomizationBackgroundPhotoPickerCoordinator alloc]
              initWithBaseViewController:self.baseViewController
                                 browser:self.browser];
      _photoPickerCoordinator.delegate = self;
      _photoPickerCoordinator.searchEngineLogoMediatorProvider =
          self.searchEngineLogoMediatorProvider;
      [_photoPickerCoordinator start];
      return;
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
                           action:@selector(cancelMenu)];
  _mainViewController.navigationItem.rightBarButtonItem = dismissButton;
  _mainViewController.navigationItem.leftBarButtonItem = nil;

  presentationController.prefersEdgeAttachedInCompactHeight = YES;
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

  presentationController.largestUndimmedDetentIdentifier =
      kBottomSheetDetentIdentifier;

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

// Creates the view controller for picking a solid background color.
- (HomeCustomizationBackgroundColorPickerViewController*)
    createColorPickerViewController {
  HomeCustomizationBackgroundColorPickerViewController* mainViewController =
      [[HomeCustomizationBackgroundColorPickerViewController alloc] init];
  mainViewController.presentationDelegate = self.presentationDelegate;
  mainViewController.mutator = _backgroundConfigurationMediator;
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
  mainViewController.searchEngineLogoMediatorProvider =
      self.searchEngineLogoMediatorProvider;
  mainViewController.presentationDelegate = self.presentationDelegate;
  mainViewController.mutator = _backgroundConfigurationMediator;
  return mainViewController;
}

// Cancels the menu.
- (void)cancelMenu {
  [_backgroundConfigurationMediator cancelThemeSelection];
  [self.presentationDelegate cancelBackgroundPicker];
}

// Cancels the menu when the alert controller cancels.
- (void)alertControllerDidCancel {
  [self.presentationDelegate cancelBackgroundPicker];
}

@end
