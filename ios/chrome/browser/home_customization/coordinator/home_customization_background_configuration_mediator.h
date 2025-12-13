// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_CONFIGURATION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_CONFIGURATION_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/ui/home_customization_background_configuration_mutator.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"

namespace image_fetcher {
class ImageFetcher;
}

class HomeBackgroundImageService;
class HomeBackgroundCustomizationService;
@protocol HomeCustomizationBackgroundConfigurationConsumer;
@protocol HomeCustomizationBackgroundPickerPresentationDelegate;
class UserUploadedImageManager;

// A mediator that generates and configures background options for the
// customization screens, and communicates them to a consumer.
@interface HomeCustomizationBackgroundConfigurationMediator
    : NSObject <HomeCustomizationBackgroundConfigurationMutator>

// Initializes a new instance of the background configuration mediator with the
// provided services. `backgroundCustomizationService` and `imageFetcher` are
// required. `homeBackgroundImageService` can be null if loading gallery images
// is not required. `userUploadedImageManager` can be null if this mediator will
// not have to deal with user uploaded images.
- (instancetype)
    initWithBackgroundCustomizationService:
        (HomeBackgroundCustomizationService*)backgroundCustomizationService
                              imageFetcher:
                                  (image_fetcher::ImageFetcher*)imageFetcher
                homeBackgroundImageService:
                    (HomeBackgroundImageService*)homeBackgroundImageService
                  userUploadedImageManager:
                      (UserUploadedImageManager*)userUploadedImageManager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Presentation delegate for the background picker UI.
@property(nonatomic, weak)
    id<HomeCustomizationBackgroundPickerPresentationDelegate>
        delegate;

// The consumer that receives the background configurations.
@property(nonatomic, weak) id<HomeCustomizationBackgroundConfigurationConsumer>
    consumer;

// Whether this mediator has changed the theme.
@property(nonatomic, readonly) BOOL themeHasChanged;

// The outcome of the user's background selection flow.
@property(nonatomic, readwrite, assign)
    BackgroundSelectionOutcome backgroundSelectionOutcome;

// Provide a gallery of preset background configurations to the consumer.
- (void)loadGalleryBackgroundConfigurations;

// Provide the recently used background configurations to the consumer.
- (void)loadRecentlyUsedBackgroundConfigurations;

// Provide the color background configurations to the consumer.
- (void)loadColorBackgroundConfigurations;

// Saves the currently selected theme data, if it has been changed by this
// mediator.
- (void)saveCurrentTheme;

// Does any necessary clean up if the selection is cancelled.
- (void)cancelThemeSelection;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_CONFIGURATION_MEDIATOR_H_
