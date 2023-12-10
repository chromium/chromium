// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_OMNIBOX_AUTOCOMPLETE_EVENT_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_OMNIBOX_AUTOCOMPLETE_EVENT_H_

#import "ios/chrome/browser/ui/omnibox/popup/debugger/autocomplete_match_group.h"
#import "ios/chrome/browser/ui/omnibox/popup/debugger/omnibox_event.h"

class AutocompleteController;
@class AutocompleteMatchFormatter;

/// A class that captures the state of the AutocompleteController at an update
/// observation.
@interface OmniboxAutocompleteEvent : NSObject <OmniboxEvent>

- (OmniboxAutocompleteEvent*)initWithAutocompleteController:
    (AutocompleteController*)controller;

/// Autocomplete match groups. The first group contains the result matches,
/// following groups contains provider matches.
@property(nonatomic, strong) NSArray<AutocompleteMatchGroup*>* matchGroups;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_OMNIBOX_AUTOCOMPLETE_EVENT_H_
