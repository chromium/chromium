// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_controller_observer_bridge.h"

AutocompleteControllerObserverBridge::AutocompleteControllerObserverBridge(
    id<AutocompleteControllerObserver> observer)
    : observer_(observer) {}

void AutocompleteControllerObserverBridge::OnStart(
    AutocompleteController* controller,
    const AutocompleteInput& input) {
  [observer_ autocompleteController:controller didStartWithInput:input];
}

void AutocompleteControllerObserverBridge::OnResultChanged(
    AutocompleteController* controller,
    bool default_match_changed) {
  [observer_ autocompleteController:controller
      didUpdateResultChangingDefaultMatch:default_match_changed];
}
