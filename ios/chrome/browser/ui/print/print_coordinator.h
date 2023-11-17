// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRINT_PRINT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_PRINT_PRINT_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/web/model/print/web_state_printer.h"

class Browser;

// Interface for printing.
@interface PrintCoordinator : ChromeCoordinator <WebStatePrinter>

// `baseViewController` is the default VC to present print preview in case it
// is not specified in the command.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Shows print UI for `view` with `title`.
// Print preview will be presented on top of `baseViewController`.
- (void)printView:(UIView*)view
             withTitle:(NSString*)title
    baseViewController:(UIViewController*)baseViewController;

// Shows print UI for `image` with `title`.
// Print preview will be presented on top of `baseViewController`.
- (void)printImage:(UIImage*)image
                 title:(NSString*)title
    baseViewController:(UIViewController*)baseViewController;

// Dismisses the print dialog with animation if `animated`.
- (void)dismissAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_PRINT_PRINT_COORDINATOR_H_
