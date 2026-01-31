// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_VIEW_STATE_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_VIEW_STATE_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"

// Delegate protocol for handling view state changes.
@protocol GeminiViewStateDelegate <NSObject>

// Called when the view state changes.
- (void)didSwitchToViewState:(ios::provider::GeminiViewState)viewState;

// Switch to `viewState`.
- (void)switchToViewState:(ios::provider::GeminiViewState)viewState;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_VIEW_STATE_DELEGATE_H_
