// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TEST_FAKE_ACTOR_TASK_INTERVENTION_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TEST_FAKE_ACTOR_TASK_INTERVENTION_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_intervention_delegate.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

@class ActorFormSuggestion;

// A fake implementation of `ActorTaskInterventionDelegate` for unit and UI
// tests. Supports automatic responses or recording requests for manual
// resolution in tests.
@interface FakeActorTaskInterventionDelegate
    : NSObject <ActorTaskInterventionDelegate>

// When set to YES, automatically invokes the callback immediately upon
// receiving a user intervention request. Defaults to NO.
@property(nonatomic, assign) BOOL autoConfirmInterventions;

// When non-nil, automatically invokes the callback with this answer immediately
// upon receiving a clarification request. Defaults to nil.
@property(nonatomic, copy) NSString* autoClarificationAnswer;

// When set to YES, automatically invokes the callback with the first
// suggestion and `shouldStorePermission = NO`. Defaults to NO.
@property(nonatomic, assign) BOOL autoSelectFirstSuggestion;

// A callback invoked synchronously when the actor task requests user
// intervention.
@property(nonatomic, copy) ProceduralBlock onUserInterventionRequested;

// A callback invoked synchronously right after the user intervention callback
// is executed.
@property(nonatomic, copy) ProceduralBlock onUserInterventionCompleted;

// A callback invoked synchronously right after the clarification callback is
// executed.
@property(nonatomic, copy) ProceduralBlock onClarificationCompleted;

// Whether `requestUserInterventionWithTitle:...` was called.
@property(nonatomic, readonly) BOOL requestConfirmationCalled;

// The title, subtitle and button text received in the last user intervention
// request.
@property(nonatomic, readonly) NSString* confirmationTitle;
@property(nonatomic, readonly) NSString* confirmationSubtitle;
@property(nonatomic, readonly) NSString* confirmationButtonText;

// Whether an unhandled user intervention callback is currently pending.
@property(nonatomic, readonly) BOOL hasPendingUserIntervention;

// Whether `selectFromSuggestions:...` was called.
@property(nonatomic, readonly) BOOL selectFromSuggestionsCalled;

// The suggestions array received in the last `selectFromSuggestions:...` call.
@property(nonatomic, readonly)
    NSArray<ActorFormSuggestion*>* promptedSuggestions;

// Whether an unhandled suggestion selection callback is currently pending.
@property(nonatomic, readonly) BOOL hasPendingSuggestionSelection;

// Whether `actorTaskDidCancelSuggestionSelection:` was called.
@property(nonatomic, readonly) BOOL cancelSuggestionSelectionCalled;

// Whether `requestNavigationConfirmationForDomain:...` was called.
@property(nonatomic, readonly) BOOL requestNavigationConfirmationCalled;

// The domain received in the last `requestNavigationConfirmationForDomain:...`
// call.
@property(nonatomic, readonly) NSString* promptedNavigationDomain;

// Whether an unhandled navigation confirmation callback is currently pending.
@property(nonatomic, readonly) BOOL hasPendingNavigationConfirmation;

// Invokes the stored navigation confirmation callback with `confirmed` and
// clears it.
- (void)runNavigationConfirmationCompletion:(BOOL)confirmed;

// Invokes the stored navigation confirmation callback with `error` and clears
// it.
- (void)failNavigationConfirmationWithError:
    (actor::InterventionDelegateError)error;

// The message received in the last
// `actorTask:requestClarificationWithMessage:callback:` call, or nil if it was
// never called.
@property(nonatomic, readonly) NSString* clarificationMessage;

// Whether an unhandled clarification callback is currently pending.
@property(nonatomic, readonly) BOOL hasPendingClarification;

// Resets `requestConfirmationCalled` to NO.
- (void)resetRequestConfirmationCalled;

// Invokes the stored user intervention callback and clears it.
- (void)runUserInterventionCompletion;

// Invokes the stored user intervention callback with `error` and clears it.
- (void)failUserInterventionWithError:(actor::InterventionDelegateError)error;

// Invokes the stored clarification callback with `answer` and clears it.
- (void)runClarificationCompletionWithAnswer:(NSString*)answer;

// Invokes the stored clarification callback with `error` and clears it.
- (void)failClarificationWithError:(actor::InterventionDelegateError)error;

// Invokes the stored suggestion callback with `selectedSuggestion` and
// `shouldStorePermission`. Clears the stored callback.
- (void)runCompletionWithSuggestion:(ActorFormSuggestion*)selectedSuggestion
              shouldStorePermission:(BOOL)shouldStorePermission;
// Convenience overload with `shouldStorePermission = NO`.
- (void)runCompletionWithSuggestion:(ActorFormSuggestion*)selectedSuggestion;

// Invokes the stored suggestion callback with `error` and clears it.
- (void)failSuggestionSelectionWithError:
    (actor::InterventionDelegateError)error;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TEST_FAKE_ACTOR_TASK_INTERVENTION_DELEGATE_H_
