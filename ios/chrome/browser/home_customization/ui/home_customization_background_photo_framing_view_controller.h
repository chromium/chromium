// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_FRAMING_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_FRAMING_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol HomeCustomizationBackgroundPhotoFramingMutator;
@protocol HomeCustomizationSearchEngineLogoMediatorProvider;
@class HomeCustomizationImageFramingViewController;

// Protocol for handling framing results.
@protocol HomeCustomizationImageFramingViewControllerDelegate <NSObject>
// Called when the user cancels the framing operation.
- (void)imageFramingViewControllerDidCancel:
    (HomeCustomizationImageFramingViewController*)controller;
// Alerts the delegate that the framing operation succeeded.
- (void)imageFramingViewControllerDidSucceed:
    (HomeCustomizationImageFramingViewController*)controller;
@end

// View controller that provides a full-screen image framing interface.
@interface HomeCustomizationImageFramingViewController : UIViewController

// Delegate to receive framing results.
@property(nonatomic, weak)
    id<HomeCustomizationImageFramingViewControllerDelegate>
        delegate;
@property(nonatomic, weak) id<HomeCustomizationBackgroundPhotoFramingMutator>
    mutator;

// A provider responsible for supplying a logo vendor object.
// TODO(crbug.com/436228514): Need to remove this property.
@property(nonatomic, weak) id<HomeCustomizationSearchEngineLogoMediatorProvider>
    searchEngineLogoMediatorProvider;

// Initialize with an image to frame.
- (instancetype)initWithImage:(UIImage*)image NS_DESIGNATED_INITIALIZER;

// Unavailable initializers.
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PHOTO_FRAMING_VIEW_CONTROLLER_H_
