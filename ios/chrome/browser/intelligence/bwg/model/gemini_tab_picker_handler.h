// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_TAB_PICKER_HANDLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_TAB_PICKER_HANDLER_H_

#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_picker_delegate.h"

class GeminiSharedTabsDelegate;
@protocol SnackbarCommands;
@protocol TabPickerCommands;

// The handler for Gemini Tab Picker actions. Configures and presents the Tab
// Picker UI in response to GeminiTabPickerDelegate actions.
@interface GeminiTabPickerHandler : NSObject <GeminiTabPickerDelegate>

// Delegate for shared tabs in a Gemini session.
@property(nonatomic, assign) GeminiSharedTabsDelegate* sharedTabsDelegate;

// The handler used to present the Tab Picker UI.
@property(nonatomic, weak) id<TabPickerCommands> tabPickerHandler;

// The handler used to present snackbar warnings (e.g., when the user hits the
// tab attachment limit).
@property(nonatomic, weak) id<SnackbarCommands> snackbarHandler;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_TAB_PICKER_HANDLER_H_
