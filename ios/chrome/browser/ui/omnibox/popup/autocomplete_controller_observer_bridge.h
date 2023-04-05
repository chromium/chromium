// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_CONTROLLER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_CONTROLLER_OBSERVER_BRIDGE_H_

#import "components/omnibox/browser/autocomplete_controller.h"

@protocol AutocompleteControllerObserver

- (void)autocompleteController:(AutocompleteController*)controller
             didStartWithInput:(const AutocompleteInput&)input;

- (void)autocompleteController:(AutocompleteController*)controller
    didUpdateResultChangingDefaultMatch:(BOOL)defaultMatchChanged;

@end

class AutocompleteControllerObserverBridge
    : public AutocompleteController::Observer {
 public:
  AutocompleteControllerObserverBridge(
      id<AutocompleteControllerObserver> observer);
  AutocompleteControllerObserverBridge(
      const AutocompleteControllerObserverBridge&) = delete;
  AutocompleteControllerObserverBridge& operator=(
      const AutocompleteControllerObserverBridge&) = delete;

  void OnStart(AutocompleteController* controller,
               const AutocompleteInput& input) override;

  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

 private:
  __weak id<AutocompleteControllerObserver> observer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_CONTROLLER_OBSERVER_BRIDGE_H_
