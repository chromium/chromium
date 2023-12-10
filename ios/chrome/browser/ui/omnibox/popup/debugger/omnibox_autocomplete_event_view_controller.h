// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_OMNIBOX_AUTOCOMPLETE_EVENT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_OMNIBOX_AUTOCOMPLETE_EVENT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class OmniboxAutocompleteEvent;

// A view controller that shows the UI of the omnibox autocomplete event.
@interface OmniboxAutocompleteEventViewController : UITableViewController

@property(nonatomic, strong) OmniboxAutocompleteEvent* event;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_OMNIBOX_AUTOCOMPLETE_EVENT_VIEW_CONTROLLER_H_
