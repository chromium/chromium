// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_BACKGROUND_CUSTOMIZATION_CONFIGURATION_ITEM_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_BACKGROUND_CUSTOMIZATION_CONFIGURATION_ITEM_H_

#import <UIKit/UIKit.h>

#import "components/themes/ntp_background_data.h"
#import "ios/chrome/browser/home_customization/model/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"

/**
 * A class representing a background customization configuration.
 * This class holds all the necessary data for a background choice.
 */
@interface BackgroundCustomizationConfigurationItem
    : NSObject <BackgroundCustomizationConfiguration>

// Initializes a new instance of the background customization configuration
// with the provided collection image.
- (instancetype)initWithCollectionImage:(const CollectionImage&)collectionImage;

// Initializes a new instance of the background customization configuration
// with the provided background color.
- (instancetype)initWithBackgroundColor:(UIColor*)backgroundColor;

// Initializes a new instance of the background customization configuration
/// with no background.
- (instancetype)initWithNoBackground;

// A pointer to a CollectionImage that points to the background image.
@property(readonly, nonatomic) const CollectionImage& collectionImage;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_BACKGROUND_CUSTOMIZATION_CONFIGURATION_ITEM_H_
