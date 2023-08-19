// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_PRESENTER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_PRESENTER_DELEGATE_H_

@class BubblePresenter;

// Delegate for the BubblePresenter.
@protocol BubblePresenterDelegate

// Whether the root view is visible.
- (BOOL)rootViewVisibleForBubblePresenter:(BubblePresenter*)bubblePresenter;

// Whether the NTP exists.
- (BOOL)isNTPActiveForBubblePresenter:(BubblePresenter*)bubblePresenter;

// Whether the NTP is scrolled to top.
- (BOOL)isNTPScrolledToTopForBubblePresenter:(BubblePresenter*)bubblePresenter;

@end

#endif  // IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_PRESENTER_DELEGATE_H_
