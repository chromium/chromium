// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_MEDIATOR_H_

#import <UIKit/UIKit.h>

@protocol HomeCustomizationBackgroundColorPickerConsumer;
class HomeBackgroundCustomizationService;

// A mediator that generates and configures background color palettes
// for the Home customization screen, and communicates them to a consumer.
@interface HomeCustomizationBackgroundColorPickerMediator : NSObject

// The consumer that receives the generated color palette configurations.
@property(nonatomic, weak) id<HomeCustomizationBackgroundColorPickerConsumer>
    consumer;

- (instancetype)initWithBackgroundCustomizationService:
    (HomeBackgroundCustomizationService*)backgroundCustomizationService;

// Generates a predefined set of color palettes and provides them to the
// consumer.
- (void)configureBackgroundConfigurations;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_MEDIATOR_H_
