// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_CARD_SWIPE_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_CARD_SWIPE_VIEW_DELEGATE_H_

// The card swipe view delegate.
@protocol CardSwipeViewDelegate

// Notifies the delegate that the tab swipe dismissal animation has finished.
- (void)sideSwipeViewDismissAnimationDidEnd:(UIView*)sideSwipeView;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_CARD_SWIPE_VIEW_DELEGATE_H_
