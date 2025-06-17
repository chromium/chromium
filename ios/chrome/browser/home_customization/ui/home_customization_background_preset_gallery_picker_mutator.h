// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PRESET_GALLERY_PICKER_MUTATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PRESET_GALLERY_PICKER_MUTATOR_H_

#import <UIKit/UIKit.h>

#import "url/gurl.h"

// A mutator protocol used to communicate with the
// `HomeCustomizationBackgroundPresetGalleryPickerMediator`.
@protocol HomeCustomizationBackgroundPresetGalleryPickerMutator

// Downloads and returns a thumbnail image from the given GURL. The image is
// returned asynchronously through the `completion` block. The method is
// intended to be used for background customization thumbnails, such as loading
// preview images for a collection view cell when it becomes visible.
- (void)fetchBackgroundCustomizationThumbnailURLImage:(GURL)thumbnailURL
                                           completion:
                                               (void (^)(UIImage*))completion;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PRESET_GALLERY_PICKER_MUTATOR_H_
