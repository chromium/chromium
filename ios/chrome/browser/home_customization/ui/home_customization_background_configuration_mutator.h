// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_CONFIGURATION_MUTATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_CONFIGURATION_MUTATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "url/gurl.h"

using UserUploadImageCompletion = void (^)(UIImage*, UserUploadedImageError);

@protocol BackgroundCustomizationConfiguration;

// Mutator protocol for the background customization views to make model
// updates.
@protocol HomeCustomizationBackgroundConfigurationMutator

// Downloads and returns a thumbnail image from the given GURL. The image is
// returned asynchronously through the `completion` block. An error is returned
// in the completion block if the image cannot be fetched. The method is
// intended to be used for background customization thumbnails, such as loading
// preview images for a collection view cell when it becomes visible.
- (void)fetchBackgroundCustomizationThumbnailURLImage:(GURL)thumbnailURL
                                           completion:(void (^)(UIImage* image,
                                                                NSError* error))
                                                          completion;

// Loads and returns (asynchronously via `completion`) the user-uploaded
// image at the given `imagePath`. The method is intended to be used for
// background customization thumbnails, such as loading preview images for a
// collection view cell when it becomes visible.
- (void)fetchBackgroundCustomizationUserUploadedImage:(NSString*)imagePath
                                           completion:
                                               (UserUploadImageCompletion)
                                                   completion;

// Applies the given background configuration to the NTP.
// This method updates the background based on the provided configuration.
- (void)applyBackgroundForConfiguration:
    (id<BackgroundCustomizationConfiguration>)backgroundConfiguration;

// Removes the given background configuration from the recently used list.
- (void)deleteBackgroundFromRecentlyUsed:
    (id<BackgroundCustomizationConfiguration>)backgroundConfiguration;

// Discards any unsaved customization changes and resets the background to the
// last one saved.
- (void)discardBackground;

// Commits any in-progress customization changes.
- (void)saveBackground;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_CONFIGURATION_MUTATOR_H_
