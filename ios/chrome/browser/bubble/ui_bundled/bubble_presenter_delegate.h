// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_PRESENTER_DELEGATE_H_
#define IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_PRESENTER_DELEGATE_H_

@class BubblePresenter;

// Delegate for the BubblePresenter.
@protocol BubblePresenterDelegate

// Whether the root view is visible.
- (BOOL)rootViewVisibleForBubblePresenter:(BubblePresenter*)bubblePresenter;

// Whether the NTP exists.
- (BOOL)isNTPActiveForBubblePresenter:(BubblePresenter*)bubblePresenter;

// Whether the NTP is scrolled to top.
- (BOOL)isNTPScrolledToTopForBubblePresenter:(BubblePresenter*)bubblePresenter;

// Whether overscroll actions are supported.
- (BOOL)isOverscrollActionsSupportedForBubblePresenter:
    (BubblePresenter*)bubblePresenter;

// Notifies the delegate that the user has performed the pull-to-refresh gesture
// as instructed by the in-product help.
- (void)bubblePresenterDidPerformPullToRefreshGesture:
    (BubblePresenter*)bubblePresenter;

// Notifies the delegate that the user has performed the back/forward swipe
// gesture as instructed by the in-product help.
- (void)bubblePresenter:(BubblePresenter*)bubblePresenter
    didPerformSwipeToNavigateInDirection:
        (UISwipeGestureRecognizerDirection)direction;
@end

#endif  // IOS_CHROME_BROWSER_BUBBLE_UI_BUNDLED_BUBBLE_PRESENTER_DELEGATE_H_
