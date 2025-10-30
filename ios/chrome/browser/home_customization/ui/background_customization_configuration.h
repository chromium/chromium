// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_BACKGROUND_CUSTOMIZATION_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_BACKGROUND_CUSTOMIZATION_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ui/color/color_provider_key.h"

class GURL;
@class HomeCustomizationFramingCoordinates;
@class NewTabPageColorPalette;

/**
 * A protocol representing a background customization information.
 * This protocol holds all the necessary data for displaying in the background
 * customization gallery.
 */
@protocol BackgroundCustomizationConfiguration <NSObject>

// A unique identifier for the background configuration.
@property(readonly, nonatomic, copy) NSString* configurationID;

// The style of background customization picker used to create the
// configuration.
@property(readonly, nonatomic) HomeCustomizationBackgroundStyle backgroundStyle;

// A pointer to a GURL that points to the low-resolution version (thumbnail)
// of the background image.
@property(readonly, nonatomic) const GURL& thumbnailURL;

// A pointer to a UIColor representing the background's base color.
@property(nonatomic, strong) UIColor* backgroundColor;

// The color variant for the background
@property(readonly, nonatomic) ui::ColorProviderKey::SchemeVariant colorVariant;

// Whether the background color was manually chosen by the user.
@property(nonatomic, assign) BOOL isCustomColor;

// The color palette for this background.
@property(readonly, nonatomic) NewTabPageColorPalette* colorPalette;

// The file path to the user-uploaded background image.
@property(readonly, nonatomic) NSString* userUploadedImagePath;

// The framing coordinates for how the user-uploaded image should be displayed.
@property(readonly, nonatomic)
    HomeCustomizationFramingCoordinates* userUploadedFramingCoordinates;

// The localized accessibility name associated with the configuration.
@property(nonatomic, copy) NSString* accessibilityName;

// The localized accessibility value associated with the configuration.
@property(nonatomic, copy) NSString* accessibilityValue;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_BACKGROUND_CUSTOMIZATION_CONFIGURATION_H_
