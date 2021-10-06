// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRINT_PRINT_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PRINT_PRINT_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/web/print/web_state_printer.h"

// The delegate for PrintController. Provide information needed to display the
// UIPrintInteractionController.
@protocol PrintControllerDelegate
// The view controller the system print dialog should be presented from.
- (UIViewController*)baseViewControllerForPrintPreview;
@end

// Interface for printing.
@interface PrintController : NSObject <WebStatePrinter>

// Set this before calling any print methods.
@property(nonatomic, weak) id<PrintControllerDelegate> delegate;

// Shows print UI for |view| with |title|.
- (void)printView:(UIView*)view withTitle:(NSString*)title;

// Shows print UI for |image| with |title|.
- (void)printImage:(UIImage*)image title:(NSString*)title;

// Dismisses the print dialog with animation if |animated|.
- (void)dismissAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_PRINT_PRINT_CONTROLLER_H_
