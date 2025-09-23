// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_BACKGROUND_CUSTOMIZATION_CONFIGURATION_ITEM_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_BACKGROUND_CUSTOMIZATION_CONFIGURATION_ITEM_H_

#import <UIKit/UIKit.h>

#import "components/themes/ntp_background_data.h"
#import "ios/chrome/browser/home_customization/ui/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ui/color/color_provider_key.h"

struct FramingCoordinates;

namespace sync_pb {
class NtpCustomBackground;
}

/**
 * A class representing a background customization configuration.
 * This class holds all the necessary data for a background choice.
 */
@interface BackgroundCustomizationConfigurationItem
    : NSObject <BackgroundCustomizationConfiguration>

// Initializes a new instance of the background customization configuration
// with the provided collection image.
- (instancetype)initWithCollectionImage:(const CollectionImage&)collectionImage
                      accessibilityName:(NSString*)accessibilityName
                     accessibilityValue:(NSString*)accessibilityValue;

// Initializes a new instance of the background customization configuration
// with the provided NtpCustomBackground, which is the sync/persistence data
// type for app-provided images.
- (instancetype)initWithNtpCustomBackground:
                    (const sync_pb::NtpCustomBackground&)customBackground
                          accessibilityName:(NSString*)accessibilityName;

// Initializes a new instance of the background customization configuration
// with the provided background color, its accessibility name and a variant.
- (instancetype)initWithBackgroundColor:(UIColor*)backgroundColor
                           colorVariant:
                               (ui::ColorProviderKey::SchemeVariant)colorVariant
                      accessibilityName:(NSString*)accessibilityName;

// Initializes a new instance of the background customization configuration
/// with no background.
- (instancetype)initWithNoBackground;

// Initializes a new instance of the background customization configuration
// with a user-uploaded image path, a framing coordinates and an accessibility
// name.
- (instancetype)initWithUserUploadedImagePath:(NSString*)imagePath
                           framingCoordinates:
                               (const FramingCoordinates&)coordinates
                            accessibilityName:(NSString*)accessibilityName;

// A pointer to a CollectionImage that points to the background image.
@property(readonly, nonatomic) const CollectionImage& collectionImage;

@property(readonly, nonatomic)
    const sync_pb::NtpCustomBackground& customBackground;

// The color scheme variant associated with the UIColor representing the
// background's base color.
@property(nonatomic, assign) ui::ColorProviderKey::SchemeVariant colorVariant;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_BACKGROUND_CUSTOMIZATION_CONFIGURATION_ITEM_H_
