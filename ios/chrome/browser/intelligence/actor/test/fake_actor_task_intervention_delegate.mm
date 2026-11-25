// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/test/fake_actor_task_intervention_delegate.h"

#import "base/types/expected.h"

@interface FakeActorTaskInterventionDelegate ()
@property(nonatomic, readwrite) BOOL requestConfirmationCalled;
@property(nonatomic, readwrite, copy) NSString* confirmationTitle;
@property(nonatomic, readwrite, copy) NSString* confirmationSubtitle;
@property(nonatomic, readwrite, copy) NSString* confirmationButtonText;
@property(nonatomic, readwrite) BOOL selectFromSuggestionsCalled;
@property(nonatomic, readwrite, copy)
    NSArray<ActorFormSuggestion*>* promptedSuggestions;
@property(nonatomic, readwrite) BOOL cancelSuggestionSelectionCalled;
@property(nonatomic, readwrite) BOOL requestNavigationConfirmationCalled;
@property(nonatomic, readwrite, copy) NSString* promptedNavigationDomain;
@property(nonatomic, readwrite, copy) NSString* clarificationMessage;
@end

@implementation FakeActorTaskInterventionDelegate {
  ActorConfirmationCallback _confirmationCallback;
  ActorClarificationCallback _clarificationCallback;
  ActorSuggestionSelectionCallback _selectSuggestionCallback;
  ActorNavigationConfirmationCallback _navigationConfirmationCallback;
}

#pragma mark - Public

- (void)resetRequestConfirmationCalled {
  self.requestConfirmationCalled = NO;
}

- (BOOL)hasPendingUserIntervention {
  return _confirmationCallback != nil;
}

- (BOOL)hasPendingClarification {
  return _clarificationCallback != nil;
}

- (void)runUserInterventionCompletion {
  if (_confirmationCallback) {
    ActorConfirmationCallback callback = _confirmationCallback;
    _confirmationCallback = nil;
    callback(base::ok());
    if (self.onUserInterventionCompleted) {
      self.onUserInterventionCompleted();
    }
  }
}

- (void)failUserInterventionWithError:(actor::InterventionDelegateError)error {
  ActorConfirmationCallback callback = _confirmationCallback;
  _confirmationCallback = nil;
  if (callback) {
    callback(base::unexpected(error));
    if (self.onUserInterventionCompleted) {
      self.onUserInterventionCompleted();
    }
  }
}

- (void)runClarificationCompletionWithAnswer:(NSString*)answer {
  ActorClarificationCallback callback = _clarificationCallback;
  _clarificationCallback = nil;
  if (callback) {
    callback(answer);
    if (self.onClarificationCompleted) {
      self.onClarificationCompleted();
    }
  }
}

- (void)failClarificationWithError:(actor::InterventionDelegateError)error {
  ActorClarificationCallback callback = _clarificationCallback;
  _clarificationCallback = nil;
  if (callback) {
    callback(base::unexpected(error));
    if (self.onClarificationCompleted) {
      self.onClarificationCompleted();
    }
  }
}

- (BOOL)hasPendingSuggestionSelection {
  return _selectSuggestionCallback != nil;
}

- (void)runCompletionWithSuggestion:(ActorFormSuggestion*)selectedSuggestion
              shouldStorePermission:(BOOL)shouldStorePermission {
  if (_selectSuggestionCallback) {
    ActorSuggestionSelectionCallback callback = _selectSuggestionCallback;
    _selectSuggestionCallback = nil;
    callback(actor::SuggestionSelection{
        .suggestion = selectedSuggestion,
        .should_store_permission = static_cast<bool>(shouldStorePermission)});
  }
}

- (BOOL)hasPendingNavigationConfirmation {
  return _navigationConfirmationCallback != nil;
}

- (void)runNavigationConfirmationCompletion:(BOOL)confirmed {
  if (_navigationConfirmationCallback) {
    ActorNavigationConfirmationCallback callback =
        _navigationConfirmationCallback;
    _navigationConfirmationCallback = nil;
    callback(static_cast<bool>(confirmed));
  }
}

- (void)failNavigationConfirmationWithError:
    (actor::InterventionDelegateError)error {
  ActorNavigationConfirmationCallback callback =
      _navigationConfirmationCallback;
  _navigationConfirmationCallback = nil;
  if (callback) {
    callback(base::unexpected(error));
  }
}

- (void)runCompletionWithSuggestion:(ActorFormSuggestion*)selectedSuggestion {
  [self runCompletionWithSuggestion:selectedSuggestion
              shouldStorePermission:NO];
}

- (void)failSuggestionSelectionWithError:
    (actor::InterventionDelegateError)error {
  ActorSuggestionSelectionCallback callback = _selectSuggestionCallback;
  _selectSuggestionCallback = nil;
  if (callback) {
    callback(base::unexpected(error));
  }
}

#pragma mark - ActorTaskInterventionDelegate

- (void)actorTask:(actor::ActorTaskId)taskID
    requestUserInterventionWithTitle:(NSString*)title
                            subtitle:(NSString*)subtitle
                          buttonText:(NSString*)buttonText
                            callback:(ActorConfirmationCallback)callback {
  self.requestConfirmationCalled = YES;
  self.confirmationTitle = title;
  self.confirmationSubtitle = subtitle;
  self.confirmationButtonText = buttonText;
  _confirmationCallback = [callback copy];

  if (self.onUserInterventionRequested) {
    self.onUserInterventionRequested();
  }

  if (self.autoConfirmInterventions) {
    [self runUserInterventionCompletion];
  }
}

- (void)actorTask:(actor::ActorTaskId)taskID
    requestClarificationWithMessage:(NSString*)message
                           callback:(ActorClarificationCallback)callback {
  self.clarificationMessage = message;
  _clarificationCallback = [callback copy];

  if (self.autoClarificationAnswer) {
    [self runClarificationCompletionWithAnswer:self.autoClarificationAnswer];
  }
}

- (void)actorTask:(actor::ActorTaskId)taskID
    selectFromSuggestions:(NSArray<ActorFormSuggestion*>*)suggestions
                 callback:(ActorSuggestionSelectionCallback)callback {
  self.selectFromSuggestionsCalled = YES;
  self.promptedSuggestions = suggestions;
  _selectSuggestionCallback = [callback copy];

  if (self.autoSelectFirstSuggestion) {
    ActorFormSuggestion* selection = suggestions.firstObject;
    [self runCompletionWithSuggestion:selection shouldStorePermission:NO];
  }
}

- (void)actorTaskDidCancelSuggestionSelection:(actor::ActorTaskId)taskID {
  self.cancelSuggestionSelectionCalled = YES;
  _selectSuggestionCallback = nil;
}

- (void)actorTask:(actor::ActorTaskId)taskID
    requestNavigationConfirmationForDomain:(NSString*)domain
                                  callback:(ActorNavigationConfirmationCallback)
                                               callback {
  self.requestNavigationConfirmationCalled = YES;
  self.promptedNavigationDomain = domain;
  _navigationConfirmationCallback = [callback copy];

  if (self.autoConfirmInterventions) {
    [self runNavigationConfirmationCompletion:YES];
  }
}

@end
