// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_PRINT_PRINT_HANDLER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_PRINT_PRINT_HANDLER_H_

#import <UIKit/UIKit.h>

// Protocol implemented to print a view or an image. Used to implement the
// javascript command "window.print".
@protocol PrintHandler <NSObject>

// Shows print UI for `view` with `title`.
// Print preview will be presented on top of the view controller that manages
// PrintHandler.
- (void)printView:(UIView*)view withTitle:(NSString*)title;

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

@end

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_PRINT_PRINT_HANDLER_H_
