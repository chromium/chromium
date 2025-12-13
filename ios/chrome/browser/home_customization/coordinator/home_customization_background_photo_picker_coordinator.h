// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_PICKER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_PICKER_COORDINATOR_H_

#import <PhotosUI/PhotosUI.h>
#import <UIKit/UIKit.h>

#import "base/values.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol HomeCustomizationSearchEngineLogoMediatorProvider;
@class HomeCustomizationBackgroundPhotoFramingMediator;
@class HomeCustomizationBackgroundPhotoPickerCoordinator;

// Protocol for handling selected images from the photo picker.
@protocol HomeCustomizationBackgroundPhotoPickerCoordinatorDelegate <NSObject>

// Called when the user cancels the photo selection process.
- (void)photoPickerCoordinatorDidCancel:
    (HomeCustomizationBackgroundPhotoPickerCoordinator*)coordinator;

// Called when the user successfully saves a framed image.
- (void)photoPickerCoordinatorDidFinish:
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

// A provider responsible for supplying a logo vendor object.
@property(nonatomic, weak) id<HomeCustomizationSearchEngineLogoMediatorProvider>
    searchEngineLogoMediatorProvider;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_PICKER_COORDINATOR_H_
