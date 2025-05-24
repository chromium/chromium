// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/coordinator/home_customization_background_photo_picker_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_photo_library_picker_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation PHPickerCoordinator

- (void)start {
  [self presentPhotoPicker];
}

- (void)stop {
  // Dismiss any presented picker if it's still showing.
  if (self.baseViewController.presentedViewController) {
    [self.baseViewController dismissViewControllerAnimated:NO completion:nil];
  }
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
  }
}

#pragma mark - Private

// Handles the selected image and presents the cropping view.
- (void)handleSelectedImage:(UIImage*)image {
  // TODO(crbug.com/415060566): Present cropping interface.
}

@end
