// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_FRAMING_MEDIATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_FRAMING_MEDIATOR_H_

#import "base/files/file_path.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/home_customization/model/home_customization_background_photo_framing_mutator.h"

class HomeBackgroundCustomizationService;

@protocol HomeCustomizationBackgroundPhotoFramingMutator;

// Implementation of the mediator.
@interface HomeCustomizationBackgroundPhotoFramingMediator
    : NSObject <HomeCustomizationBackgroundPhotoFramingMutator>

// Initialize with file path for profile-specific storage.
- (instancetype)initWithFilePath:(const base::FilePath&)filePath
               backgroundService:
                   (HomeBackgroundCustomizationService*)backgroundService;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_FRAMING_MEDIATOR_H_
