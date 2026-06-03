// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_TAB_PICKER_HANDLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_TAB_PICKER_HANDLER_H_

#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_picker_delegate.h"

// The handler for Gemini Tab Picker actions. Configures and presents the Tab
// Picker UI in response to GeminiTabPickerDelegate actions.
@interface GeminiTabPickerHandler : NSObject <GeminiTabPickerDelegate>
@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_TAB_PICKER_HANDLER_H_
