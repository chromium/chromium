// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CUSTOM_UI_HANDLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CUSTOM_UI_HANDLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/bwg/model/gemini_custom_ui_delegate.h"

@protocol GeminiCustomUIViewProvider;

// Routes Gemini response blocks to the view providers that render them.
@interface GeminiCustomUIHandler : NSObject <GeminiCustomUIDelegate>

// Adds `provider`, held weakly. Providers must render distinct block types.
- (void)addViewProvider:(id<GeminiCustomUIViewProvider>)provider;

// Removes `provider`, if added.
- (void)removeViewProvider:(id<GeminiCustomUIViewProvider>)provider;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CUSTOM_UI_HANDLER_H_
