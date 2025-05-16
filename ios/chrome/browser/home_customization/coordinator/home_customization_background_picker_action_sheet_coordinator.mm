// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_picker_action_sheet_coordinator.h"

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_color_picker_mediator.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_preset_gallery_picker_mediator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_color_picker_view_controller.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_photo_library_picker_view_controller.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_preset_gallery_picker_view_controller.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_logo_vendor_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface HomeCustomizationBackgroundPickerActionSheetCoordinator () <
    HomeCustomizationLogoVendorProvider> {
  // The mediator for the color picker.
  HomeCustomizationBackgroundColorPickerMediator*
      _backgroundColorPickerMediator;

  // The mediator for the background preset gallery picker.
  HomeCustomizationBackgroundPresetGalleryPickerMediator*
      _backgroundPresetGalleryPickerMediator;
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
  _backgroundColorPickerMediator =
      [[HomeCustomizationBackgroundColorPickerMediator alloc] init];

  _backgroundPresetGalleryPickerMediator =
      [[HomeCustomizationBackgroundPresetGalleryPickerMediator alloc] init];

  [self
      addItemWithTitle:
          l10n_util::GetNSStringWithFixup(
              IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_PICKER_PRESET_GALLERY_TITLE)
                action:^{
                  [weakSelf presentPresetGalleryPicker];
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
                    [weakSelf presentBackgroundColorPicker];
                  }
                   style:UIAlertActionStyleDefault];

  [super start];
}

- (void)stop {
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  _backgroundColorPickerMediator = nil;
  _backgroundPresetGalleryPickerMediator = nil;
  [super stop];
}

#pragma mark - HomeCustomizationLogoVendorProvider

- (id<LogoVendor>)provideLogoVendor {
  return ios::provider::CreateLogoVendor(
      self.browser, self.browser->GetWebStateList()->GetActiveWebState());
}

#pragma mark - Private functions

// Presents the view controller for picking a solid background color.
- (void)presentBackgroundColorPicker {
  HomeCustomizationBackgroundColorPickerViewController* mainViewController =
      [[HomeCustomizationBackgroundColorPickerViewController alloc] init];

  mainViewController.mutator = _backgroundColorPickerMediator;
  _backgroundColorPickerMediator.consumer = mainViewController;
  [_backgroundColorPickerMediator configureColorPalettes];

  mainViewController.modalPresentationStyle = UIModalPresentationFormSheet;
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:mainViewController];

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

// Presents the view controller for picking a background image
// from a preselected gallery collection.
- (void)presentPresetGalleryPicker {
  HomeCustomizationBackgroundPresetGalleryPickerViewController*
      mainViewController =
          [[HomeCustomizationBackgroundPresetGalleryPickerViewController alloc]
              init];
  mainViewController.logoVendorProvider = self;
  mainViewController.mutator = _backgroundPresetGalleryPickerMediator;
  _backgroundPresetGalleryPickerMediator.consumer = mainViewController;
  [_backgroundPresetGalleryPickerMediator configureBackgroundConfigurations];

  mainViewController.modalPresentationStyle = UIModalPresentationFormSheet;
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:mainViewController];

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

// Presents the view controller for selecting a background photo
// from the device's photo library.
- (void)presentPhotoLibraryPicker {
  UIViewController* mainViewController =
      [[HomeCustomizationBackgroundPhotoLibraryPickerViewController alloc]
          init];
  mainViewController.modalPresentationStyle = UIModalPresentationFormSheet;
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:mainViewController];

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

@end
