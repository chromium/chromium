// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_UNITTEST_UTIL_H_
#define IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_UNITTEST_UTIL_H_

#import "ios/chrome/browser/ui/bubble/bubble_view.h"

UIButton* GetCloseButtonFromBubbleView(BubbleView* bubble_view);
UILabel* GetTitleLabelFromBubbleView(BubbleView* bubble_view);
UIImageView* GetImageViewFromBubbleView(BubbleView* bubble_view);
UIButton* GetSnoozeButtonFromBubbleView(BubbleView* bubble_view);
UIView* GetArrowViewFromBubbleView(BubbleView* bubble_view);

#endif  // IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_UNITTEST_UTIL_H_
