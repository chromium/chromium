// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PRESET_GALLERY_PICKER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PRESET_GALLERY_PICKER_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/ui/home_customization_background_preset_gallery_picker_mutator.h"

namespace image_fetcher {
class ImageFetcherService;
}

@protocol HomeCustomizationBackgroundPresetGalleryPickerConsumer;

// A mediator that generates and configures background presets for the Home
// customization screen, and communicates them to a consumer.
@interface HomeCustomizationBackgroundPresetGalleryPickerMediator
    : NSObject <HomeCustomizationBackgroundPresetGalleryPickerMutator>

// The consumer that receives the background configurations.
@property(nonatomic, weak)
    id<HomeCustomizationBackgroundPresetGalleryPickerConsumer>
        consumer;

// Initializes a new instance of the background preset gallery picker mediator
// with the provided image fetcher service.
- (instancetype)initWithImageFetcherService:
    (image_fetcher::ImageFetcherService*)imageFetcherService;

// Provide a collection of background configurations to the consumer.
- (void)configureBackgroundConfigurations;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_PRESET_GALLERY_PICKER_MEDIATOR_H_
