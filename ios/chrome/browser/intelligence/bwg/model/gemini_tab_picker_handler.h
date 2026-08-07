// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_TAB_PICKER_HANDLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_TAB_PICKER_HANDLER_H_

#import <set>

#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_picker_delegate.h"
#import "ios/web/public/web_state_id.h"

@protocol SnackbarCommands;
@protocol TabPickerCommands;

// Callback type invoked when the tab picker selection changes.
typedef void (^GeminiTabPickerSelectionCallback)(
    std::set<web::WebStateID> selectedIDs);

// Callback type invoked to determine which tabs to pre-select.
typedef std::set<web::WebStateID> (^GeminiTabPickerSelectedTabsProvider)();

// The handler for Gemini Tab Picker actions. Configures and presents the Tab
// Picker UI in response to GeminiTabPickerDelegate actions.
@interface GeminiTabPickerHandler : NSObject <GeminiTabPickerDelegate>

// Callback invoked when the user changes their selected tabs via the Tab
// Picker.
@property(nonatomic, copy) GeminiTabPickerSelectionCallback selectionCallback;

// Provider invoked when the tab picker is opened to determine the pre-selected
// tabs.
@property(nonatomic, copy)
    GeminiTabPickerSelectedTabsProvider selectedTabsProvider;

// The handler used to present the Tab Picker UI.
@property(nonatomic, weak) id<TabPickerCommands> tabPickerHandler;

// The handler used to present snackbar warnings (e.g., when the user hits the
// tab attachment limit).
@property(nonatomic, weak) id<SnackbarCommands> snackbarHandler;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_TAB_PICKER_HANDLER_H_
