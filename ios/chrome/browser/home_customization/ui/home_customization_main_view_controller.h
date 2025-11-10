// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_MAIN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_MAIN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/ui/home_customization_background_configuration_consumer.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_main_consumer.h"

@class HomeCustomizationBackgroundCell;
@protocol BackgroundCustomizationConfiguration;
@protocol HomeCustomizationBackgroundConfigurationMutator;
@protocol HomeCustomizationBackgroundPickerPresentationDelegate;
@protocol HomeCustomizationDelegate;
@protocol HomeCustomizationMutator;
@protocol HomeCustomizationSearchEngineLogoMediatorProvider;
@protocol SnackbarCommands;

@class HomeCustomizationMainViewController;

@protocol HomeCustomizationMainViewControllerDelegate

// Alerts the delegate that the view controller's content height has changed.
- (void)viewContentHeightChangedInHomeCustomizationViewController:
    (HomeCustomizationMainViewController*)viewController;

@end

// Procedural block that will be used to handle the retry action in the
// snackbar.
typedef void (^ProceduralBlock)(void);

// The view controller representing the first page of the Home customization
// menu.
@interface HomeCustomizationMainViewController
    : UIViewController <HomeCustomizationBackgroundConfigurationConsumer,
                        HomeCustomizationMainConsumer>

// Mutator for communicating with the HomeCustomizationMediator.
@property(nonatomic, weak) id<HomeCustomizationMutator> mutator;

// Mutator for communicating background customization updates
@property(nonatomic, weak) id<HomeCustomizationBackgroundConfigurationMutator>
    customizationMutator;

// Delegate for communicating with the coordinator.
@property(nonatomic, weak) id<HomeCustomizationMainViewControllerDelegate>
    delegate;

// Delegate for background picker actions.
@property(nonatomic, weak)
    id<HomeCustomizationBackgroundPickerPresentationDelegate>
        backgroundPickerPresentationDelegate;

// A provider responsible for supplying a logo vendor object.
@property(nonatomic, weak) id<HomeCustomizationSearchEngineLogoMediatorProvider>
    searchEngineLogoMediatorProvider;

// The dispatcher for this view controller.
@property(nonatomic, weak) id<SnackbarCommands> snackbarCommandHandler;

// Whether the NTP custom background is disabled by enterprise policy.
@property(nonatomic, assign) BOOL customizationDisabledByPolicy;

// Whether interaction with the background customization section is enabled.
// Prevents the background from changing when it should not change.
@property(nonatomic, assign) BOOL backgroundCustomizationUserInteractionEnabled;

// Height of the content inside this view.
@property(nonatomic, readonly) CGFloat viewContentHeight;

// Fetches and sets the background image for a preset background cell, handling
// failures with a snackbar and retry mechanism.
- (void)fetchPresetImageForCell:(HomeCustomizationBackgroundCell*)cell
                  configuration:
                      (id<BackgroundCustomizationConfiguration>)configuration
                 itemIdentifier:(NSString*)itemIdentifier;

// Presents a snackbar indicating an image loading failure, with a retry action.
- (void)presentImageLoadFailSnackbarWithRetryBlock:(ProceduralBlock)retryBlock;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_MAIN_VIEW_CONTROLLER_H_
