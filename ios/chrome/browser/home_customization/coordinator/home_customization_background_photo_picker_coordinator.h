// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_PICKER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_PICKER_COORDINATOR_H_

#import <PhotosUI/PhotosUI.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class HomeCustomizationBackgroundPhotoPickerCoordinator;

// Protocol for handling selected images from the photo picker.
@protocol HomeCustomizationBackgroundPhotoPickerCoordinatorDelegate <NSObject>

// Called when the user successfully selects and frames an image.
- (void)photoPickerCoordinator:
            (HomeCustomizationBackgroundPhotoPickerCoordinator*)coordinator
                didSelectImage:(UIImage*)image;

// Called when the user cancels the photo selection process.
- (void)photoPickerCoordinatorDidCancel:
    (HomeCustomizationBackgroundPhotoPickerCoordinator*)coordinator;

@end

// Coordinator that handles photo selection from the device's photo library.
// This coordinator manages the PHPickerViewController and handles selected
// images.
@interface HomeCustomizationBackgroundPhotoPickerCoordinator
    : ChromeCoordinator <PHPickerViewControllerDelegate>

// Delegate to handle the final selected and framed image.
@property(nonatomic, weak)
    id<HomeCustomizationBackgroundPhotoPickerCoordinatorDelegate>
        delegate;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_PICKER_COORDINATOR_H_
