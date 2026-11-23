// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_INLINE_WORKLOG_VIEW_PROVIDER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_INLINE_WORKLOG_VIEW_PROVIDER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/bwg/model/gemini_custom_ui_view_provider.h"

// Renders the worklog of agent tasks inside Gemini responses, built from their
// blocks. Vended views may outlive the provider.
@interface GeminiInlineWorklogViewProvider
    : NSObject <GeminiCustomUIViewProvider>
@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_INLINE_WORKLOG_VIEW_PROVIDER_H_
