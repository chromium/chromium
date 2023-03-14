// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class WebOmniboxEditModelDelegate;
@protocol EditViewAnimatee;
@class OmniboxPopupCoordinator;
@class OmniboxTextFieldIOS;
@protocol LocationBarOffsetProvider;
@protocol OmniboxPopupPresenterDelegate;

// The coordinator for the omnibox.
@interface OmniboxCoordinator : ChromeCoordinator

// Returns a popup coordinator created by this coordinator.
// Created and started at `start` and stopped & destroyed at `stop`.
@property(nonatomic, strong, readonly)
    OmniboxPopupCoordinator* popupCoordinator;
// The edit controller interfacing the `textField` and the omnibox components
// code. Needs to be set before the coordinator is started.
@property(nonatomic, assign) WebOmniboxEditModelDelegate* editModelDelegate;
// Returns the animatee for the omnibox focus orchestrator.
@property(nonatomic, strong, readonly) id<EditViewAnimatee> animatee;

/// Positioner for the popup. Has to be configured before calling `start`.
@property(nonatomic, weak) id<OmniboxPopupPresenterDelegate> presenterDelegate;

// The view controller managed by this coordinator. The parent of this
// coordinator is expected to add it to the responder chain.
- (UIViewController*)managedViewController;

// Offset provider for location bar animations.
- (id<LocationBarOffsetProvider>)offsetProvider;

// Start this coordinator. When it starts, it expects to have `textField` and
// `editModelDelegate`.
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

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_COORDINATOR_H_
