// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_PICKER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_PICKER_COORDINATOR_H_

#import <PhotosUI/PhotosUI.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator that handles photo selection from the device's photo library.
// This coordinator manages the PHPickerViewController and handles selected
// images.
@interface PHPickerCoordinator
    : ChromeCoordinator <PHPickerViewControllerDelegate>

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_PICKER_COORDINATOR_H_
