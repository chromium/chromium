// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_UNITTEST_UTIL_H_
#define IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_UNITTEST_UTIL_H_

#import "ios/chrome/browser/ui/bubble/bubble_view.h"

UIButton* GetCloseButtonFromBubbleView(BubbleView* bubbleView);
UILabel* GetTitleLabelFromBubbleView(BubbleView* bubbleView);
UIImageView* GetImageViewFromBubbleView(BubbleView* bubbleView);
UIButton* GetSnoozeButtonFromBubbleView(BubbleView* bubbleView);
UIView* GetArrowViewFromBubbleView(BubbleView* bubbleView);

#endif  // IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_UNITTEST_UTIL_H_
