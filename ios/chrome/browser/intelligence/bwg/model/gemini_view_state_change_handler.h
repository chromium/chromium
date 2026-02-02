// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_VIEW_STATE_CHANGE_HANDLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_VIEW_STATE_CHANGE_HANDLER_H_

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_view_state_delegate.h"

class GeminiBrowserAgent;

// Handler for the Gemini view state changes.
@interface GeminiViewStateChangeHandler : NSObject <GeminiViewStateDelegate>

- (instancetype)initWithBrowserAgent:(base::WeakPtr<GeminiBrowserAgent>)agent
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_VIEW_STATE_CHANGE_HANDLER_H_
