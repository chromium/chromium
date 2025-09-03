// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_FRAMING_MUTATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_FRAMING_MUTATOR_H_

#import <UIKit/UIKit.h>

#import "base/functional/callback_forward.h"
#import "base/values.h"

@class HomeCustomizationFramingCoordinates;

// Mutator protocol for handling background image operations.
@protocol HomeCustomizationBackgroundPhotoFramingMutator <NSObject>

// Saves image with the given framing coordinates.
- (void)saveImage:(UIImage*)image
    withFramingCoordinates:(HomeCustomizationFramingCoordinates*)coordinates
                completion:(base::OnceCallback<void(BOOL success)>)completion;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_FRAMING_MUTATOR_H_
