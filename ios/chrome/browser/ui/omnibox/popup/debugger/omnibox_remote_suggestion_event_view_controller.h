// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_OMNIBOX_REMOTE_SUGGESTION_EVENT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_OMNIBOX_REMOTE_SUGGESTION_EVENT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class OmniboxRemoteSuggestionEvent;

// ViewController displaying the remote suggestion service update evetn.
@interface OmniboxRemoteSuggestionEventViewController : UIViewController

@property(nonatomic, strong) OmniboxRemoteSuggestionEvent* event;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_OMNIBOX_REMOTE_SUGGESTION_EVENT_VIEW_CONTROLLER_H_
