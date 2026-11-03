// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_suggestions_coordinator.h"

#import "base/check.h"
#import "base/notreached.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_suggestions_delegate.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_form_suggestion.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/credential_suggestion_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

@interface ActuationSuggestionsCoordinator () <
    CredentialSuggestionBottomSheetCoordinatorDelegate>
@end

@implementation ActuationSuggestionsCoordinator {
  // Suggestions that should be displayed.
  NSArray<ActorFormSuggestion*>* _suggestions;

  // Child coordinator managing the credential suggestions bottom sheet.
  CredentialSuggestionBottomSheetCoordinator*
      _credentialSuggestionBottomSheetCoordinator;

  // Temporary view controller displaying non-credential suggestions.
  UIAlertController* _paymentAlert;

  // The index of the suggestion that is currently selected.
  NSInteger _activeIndex;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               suggestions:
                                   (NSArray<ActorFormSuggestion*>*)suggestions
                               worklogItem:(ActuationWorklogItem*)worklogItem {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _suggestions = [suggestions copy];
    _worklogItem = worklogItem;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)stop {
  [self credentialSuggestionBottomSheetCoordinatorDidDismiss:
            _credentialSuggestionBottomSheetCoordinator];
  if (_paymentAlert.presentingViewController) {
    [_paymentAlert.presentingViewController dismissViewControllerAnimated:NO
                                                               completion:nil];
  }
  _paymentAlert = nil;
  _suggestions = nil;
  _worklogItem = nil;
  _activeIndex = 0;
}

#pragma mark - ActuationSuggestionsPresenter

- (void)showSuggestionsPickerForAccessory:
    (ActuationWorklogAccessoryItem*)accessoryItem {
  if (_worklogItem.accessoryItem != accessoryItem ||
      _credentialSuggestionBottomSheetCoordinator || _paymentAlert) {
    return;
  }
  switch (_suggestions.firstObject.type) {
    case autofill::SuggestionType::kIdentityCredential:
    case autofill::SuggestionType::kPasswordEntry:
      [self createAndPresentCredentialsBottomSheet];
      break;

    case autofill::SuggestionType::kAddressEntry:
    case autofill::SuggestionType::kCreditCardEntry:
      [self createAndPresentPaymentBottomSheet];
      break;
    default:
      NOTREACHED(base::NotFatalUntil::M165);
  }
}

#pragma mark - CredentialSuggestionBottomSheetCoordinatorDelegate

- (void)credentialSuggestionBottomSheetCoordinator:
            (CredentialSuggestionBottomSheetCoordinator*)coordinator
                               didSelectSuggestion:
                                   (FormSuggestion*)formSuggestion
                                           atIndex:(NSInteger)index {
  if (index >= 0 && static_cast<NSUInteger>(index) < _suggestions.count) {
    ActorFormSuggestion* selectedSuggestion = _suggestions[index];
    if (selectedSuggestion.formSuggestion == formSuggestion) {
      _activeIndex = index;
      [self.delegate suggestionsCoordinator:self
                        didSelectSuggestion:selectedSuggestion];
    }
  }
}

- (void)credentialSuggestionBottomSheetCoordinatorDidDismiss:
    (CredentialSuggestionBottomSheetCoordinator*)coordinator {
  CHECK_EQ(coordinator, _credentialSuggestionBottomSheetCoordinator);
  _credentialSuggestionBottomSheetCoordinator.delegate = nil;
  [_credentialSuggestionBottomSheetCoordinator stop];
  _credentialSuggestionBottomSheetCoordinator = nil;
}

#pragma mark - Private

// Helper method to present the bottom sheet to display credential suggestions.
- (void)createAndPresentCredentialsBottomSheet {
  NSMutableArray<FormSuggestion*>* formSuggestions =
      [[NSMutableArray alloc] initWithCapacity:_suggestions.count];
  for (ActorFormSuggestion* suggestion in _suggestions) {
    CHECK(suggestion.formSuggestion, base::NotFatalUntil::M166);
    [formSuggestions addObject:suggestion.formSuggestion];
  }
  if (formSuggestions.count <= 0) {
    return;
  }
  _credentialSuggestionBottomSheetCoordinator =
      [[CredentialSuggestionBottomSheetCoordinator alloc]
          initWithBaseViewController:self.baseViewController
                             browser:self.browser
                         suggestions:formSuggestions
                         activeIndex:_activeIndex];
  _credentialSuggestionBottomSheetCoordinator.delegate = self;
  [_credentialSuggestionBottomSheetCoordinator start];
}

// Helper method to present the bottom sheet to display payment and contact info
// suggestions.
- (void)createAndPresentPaymentBottomSheet {
  // TODO(crbug.com/571139088): Replace temporary alert with suggestion
  // picker bottom sheet for suggestion types. This method might also be split
  // into multiple, based on suggestion type.
  _paymentAlert = [UIAlertController
      alertControllerWithTitle:@"Actuation Suggestions"
                       message:[NSString stringWithFormat:
                                             @"%lu suggestion(s) available",
                                             (unsigned long)_suggestions.count]
                preferredStyle:UIAlertControllerStyleAlert];
  [_paymentAlert
      addAction:[UIAlertAction actionWithTitle:@"Ack"
                                         style:UIAlertActionStyleDefault
                                       handler:nil]];
  [self.baseViewController presentViewController:_paymentAlert
                                        animated:YES
                                      completion:nil];
}

@end
