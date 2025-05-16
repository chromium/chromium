// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PRESET_GALLERY_PICKER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PRESET_GALLERY_PICKER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/ui/home_customization_background_preset_gallery_picker_consumer.h"

@protocol HomeCustomizationLogoVendorProvider;

@protocol HomeCustomizationBackgroundPresetGalleryPickerMutator;

// View controller for displaying a preset gallery of background images in the
// Home customization flow. Uses a collection view to showcase selectable preset
// backgrounds.
@interface HomeCustomizationBackgroundPresetGalleryPickerViewController
    : UIViewController <UICollectionViewDelegate,
                        HomeCustomizationBackgroundPresetGalleryPickerConsumer>

// A provider responsible for supplying a logo vendor object.
@property(nonatomic, weak) id<HomeCustomizationLogoVendorProvider>
    logoVendorProvider;

// Mutator to handle the user's customization updates.
@property(nonatomic, weak)
    id<HomeCustomizationBackgroundPresetGalleryPickerMutator>
        mutator;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PRESET_GALLERY_PICKER_VIEW_CONTROLLER_H_
