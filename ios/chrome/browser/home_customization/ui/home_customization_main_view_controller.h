// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_MAIN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_MAIN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/ui/home_customization_main_consumer.h"

@protocol HomeCustomizationDelegate;
@protocol HomeCustomizationMutator;
@protocol HomeCustomizationBackgroundPickerPresentationDelegate;
@protocol HomeCustomizationLogoVendorProvider;

// The view controller representing the first page of the Home customization
// menu.
@interface HomeCustomizationMainViewController
    : UIViewController <HomeCustomizationMainConsumer>

// Mutator for communicating with the HomeCustomizationMediator.
@property(nonatomic, weak) id<HomeCustomizationMutator> mutator;

// Delegate for communicating with the coordinator.
@property(nonatomic, weak) id<HomeCustomizationDelegate> delegate;

// Delegate for background picker actions.
@property(nonatomic, weak)
    id<HomeCustomizationBackgroundPickerPresentationDelegate>
        backgroundPickerPresentationDelegate;

// A provider responsible for supplying a logo vendor object.
@property(nonatomic, weak) id<HomeCustomizationLogoVendorProvider>
    logoVendorProvider;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_MAIN_VIEW_CONTROLLER_H_
