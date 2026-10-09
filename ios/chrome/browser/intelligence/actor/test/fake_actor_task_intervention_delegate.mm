// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/test/fake_actor_task_intervention_delegate.h"

@interface FakeActorTaskInterventionDelegate ()
@property(nonatomic, readwrite) BOOL requestConfirmationCalled;
@property(nonatomic, readwrite, copy) NSString* confirmationTitle;
@property(nonatomic, readwrite, copy) NSString* confirmationSubtitle;
@property(nonatomic, readwrite, copy) NSString* confirmationButtonText;
@property(nonatomic, readwrite) BOOL selectFromSuggestionsCalled;
@property(nonatomic, readwrite, copy)
    NSArray<ActorFormSuggestion*>* promptedSuggestions;
@end

@implementation FakeActorTaskInterventionDelegate {
  void (^_confirmationCompletionHandler)(void);
  void (^_selectSuggestionCompletionHandler)(ActorFormSuggestion*, BOOL);
}

#pragma mark - Public

- (void)resetRequestConfirmationCalled {
  self.requestConfirmationCalled = NO;
}

- (BOOL)hasPendingUserIntervention {
  return _confirmationCompletionHandler != nil;
}

- (void)runUserInterventionCompletion {
  if (_confirmationCompletionHandler) {
    void (^completion)(void) = _confirmationCompletionHandler;
    _confirmationCompletionHandler = nil;
    completion();
    if (self.onUserInterventionCompleted) {
      self.onUserInterventionCompleted();
    }
  }
}

- (BOOL)hasPendingSuggestionSelection {
  return _selectSuggestionCompletionHandler != nil;
}

- (void)runCompletionWithSuggestion:(ActorFormSuggestion*)selectedSuggestion
              shouldStorePermission:(BOOL)shouldStorePermission {
  if (_selectSuggestionCompletionHandler) {
    void (^completion)(ActorFormSuggestion*, BOOL) =
        _selectSuggestionCompletionHandler;
    _selectSuggestionCompletionHandler = nil;
    completion(selectedSuggestion, shouldStorePermission);
  }
}

- (void)runCompletionWithSuggestion:(ActorFormSuggestion*)selectedSuggestion {
  [self runCompletionWithSuggestion:selectedSuggestion
              shouldStorePermission:NO];
}

#pragma mark - ActorTaskInterventionDelegate

- (void)actorTask:(actor::ActorTaskId)taskID
    requestUserInterventionWithTitle:(NSString*)title
                            subtitle:(NSString*)subtitle
                          buttonText:(NSString*)buttonText
                   completionHandler:(void (^)(void))completionHandler {
  self.requestConfirmationCalled = YES;
  self.confirmationTitle = title;
  self.confirmationSubtitle = subtitle;
  self.confirmationButtonText = buttonText;
  _confirmationCompletionHandler = [completionHandler copy];

  if (self.onUserInterventionRequested) {
    self.onUserInterventionRequested();
  }

  if (self.autoConfirmInterventions) {
    [self runUserInterventionCompletion];
  }
}

- (void)actorTask:(actor::ActorTaskId)taskID
    selectFromSuggestions:(NSArray<ActorFormSuggestion*>*)suggestions
        completionHandler:
            (void (^)(ActorFormSuggestion* selectedSuggestion,
                      BOOL shouldStorePermission))completionHandler {
  self.selectFromSuggestionsCalled = YES;
  self.promptedSuggestions = suggestions;
  _selectSuggestionCompletionHandler = [completionHandler copy];

  if (self.autoSelectFirstSuggestion) {
    ActorFormSuggestion* selection = suggestions.firstObject;
    [self runCompletionWithSuggestion:selection shouldStorePermission:NO];
  }
}

@end
