// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TASK_INTERVENTION_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TASK_INTERVENTION_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

@class ActorFormSuggestion;

namespace actor {

// The user's choice in a suggestion selection prompt.
struct SuggestionSelection {
  // The selected suggestion, or `nil` if the user closed the prompt.
  ActorFormSuggestion* suggestion = nil;
  // Whether the suggestion can be used in future calls to the tool.
  bool should_store_permission = false;
};

}  // namespace actor

// Receives the result of a confirmation prompt.
using ActorConfirmationCallback =
    void (^)(actor::InterventionDelegateResult<void>);

// Receives the user's non-empty answer to a clarification question.
using ActorClarificationCallback =
    void (^)(actor::InterventionDelegateResult<NSString*>);

// Receives the user's choice in a suggestion selection prompt.
using ActorSuggestionSelectionCallback =
    void (^)(actor::InterventionDelegateResult<actor::SuggestionSelection>);

// Receives the user's decision on a navigation confirmation prompt (`true` if
// the user confirmed).
using ActorNavigationConfirmationCallback =
    void (^)(actor::InterventionDelegateResult<bool>);

// The ActorTask intervention delegate protocol (1-to-1). Used for blocking
// interventions (likely user facing prompts, but can be programmatic in
// headless Actor mode). A delegate that cannot handle a request runs its
// callback with `kUnsupported`. Except for `selectFromSuggestions:` (cancelled
// via `actorTaskDidCancelSuggestionSelection:`), prompts receive no cancel call
// when the task pauses or stops; the delegate should dismiss them on task state
// changes, and later answers are dropped.
@protocol ActorTaskInterventionDelegate <NSObject>

// Dismisses the prompt shown by `selectFromSuggestions:` for `taskID` without
// running its callback. Called when the tool that requested a selection ends
// before the user answers.
- (void)actorTaskDidCancelSuggestionSelection:(actor::ActorTaskId)taskID;

// Prompts the user to select a suggestion from the list. `callback` must be
// non-nil and runs at most once, with the selection, or with an error if the
// prompt cannot be shown.
- (void)actorTask:(actor::ActorTaskId)taskID
    selectFromSuggestions:(NSArray<ActorFormSuggestion*>*)suggestions
                 callback:(ActorSuggestionSelectionCallback)callback;

// Prompts the user with an intervention card containing a title,
// subtitle, and action button text. `callback` must be non-nil and runs at
// most once: when the user confirms the action to resume the task, or with an
// error if the card cannot be shown. To abort, the user should stop the
// execution.
- (void)actorTask:(actor::ActorTaskId)taskID
    requestUserInterventionWithTitle:(NSString*)title
                            subtitle:(NSString*)subtitle
                          buttonText:(NSString*)buttonText
                            callback:(ActorConfirmationCallback)callback;

// Prompts the user to confirm or decline navigating to `domain`. `callback`
// must be non-nil and runs at most once: with `true` if the user taps
// "Continue", `false` if the user taps "Don't use this site.", or with an
// error if the prompt cannot be shown.
- (void)actorTask:(actor::ActorTaskId)taskID
    requestNavigationConfirmationForDomain:(NSString*)domain
                                  callback:(ActorNavigationConfirmationCallback)
                                               callback;

// Asks the user `message` and waits for a typed answer. `callback` must be
// non-nil and runs at most once: with the user's non-empty answer, or with an
// error if the question cannot be asked. To abort, the user should stop the
// execution.
- (void)actorTask:(actor::ActorTaskId)taskID
    requestClarificationWithMessage:(NSString*)message
                           callback:(ActorClarificationCallback)callback;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TASK_INTERVENTION_DELEGATE_H_
