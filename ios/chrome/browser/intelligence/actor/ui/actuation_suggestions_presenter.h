// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_SUGGESTIONS_PRESENTER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_SUGGESTIONS_PRESENTER_H_

#import <Foundation/Foundation.h>

@class ActuationWorklogAccessoryItem;

// Presenter protocol for showing suggestions picker UI.
@protocol ActuationSuggestionsPresenter

// Requests presenting the suggestions picker UI for `accessoryItem`.
- (void)showSuggestionsPickerForAccessory:
    (ActuationWorklogAccessoryItem*)accessoryItem;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_SUGGESTIONS_PRESENTER_H_
