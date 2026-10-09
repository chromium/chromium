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

// When set to YES, automatically invokes the completion handler immediately
// upon receiving a user intervention request. Defaults to NO.
@property(nonatomic, assign) BOOL autoConfirmInterventions;

// When set to YES, automatically invokes the completion handler with the first
// suggestion and `shouldStorePermission = NO`. Defaults to NO.
@property(nonatomic, assign) BOOL autoSelectFirstSuggestion;

// A callback invoked synchronously when the actor task requests user
// intervention.
@property(nonatomic, copy) ProceduralBlock onUserInterventionRequested;

// A callback invoked synchronously right after the user intervention completion
// handler is executed.
@property(nonatomic, copy) ProceduralBlock onUserInterventionCompleted;

// Whether `requestUserInterventionWithTitle:...` was called.
@property(nonatomic, readonly) BOOL requestConfirmationCalled;

// The title, subtitle and button text received in the last user intervention
// request.
@property(nonatomic, readonly) NSString* confirmationTitle;
@property(nonatomic, readonly) NSString* confirmationSubtitle;
@property(nonatomic, readonly) NSString* confirmationButtonText;

// Whether an unhandled user intervention completion handler is currently
// pending.
@property(nonatomic, readonly) BOOL hasPendingUserIntervention;

// Whether `selectFromSuggestions:...` was called.
@property(nonatomic, readonly) BOOL selectFromSuggestionsCalled;

// The suggestions array received in the last `selectFromSuggestions:...` call.
@property(nonatomic, readonly)
    NSArray<ActorFormSuggestion*>* promptedSuggestions;

// Whether an unhandled suggestion selection completion handler is currently
// pending.
@property(nonatomic, readonly) BOOL hasPendingSuggestionSelection;

// Resets `requestConfirmationCalled` to NO.
- (void)resetRequestConfirmationCalled;

// Invokes the stored user intervention completion handler and clears it.
- (void)runUserInterventionCompletion;

// Invokes the stored suggestion completion handler with `selectedSuggestion`
// and `shouldStorePermission`. Clears the stored completion handler.
- (void)runCompletionWithSuggestion:(ActorFormSuggestion*)selectedSuggestion
              shouldStorePermission:(BOOL)shouldStorePermission;
// Convenience overload with `shouldStorePermission = NO`.
- (void)runCompletionWithSuggestion:(ActorFormSuggestion*)selectedSuggestion;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TEST_FAKE_ACTOR_TASK_INTERVENTION_DELEGATE_H_
