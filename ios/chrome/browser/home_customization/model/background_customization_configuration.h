// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_BACKGROUND_CUSTOMIZATION_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_BACKGROUND_CUSTOMIZATION_CONFIGURATION_H_

#import <UIKit/UIKit.h>

class GURL;

/**
 * A class representing a background customization configuration.
 * This class holds all the necessary data for a background choice.
 */
@interface BackgroundCustomizationConfiguration : NSObject

// A unique identifier for the background configuration.
@property(nonatomic, copy) NSString* configurationID;

// A pointer to a GURL that points to the low-resolution version (thumbnail)
// of the background image.
@property(nonatomic, assign) GURL& thumbnailURL;

// A pointer to a GURL that points to the high-resolution version of the
// background image.
@property(nonatomic, assign) GURL& highResURL;

// A pointer to a UIColor representing the background's base color.
@property(nonatomic, strong) UIColor* backgroundColor;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_BACKGROUND_CUSTOMIZATION_CONFIGURATION_H_
