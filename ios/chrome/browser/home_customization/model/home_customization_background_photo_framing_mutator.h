// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_FRAMING_MUTATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_FRAMING_MUTATOR_H_

#import <UIKit/UIKit.h>

#import "base/values.h"

@class HomeCustomizationFramingCoordinates;

// Command for notifying when photo selection is finished.
typedef void (^PhotoSelectionFinishedCommand)(void);

// Mutator protocol for handling background image operations.
@protocol HomeCustomizationBackgroundPhotoFramingMutator <NSObject>

// Saves image with framing coordinates and notifies via command when complete.
// On success: command called with valid imagePath and framingData.
// On failure: command called with nil imagePath and empty framingData.
- (void)saveImageWithFramingCoordinates:(UIImage*)image
                            coordinates:(HomeCustomizationFramingCoordinates*)
                                            coordinates;

- (void)setCompletionCommand:(PhotoSelectionFinishedCommand)command;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_FRAMING_MUTATOR_H_
