// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_MAGIC_STACK_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_MAGIC_STACK_MEDIATOR_H_

#import <UIKit/UIKit.h>

@protocol ContentSuggestionsConsumer;
@protocol ContentSuggestionsViewControllerAudience;
class IOSChromeSafetyCheckManager;
class PrefService;
@class SafetyCheckMagicStackMediator;
@class SafetyCheckState;
@class AppState;

// Handles Safety Check Module events.
@protocol SafetyCheckMagicStackMediatorDelegate

// Indicates to receiver that the Safety Check module should be removed.
- (void)removeSafetyCheckModule;

@end

// Mediator for managing the state of the Safety Check Magic Stack module
@interface SafetyCheckMagicStackMediator : NSObject

// Used by the Safety Check (Magic Stack) module for the current Safety Check
// state.
@property(nonatomic, strong, readonly) SafetyCheckState* safetyCheckState;

// Delegate.
@property(nonatomic, weak) id<SafetyCheckMagicStackMediatorDelegate> delegate;

// Audience for presentation actions.
@property(nonatomic, weak) id<ContentSuggestionsViewControllerAudience>
    presentationAudience;

// Consumer for this mediator.
@property(nonatomic, weak) id<ContentSuggestionsConsumer> consumer;

// Default initializer.
- (instancetype)initWithSafetyCheckManager:
                    (IOSChromeSafetyCheckManager*)safetyCheckManager
                                localState:(PrefService*)localState
                                 userState:(PrefService*)userState
                                  appState:(AppState*)appState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

// Disables and hides the Safety Check module in the Magic Stack.
- (void)disableModule;

// Resets the latest Safety Check State.
- (void)reset;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_MAGIC_STACK_MEDIATOR_H_
