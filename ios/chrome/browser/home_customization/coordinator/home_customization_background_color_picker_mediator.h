// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/ui/home_customization_background_color_picker_mutator.h"
@protocol HomeCustomizationBackgroundColorPickerConsumer;

// A mediator that generates and configures background color palettes
// for the Home customization screen, and communicates them to a consumer.
@interface HomeCustomizationBackgroundColorPickerMediator
    : NSObject <HomeCustomizationBackgroundColorPickerMutator>

// The consumer that receives the generated color palette configurations.
@property(nonatomic, weak) id<HomeCustomizationBackgroundColorPickerConsumer>
    consumer;

// Generates a predefined set of color palettes and provides them to the
// consumer.
- (void)configureColorPalettes;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_COORDINATOR_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_MEDIATOR_H_
