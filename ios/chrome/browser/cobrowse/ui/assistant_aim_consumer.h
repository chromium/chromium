// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_CONSUMER_H_
#define IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_CONSUMER_H_

#import <UIKit/UIKit.h>

#import <vector>

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_history_item.h"

// Represents the current main content state of the Assistant AIM UI.
enum class AssistantAIMState {
  // The greeting/welcoming zero state shown before a thread is started.
  kZeroState,
  // The active interaction thread state with the web content.
  kThread,
  // The history state showing a list of past tasks.
  kHistory,
};

// Consumer for the Assistant AIM UI.
@protocol AssistantAIMConsumer <NSObject>

// Sets the WebState view to be displayed.
- (void)setWebStateView:(UIView*)webStateView;

// Displays the history view with the given items.
- (void)displayHistoryWithItems:
    (const std::vector<AssistantAIMHistoryItem>&)items;

// Sets the header title.
- (void)setHeaderTitle:(NSString*)title;

// Hides the input plate.
- (void)setInputPlateForceHidden:(BOOL)hidden;

// Sets the greeting message to display in the landing state.
- (void)setGreetingMessage:(NSString*)message;

// Notifies the consumer to switch to the thread view if needed.
- (void)displayThread;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_CONSUMER_H_
