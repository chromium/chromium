// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_VIEW_CONTROLLER_PRESENTER_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_VIEW_CONTROLLER_PRESENTER_PRIVATE_H_

#import "ios/chrome/browser/ui/bubble/bubble_view_controller_presenter.h"

// Class extension exposing private properties of BubbleViewControllerPresenter
// for testing.
@interface BubbleViewControllerPresenter ()

// The underlying BubbleViewController managed by this object.
// `bubbleViewController` manages the BubbleView instance.
@property(nonatomic, strong) BubbleViewController* bubbleViewController;

// The timer used to dismiss the bubble after a certain length of time. The
// bubble is dismissed automatically if the user does not dismiss it manually.
// If the user dismisses it manually, this timer is invalidated. The timer
// maintains a strong reference to the presenter, so it must be retained weakly
// to prevent a retain cycle. The run loop retains a strong reference to the
// timer so it is not deallocated until it is invalidated.
@property(nonatomic, strong) NSTimer* bubbleDismissalTimer;

// The timer used to reset the user's engagement. The user is considered
// engaged with the bubble while it is visible and for a certain duration after
// it disappears. The timer maintains a strong reference to the presenter, so it
// must be retained weakly to prevent a retain cycle. The run loop retains a
// strong reference to the timer so it is not deallocated until it is
// invalidated.
@property(nonatomic, strong) NSTimer* engagementTimer;

@end

#endif  // IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_VIEW_CONTROLLER_PRESENTER_PRIVATE_H_
