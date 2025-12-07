// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_AUTOCOMPLETE_CONTROLLER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_AUTOCOMPLETE_CONTROLLER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "components/omnibox/browser/autocomplete_controller.h"

// Objective-C observer for AutocompleteController.
@protocol AutocompleteControllerObserver <NSObject>

@optional

- (void)autocompleteController:(AutocompleteController*)controller
             didStartWithInput:(const AutocompleteInput&)input;

- (void)autocompleteController:(AutocompleteController*)controller
    didUpdateResultChangingDefaultMatch:(BOOL)defaultMatchChanged;

- (void)autocompleteController:(AutocompleteController*)controller
                    didMLScore:(const AutocompleteResult&)result;

- (void)autocompleteControllerDidTriggerStopTimer:
    (const AutocompleteInput&)input;

@end

// Bridge class to allow C++ AutocompleteController to call Objective-C
// AutocompleteControllerObserver.
class AutocompleteControllerObserverBridge
    : public AutocompleteController::Observer {
 public:
  AutocompleteControllerObserverBridge(
      id<AutocompleteControllerObserver> observer);
  ~AutocompleteControllerObserverBridge() override;
  AutocompleteControllerObserverBridge(
      const AutocompleteControllerObserverBridge&) = delete;
  AutocompleteControllerObserverBridge& operator=(
      const AutocompleteControllerObserverBridge&) = delete;

  // AutocompleteController::Observer.
  void OnStart(AutocompleteController* controller,
               const AutocompleteInput& input) override;
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;
  void OnMlScored(AutocompleteController* controller,
                  const AutocompleteResult& result) override;
  void OnAutocompleteStopTimerTriggered(
      const AutocompleteInput& input) override;

 private:
  __weak id<AutocompleteControllerObserver> observer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_AUTOCOMPLETE_CONTROLLER_OBSERVER_BRIDGE_H_
