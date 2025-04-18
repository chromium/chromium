// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_AUDIENCE_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_AUDIENCE_H_

enum class BrowserViewVisibilityState;

/// Consumer protocol that gets notified when the browser view's visibility has
/// changed.
@protocol BrowserViewVisibilityAudience

/// Method that responds to browser view visibility changes.
- (void)browserViewDidTransitionToVisibilityState:
            (BrowserViewVisibilityState)currentState
                                        fromState:(BrowserViewVisibilityState)
                                                      previousState;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_AUDIENCE_H_
