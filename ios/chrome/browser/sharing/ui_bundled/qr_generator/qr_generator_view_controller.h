// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_QR_GENERATOR_QR_GENERATOR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_QR_GENERATOR_QR_GENERATOR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/public/commands/qr_generation_commands.h"

@class QRGeneratorViewController;

// Delegate protocol for the QR generator.
@protocol QRGeneratorViewControllerDelegate

// Called when the user tapped on the dismiss button of the QR Generator.
- (void)QRGeneratorViewControllerDidTapDismiss:
    (QRGeneratorViewController*)generator;

// Called when the user tapped on the confirm button of the QR Generator.
- (void)QRGeneratorViewControllerDidTapConfirm:
    (QRGeneratorViewController*)generator;

// Called when the user tapped on the learn more button of the QR Generator.
- (void)QRGeneratorViewControllerDidTapLearnMore:
    (QRGeneratorViewController*)generator;

@end

// View controller that displays a QR code representing a given website.
@interface QRGeneratorViewController : UIViewController

// Initializes the view controller with the `title` to be displayed and the
// `pageURL`.
- (instancetype)initWithTitle:(NSString*)title pageURL:(NSURL*)pageURL;

// The action handler for interactions in this View Controller.
@property(nonatomic, weak) id<QRGeneratorViewControllerDelegate> delegate;

// Returns an image generated from the content of this view controller.
@property(nonatomic, readonly) UIImage* content;

// The button for the primary action.
@property(nonatomic, readonly) UIView* primaryActionButton;

// The help button item in the top left of the view.
@property(nonatomic, readonly) UIBarButtonItem* helpButton;

@end

#endif  // IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_QR_GENERATOR_QR_GENERATOR_VIEW_CONTROLLER_H_
