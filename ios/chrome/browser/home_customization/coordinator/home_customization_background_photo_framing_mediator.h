// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_FRAMING_MEDIATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_FRAMING_MEDIATOR_H_

#import "ios/chrome/browser/home_customization/ui/home_customization_background_photo_framing_mutator.h"

class HomeBackgroundCustomizationService;
class UserUploadedImageManager;

// Implementation of the mediator.
@interface HomeCustomizationBackgroundPhotoFramingMediator
    : NSObject <HomeCustomizationBackgroundPhotoFramingMutator>

- (instancetype)initWithUserUploadedImageManager:
                    (UserUploadedImageManager*)userUploadedImageManager
                               backgroundService:
                                   (HomeBackgroundCustomizationService*)
                                       backgroundService;
- (instancetype)init NS_UNAVAILABLE;

// Discards customization changes.
- (void)discardBackground;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_FRAMING_MEDIATOR_H_
