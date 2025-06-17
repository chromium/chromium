// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_BACKGROUND_CUSTOMIZATION_INFORMATION_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_BACKGROUND_CUSTOMIZATION_INFORMATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"

class GURL;

/**
 * A protocol representing a background customization information.
 * This protocol holds all the necessary data for displaying in the background
 * customization gallery.
 */
@protocol BackgroundCustomizationConfiguration <NSObject>

// A unique identifier for the background configuration.
@property(readonly, nonatomic, copy) NSString* configurationID;

// The type of background customization picker used to create the configuration.
@property(readonly, nonatomic) HomeCustomizationBackgroundStyle backgroundType;

// A pointer to a GURL that points to the low-resolution version (thumbnail)
// of the background image.
@property(readonly, nonatomic) const GURL& thumbnailURL;

// A pointer to a UIColor representing the background's base color.
@property(readonly, nonatomic, strong) UIColor* backgroundColor;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_BACKGROUND_CUSTOMIZATION_INFORMATION_H_
