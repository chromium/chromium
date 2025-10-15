// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_CONFIGURATION_CONSUMER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_CONFIGURATION_CONSUMER_H_

#import <Foundation/Foundation.h>

@class BackgroundCollectionConfiguration;
@protocol BackgroundCustomizationConfiguration;

// A consumer protocol for receiving updates about background configurations.
@protocol HomeCustomizationBackgroundConfigurationConsumer

// Set the background collection configurations, including section data and
// the selected background identifier. This method also sets the data source
// with the appropriate configuration options for each section.
- (void)setBackgroundCollectionConfigurations:
            (NSArray<BackgroundCollectionConfiguration*>*)
                backgroundCollectionConfigurations
                         selectedBackgroundId:(NSString*)selectedBackgroundId;

// Alerts the consumer that the given configuration is now the current
// background.
- (void)currentBackgroundConfigurationChanged:
    (id<BackgroundCustomizationConfiguration>)currentConfiguration;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_CONFIGURATION_CONSUMER_H_
