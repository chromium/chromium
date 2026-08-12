// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_consumer.h"

@class GeminiContainerViewController;

// Delegate for the GeminiContainerViewController.
@protocol GeminiContainerViewControllerDelegate <NSObject>

// Called when the keyboard is shown.
- (void)geminiContainerViewController:
            (GeminiContainerViewController*)viewController
          didShowKeyboardWithDuration:(NSTimeInterval)duration
                                curve:(UIViewAnimationCurve)curve;

@end

// A view controller that acts as a container for Gemini features.
@interface GeminiContainerViewController
    : UIViewController <GeminiContainerConsumer>

// The delegate for this view controller.
@property(nonatomic, weak) id<GeminiContainerViewControllerDelegate> delegate;

// Initializes the container with the Gemini backend view controller.
- (instancetype)initWithGeminiViewController:
    (UIViewController*)geminiViewController NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONTAINER_VIEW_CONTROLLER_H_
