// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_OMNIBOX_EVENT_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_OMNIBOX_EVENT_H_

#import <Foundation/Foundation.h>

// An enum type describing the type of an omnibox event.
enum EventType { kAutocompleteUpdate = 0, kRemoteSuggestionUpdate = 1 };

// A protocol describing an event coming to the omnibox popup.
@protocol OmniboxEvent

// Returns a title string based on the event type.
- (NSString*)title;

// Returns the type of the omnibox event.
- (EventType)type;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_OMNIBOX_EVENT_H_
