// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/autocomplete_controller_observer_bridge.h"

AutocompleteControllerObserverBridge::AutocompleteControllerObserverBridge(
    id<AutocompleteControllerObserver> observer)
    : observer_(observer) {}

AutocompleteControllerObserverBridge::~AutocompleteControllerObserverBridge() {}

void AutocompleteControllerObserverBridge::OnStart(
    AutocompleteController* controller,
    const AutocompleteInput& input) {
  if ([observer_ respondsToSelector:@selector(autocompleteController:
                                                   didStartWithInput:)]) {
    [observer_ autocompleteController:controller didStartWithInput:input];
  }
}

void AutocompleteControllerObserverBridge::OnResultChanged(
    AutocompleteController* controller,
    bool default_match_changed) {
  if ([observer_
          respondsToSelector:@selector(autocompleteController:
                                 didUpdateResultChangingDefaultMatch:)]) {
    [observer_ autocompleteController:controller
        didUpdateResultChangingDefaultMatch:default_match_changed];
  }
}

void AutocompleteControllerObserverBridge::OnMlScored(
    AutocompleteController* controller,
    const AutocompleteResult& result) {
  if ([observer_ respondsToSelector:@selector(autocompleteController:
                                                          didMLScore:)]) {
    [observer_ autocompleteController:controller didMLScore:result];
  }
}

void AutocompleteControllerObserverBridge::OnAutocompleteStopTimerTriggered(
    const AutocompleteInput& input) {
  if ([observer_ respondsToSelector:@selector
                 (autocompleteControllerDidTriggerStopTimer:)]) {
    [observer_ autocompleteControllerDidTriggerStopTimer:input];
  }
}
