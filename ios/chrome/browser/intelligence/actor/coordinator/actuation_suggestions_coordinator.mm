// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_suggestions_coordinator.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"

@implementation ActuationSuggestionsCoordinator {
  // Suggestions that should be displayed.
  NSArray<ActorFormSuggestion*>* _suggestions;

  // The worklog item triggering the actuation suggestions.
  ActuationWorklogItem* _worklogItem;

  // The view controller displaying the suggestions.
  UIViewController* _suggestionsViewController;
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
  if (_suggestionsViewController.presentingViewController) {
    [_suggestionsViewController.presentingViewController
        dismissViewControllerAnimated:NO
                           completion:nil];
  }
  _suggestionsViewController = nil;
  _suggestions = nil;
  _worklogItem = nil;
}

#pragma mark - ActuationSuggestionsPresenter

- (void)showSuggestionsPickerForAccessory:
    (ActuationWorklogAccessoryItem*)accessoryItem {
  if (_worklogItem.accessoryItem != accessoryItem ||
      _suggestionsViewController) {
    return;
  }
  // TODO(crbug.com/571139088): Replace temporary alert with credential picker
  // bottom sheet.
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:@"Actuation Suggestions"
                       message:[NSString stringWithFormat:
                                             @"%lu suggestion(s) available",
                                             (unsigned long)_suggestions.count]
                preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"Ack"
                                            style:UIAlertActionStyleDefault
                                          handler:nil]];
  _suggestionsViewController = alert;
  [self.baseViewController presentViewController:_suggestionsViewController
                                        animated:YES
                                      completion:nil];
}

@end
