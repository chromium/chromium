// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SHARED_TABS_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SHARED_TABS_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/web_state_id.h"

@class GeminiPageContext;

// Delegate for GeminiContainerMediator to communicate with the instance
// managing the shared tabs (shared by default or by user).
// As shared tabs have longer lifecycle then Gemini itself
// GeminiContainerMediator cannot own shared tabs or its' managing instance.
// Therefore, shared tabs are owned and managed by an instance necessary
// for it's lifecylce and communicate with GeminiContainerMediator through
// this interface.
@protocol GeminiSharedTabsDelegate <NSObject>

// Returns the currently inactive shared tabs for the session.
- (NSArray<GeminiPageContext*>*)inactiveSharedTabs;

// Adds the active page context to the list of shared tabs.
- (void)saveActivePageContextToSharedTabs:
    (GeminiPageContext*)active_page_context;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SHARED_TABS_DELEGATE_H_
