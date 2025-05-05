// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_DEBUGGER_OMNIBOX_DEBUGGER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_OMNIBOX_DEBUGGER_OMNIBOX_DEBUGGER_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/omnibox/debugger/autocomplete_controller_observer_bridge.h"
#import "ios/chrome/browser/omnibox/debugger/remote_suggestions_service_observer_bridge.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller_debugger_delegate.h"

@protocol OmniboxDebuggerConsumer;
@protocol RemoteSuggestionsServiceObserver;
@protocol AutocompleteControllerObserver;
class AutocompleteController;
class RemoteSuggestionsService;

// The omnibox debugger mediator.
@interface OmniboxDebuggerMediator
    : NSObject <OmniboxAutocompleteControllerDebuggerDelegate,
                RemoteSuggestionsServiceObserver,
                AutocompleteControllerObserver>

- (instancetype)initWithAutocompleteController:
                    (AutocompleteController*)autocompleteController
                      remoteSuggestionsService:
                          (RemoteSuggestionsService*)remoteSuggestionsService;

/// Disconnects the omnibox debugger mediator.
- (void)disconnect;

/// The omnibox debugger consumer.
@property(nonatomic, weak) id<OmniboxDebuggerConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_DEBUGGER_OMNIBOX_DEBUGGER_MEDIATOR_H_
