// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_PRESENTER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_PRESENTER_DELEGATE_H_

namespace web {
class WebState;
}

@class BubblePresenter;

// Delegate for the BubblePresenter.
@protocol BubblePresenterDelegate

// Returns the currently used WebState.
- (web::WebState*)currentWebStateForBubblePresenter:
    (BubblePresenter*)bubblePresenter;
// Whether the root view is visible.
- (BOOL)rootViewVisibleForBubblePresenter:(BubblePresenter*)bubblePresenter;
// Scroll offset for the current tab.
- (BOOL)isTabScrolledToTopForBubblePresenter:(BubblePresenter*)bubblePresenter;

@end

#endif  // IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_PRESENTER_DELEGATE_H_
