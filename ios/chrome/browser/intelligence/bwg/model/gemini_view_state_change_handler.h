// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_VIEW_STATE_CHANGE_HANDLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_VIEW_STATE_CHANGE_HANDLER_H_

#import "ios/chrome/browser/intelligence/bwg/model/gemini_view_state_delegate.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"

// Target interface to handle changes in the Gemini view state.
class GeminiViewStateChangeHandlerTarget {
 public:
  virtual ~GeminiViewStateChangeHandlerTarget() = default;

  // Called when the Gemini view state expands.
  virtual void OnGeminiViewStateExpanded() = 0;

  // Collapses floaty if invoked.
  virtual void CollapseFloatyIfInvoked() = 0;

  // Records the most recently presented state of the Gemini view to inform
  // future interactions.
  virtual void SetLastShownViewState(
      ios::provider::GeminiViewState view_state) = 0;
};

// Handler for the Gemini view state changes.
@interface GeminiViewStateChangeHandler : NSObject <GeminiViewStateDelegate>

// Initializes the handler with the given target.
- (instancetype)initWithTarget:(GeminiViewStateChangeHandlerTarget*)target
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Call this before destroying the target to prevent dangling pointer access.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_VIEW_STATE_CHANGE_HANDLER_H_
