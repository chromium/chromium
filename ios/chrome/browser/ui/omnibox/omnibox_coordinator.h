// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_COORDINATOR_H_

#import <memory>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class OmniboxClient;
@protocol EditViewAnimatee;
@class OmniboxPopupCoordinator;
@protocol LocationBarOffsetProvider;
@protocol OmniboxPopupPresenterDelegate;
@protocol TextFieldViewContaining;
@protocol ToolbarOmniboxConsumer;
@protocol OmniboxFocusDelegate;

// The coordinator for the omnibox.
@interface OmniboxCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                             omniboxClient:
                                 (std::unique_ptr<OmniboxClient>)client
                             isLensOverlay:(BOOL)isLensOverlay
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Returns a popup coordinator created by this coordinator.
// Created and started at `start` and stopped & destroyed at `stop`.
@property(nonatomic, strong, readonly)
    OmniboxPopupCoordinator* popupCoordinator;
// Returns the animatee for the omnibox focus orchestrator.
@property(nonatomic, strong, readonly) id<EditViewAnimatee> animatee;

/// Positioner for the popup. Has to be configured before calling `start`.
@property(nonatomic, weak) id<OmniboxPopupPresenterDelegate> presenterDelegate;

/// Delegate for responding to focusing events.
@property(nonatomic, weak) id<OmniboxFocusDelegate> focusDelegate;

/// The edit view, which contains a text field.
@property(nonatomic, readonly) UIView<TextFieldViewContaining>* editView;

/// Controls the UI configuration of the omnibox to reflect search-only mode.
/// Actual navigation limitations are managed by the `OmniboxClient`. Has to be
/// configured before calling `start`. Defaults to `NO`.
@property(nonatomic, assign) BOOL isSearchOnlyUI;

// The view controller managed by this coordinator. The parent of this
// coordinator is expected to add it to the responder chain.
- (UIViewController*)managedViewController;

// Offset provider for location bar animations.
- (id<LocationBarOffsetProvider>)offsetProvider;

// Start this coordinator. When it starts, it expects to have `textField` and
// `locationBar`.
- (void)start;
// Stop this coordinator.
- (void)stop;

// Indicates if the omnibox is the first responder.
- (BOOL)isOmniboxFirstResponder;
// Inserts text to the omnibox without triggering autocomplete.
// Use this method to insert target URL or search terms for alternative input
// methods, such as QR code scanner or voice search.
- (void)insertTextToOmnibox:(NSString*)string;
// Update the contents and the styling of the omnibox.
- (void)updateOmniboxState;
// Use this method to make the omnibox first responder.
- (void)focusOmnibox;

// Prepare the omnibox for scribbling.
- (void)focusOmniboxForScribble;
// Target input for scribble targeting the omnibox.
- (UIResponder<UITextInput>*)scribbleInput;

// Use this method to resign `textField` as the first responder.
- (void)endEditing;

/// Sets the thumbnail image used for image search. Set to`nil` to hide the
/// thumbnail.
- (void)setThumbnailImage:(UIImage*)image;

// Returns the toolbar omnibox consumer.
- (id<ToolbarOmniboxConsumer>)toolbarOmniboxConsumer;
@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_COORDINATOR_H_
