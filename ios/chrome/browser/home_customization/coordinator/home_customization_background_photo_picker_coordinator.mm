// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_photo_picker_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_photo_framing_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface HomeCustomizationBackgroundPhotoPickerCoordinator () <
    HomeCustomizationImageFramingViewControllerDelegate>
@end

@implementation HomeCustomizationBackgroundPhotoPickerCoordinator {
  // Strong reference to the framing view controller while it's being presented.
  HomeCustomizationImageFramingViewController* _framingViewController;
}

- (void)start {
  [self presentPhotoPicker];
}

- (void)stop {
  // Dismiss any presented picker if it's still showing.
  if (self.baseViewController.presentedViewController) {
    [self.baseViewController dismissViewControllerAnimated:NO completion:nil];
  }

  _framingViewController = nil;

  [super stop];
}

#pragma mark - Photo Library Picker

- (void)presentPhotoPicker {
  // Create a PHPickerConfiguration with appropriate filter options.
  PHPickerConfiguration* config = [[PHPickerConfiguration alloc] init];
  // Only allow selecting one image.
  config.selectionLimit = 1;
  config.filter = [PHPickerFilter imagesFilter];

  // Create the picker view controller.
  PHPickerViewController* pickerViewController =
      [[PHPickerViewController alloc] initWithConfiguration:config];
  pickerViewController.delegate = self;

  // Present the picker.
  [self.baseViewController presentViewController:pickerViewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - PHPickerViewControllerDelegate

- (void)picker:(PHPickerViewController*)picker
    didFinishPicking:(NSArray<PHPickerResult*>*)results {
  // Dismiss the picker.
  [picker dismissViewControllerAnimated:YES completion:nil];

  // Check if the user selected an image.
  if (results.count > 0) {
    PHPickerResult* result = results.firstObject;

    __weak __typeof(self) weakSelf = self;
    // Load the selected image.
    [result.itemProvider loadObjectOfClass:[UIImage class]
                         completionHandler:^(UIImage* image, NSError* error) {
                           if (image) {
                             CHECK(image);
                             dispatch_async(dispatch_get_main_queue(), ^{
                               [weakSelf handleSelectedImage:image];
                             });
                           }
                         }];
  } else {
    // User cancelled without selecting an image.
    [self.delegate photoPickerCoordinatorDidCancel:self];
  }
}

#pragma mark - Private

// Handles the selected image and presents the framing view.
- (void)handleSelectedImage:(UIImage*)image {
  // Create the logo vendor
  id<LogoVendor> logoVendor = ios::provider::CreateLogoVendor(
      self.browser, self.browser->GetWebStateList()->GetActiveWebState());

  // Create the framing view controller with both image and logo vendor
  _framingViewController = [[HomeCustomizationImageFramingViewController alloc]
      initWithImage:image
         logoVendor:logoVendor];

  // Set the delegate to handle the framed image.
  _framingViewController.delegate = self;

  _framingViewController.modalPresentationStyle =
      UIModalPresentationOverFullScreen;
  _framingViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;

  // Present the framing interface.
  [self.baseViewController presentViewController:_framingViewController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - HomeCustomizationImageFramingViewControllerDelegate

- (void)imageFramingViewController:
            (HomeCustomizationImageFramingViewController*)controller
                didFinishWithImage:(UIImage*)framedImage {
  // Dismiss the framing view controller.
  __weak __typeof(self) weakSelf = self;
  [controller dismissViewControllerAnimated:YES
                                 completion:^{
                                   // Pass the framed image to the delegate.
                                   [weakSelf.delegate
                                       photoPickerCoordinator:weakSelf
                                               didSelectImage:framedImage];
                                 }];

  _framingViewController = nil;
}

- (void)imageFramingViewControllerDidCancel:
    (HomeCustomizationImageFramingViewController*)controller {
  // Dismiss the framing view controller.
  __weak __typeof(self) weakSelf = self;
  [controller
      dismissViewControllerAnimated:YES
                         completion:^{
                           // Notify delegate of cancellation.
                           [weakSelf.delegate
                               photoPickerCoordinatorDidCancel:weakSelf];
                         }];

  _framingViewController = nil;
}

@end
