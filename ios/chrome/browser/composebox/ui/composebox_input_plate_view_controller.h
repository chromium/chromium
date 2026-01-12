// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/ui/composebox_input_plate_consumer.h"

@protocol ComposeboxInputPlateMutator;
@protocol ComposeboxInputPlateViewControllerDelegate;
@class ComposeboxTheme;
@protocol TextFieldViewContaining;

/// View controller for the composebox composebox.
@interface ComposeboxInputPlateViewController
    : UIViewController <ComposeboxInputPlateConsumer>

@property(nonatomic, weak) id<ComposeboxInputPlateViewControllerDelegate>
    delegate;
@property(nonatomic, weak) id<ComposeboxInputPlateMutator> mutator;

/// Height of the input view.
@property(nonatomic, readonly) CGFloat inputHeight;
@property(nonatomic, readonly) CGFloat keyboardHeight;

// The input plate view to be used in animations.
@property(nonatomic, readonly) UIView* inputPlateViewForAnimation;

// Whether the UI is in compact (single line) mode.
@property(nonatomic, readonly, getter=isCompact) BOOL compact;

/// The button to toggle AI mode.
@property(nonatomic, strong) UIButton* aimButton;

/// The button to toggle Image Generation mode.
@property(nonatomic, strong) UIButton* imageGenerationButton;

// Initializes a new instance with a given theme.
- (instancetype)initWithTheme:(ComposeboxTheme*)theme NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

/// Sets the omnibox edit view.
- (void)setEditView:(UIView<TextFieldViewContaining>*)editView;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_VIEW_CONTROLLER_H_
