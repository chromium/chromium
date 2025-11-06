// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/ui/composebox_animation_context_provider.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_consumer.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_mutator.h"

@protocol ComposeboxInputPlateMutator;
@class ComposeboxInputPlateViewController;
@protocol TextFieldViewContaining;

/// Delegate for the composebox composebox view controller.
@protocol ComposeboxInputPlateViewControllerDelegate
/// Informs the delegate that a user did tap on the gallery button.
- (void)composeboxViewControllerDidTapGalleryButton:
    (ComposeboxInputPlateViewController*)composeboxViewController;
/// Informs the delegate that a user did tap on the mic button.
- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)composeboxViewController
                 didTapMicButton:(UIButton*)button;
/// Informs the delegate that a user did tap on the lens button.
- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)composeboxViewController
                didTapLensButton:(UIButton*)button;
/// Informs the delegate that a user did tap on the camera button.
- (void)composeboxViewControllerDidTapCameraButton:
    (ComposeboxInputPlateViewController*)composeboxViewController;
/// Informs the delegate that a user did tap on the gallery button.
- (void)composeboxViewControllerMayShowGalleryPicker:
    (ComposeboxInputPlateViewController*)composeboxViewController;
/// Informs the delegate that a user did tap on the file button.
- (void)composeboxViewControllerDidTapFileButton:
    (ComposeboxInputPlateViewController*)composeboxViewController;
/// Informs the delegate that a user did tap on the attach tabs button.
- (void)composeboxViewControllerDidTapAttachTabsButton:
    (ComposeboxInputPlateViewController*)composeboxViewController;
/// Informs the delegate that a user did tap on the lens button.
- (void)composeboxViewController:
            (ComposeboxInputPlateViewController*)composeboxViewController
                didTapSendButton:(UIButton*)button;
@end

/// View controller for the composebox composebox.
@interface ComposeboxInputPlateViewController
    : UIViewController <ComposeboxAnimationContextProvider,
                        ComposeboxInputPlateConsumer>

@property(nonatomic, weak) id<ComposeboxInputPlateViewControllerDelegate>
    delegate;
@property(nonatomic, weak) id<ComposeboxInputPlateMutator> mutator;

/// Height of the input view.
@property(nonatomic, readonly) CGFloat inputHeight;

/// Sets the omnibox edit view.
- (void)setEditView:(UIView<TextFieldViewContaining>*)editView;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_VIEW_CONTROLLER_H_
