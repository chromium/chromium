// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PRESET_GALLERY_PICKER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PRESET_GALLERY_PICKER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/ui/home_customization_background_configuration_consumer.h"

@protocol HomeCustomizationBackgroundPickerPresentationDelegate;
@protocol HomeCustomizationSearchEngineLogoMediatorProvider;
@protocol HomeCustomizationBackgroundConfigurationMutator;

// View controller for displaying a preset gallery of background images in the
// Home customization flow. Uses a collection view to showcase selectable preset
// backgrounds.
@interface HomeCustomizationBackgroundPresetGalleryPickerViewController
    : UIViewController <HomeCustomizationBackgroundConfigurationConsumer,
                        UICollectionViewDelegate>

// A provider responsible for supplying a logo vendor object.
// TODO(crbug.com/436228514): Need to remove this property.
@property(nonatomic, weak) id<HomeCustomizationSearchEngineLogoMediatorProvider>
    searchEngineLogoMediatorProvider;

// Mutator to handle model interactions.
@property(nonatomic, weak) id<HomeCustomizationBackgroundConfigurationMutator>
    mutator;

// Presentation delegate for background picker UI.
@property(nonatomic, weak)
    id<HomeCustomizationBackgroundPickerPresentationDelegate>
        presentationDelegate;

// The index of the selected section in the gallery.
@property(nonatomic, readonly) NSInteger selectedSectionIndex;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PRESET_GALLERY_PICKER_VIEW_CONTROLLER_H_
