// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_UI_STATE_MANAGER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_UI_STATE_MANAGER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_view_state_delegate.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"

// Declarative description of the container UI state.
struct GeminiContainerUIState {
  // Returns a zero-state configuration with the specified detent (defaults to
  // medium).
  static GeminiContainerUIState ZeroState(
      AssistantContainerDetent detent = AssistantContainerDetent::kMedium);

  // Returns an expanded response state (medium detent, grabber visible, zero
  // state hidden).
  static GeminiContainerUIState ExpandedResponse();

  // Returns a minimized state with or without a grabber.
  static GeminiContainerUIState Minimized(BOOL has_grabber);

  AssistantContainerDetent detent;
  BOOL hasGrabber;
  BOOL zeroStateVisible;
};

// Delegate protocol for handling UI state changes from the state manager.
@protocol GeminiContainerUIStateManagerDelegate <NSObject>

// Notifies the delegate that the container UI state has changed.
- (void)didChangeUIState:(GeminiContainerUIState)containerUIState;

@end

// Manages the state transitions and timing for the Gemini container UI.
@interface GeminiContainerUIStateManager : NSObject

// Delegate to receive UI state update notifications.
@property(nonatomic, weak) id<GeminiContainerUIStateManagerDelegate> delegate;

// Resets and applies the initial container UI state.
- (void)setupInitialUIState;

// Updates the view mode and notifies the delegate if UI state changes.
- (void)transitionToMode:(ios::provider::GeminiViewMode)mode;

// Updates the processing status, manages the thinking timer internally, and
// notifies the delegate if UI state changes.
- (void)transitionToProcessingStatus:
    (ios::provider::GeminiClientMode)processingStatus;

// Handles user tapping the new chat button and notifies the delegate with the
// new zero-state state preserving the current detent.
- (void)handleNewChat;

// Handles response cancellation and notifies the delegate with the expanded
// state if cancelled via the stop button.
- (void)handleResponseCancellationWithReason:(GeminiCancelType)reason;

// Updates the detent on the current state (e.g., when the user drags
// the sheet).
- (void)updateDetent:(AssistantContainerDetent)detent;

// Returns whether the container should be dismissed in its current state (i.e.
// when floaty is minimized in zero state with Chrome Next IA enabled).
- (BOOL)shouldBeDismissed;

// Disconnects and resets internal state and timer.
- (void)reset;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_UI_STATE_MANAGER_H_
