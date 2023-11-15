// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_OMNIBOX_AUTOCOMPLETE_EVENT_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_OMNIBOX_AUTOCOMPLETE_EVENT_H_

#import "ios/chrome/browser/ui/omnibox/popup/debugger/omnibox_event.h"

class AutocompleteController;
@class AutocompleteMatchFormatter;

// A class that captures the state of the AutocompleteController at an update
// observation.
@interface OmniboxAutocompleteEvent : NSObject <OmniboxEvent>

- (OmniboxAutocompleteEvent*)initWithAutocompleteController:
    (AutocompleteController*)controller;

// A list of the autocomplete matches.
@property(nonatomic, strong)
    NSMutableArray<AutocompleteMatchFormatter*>* matches;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_OMNIBOX_AUTOCOMPLETE_EVENT_H_
