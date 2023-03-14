// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_QR_GENERATOR_QR_GENERATOR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SHARING_QR_GENERATOR_QR_GENERATOR_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/public/commands/qr_generation_commands.h"

#import <UIKit/UIKit.h>

@protocol ConfirmationAlertActionHandler;

// View controller that displays a QR code representing a given website.
@interface QRGeneratorViewController : UIViewController

// Initializes the view controller with the `title` to be displayed and the
// `pageURL`.
- (instancetype)initWithTitle:(NSString*)title pageURL:(NSURL*)pageURL;

// The action handler for interactions in this View Controller.
@property(nonatomic, weak) id<ConfirmationAlertActionHandler> actionHandler;

// Returns an image generated from the content of this view controller.
@property(nonatomic, readonly) UIImage* content;

// The button for the primary action.
@property(nonatomic, readonly) UIView* primaryActionButton;

// The help button item in the top left of the view.
@property(nonatomic, readonly) UIBarButtonItem* helpButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_SHARING_QR_GENERATOR_QR_GENERATOR_VIEW_CONTROLLER_H_
