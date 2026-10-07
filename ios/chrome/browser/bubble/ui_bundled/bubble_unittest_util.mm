// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/bubble_unittest_util.h"

#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/test/app/uikit_test_util.h"

using chrome_test_util::FindViewById;

UIButton* GetCloseButtonFromBubbleView(BubbleView* bubble_view) {
  return FindViewById<UIButton>(bubble_view, kBubbleViewCloseButtonIdentifier);
}

UILabel* GetTitleLabelFromBubbleView(BubbleView* bubble_view) {
  return FindViewById<UILabel>(bubble_view, kBubbleViewTitleLabelIdentifier);
}

UIButton* GetSnoozeButtonFromBubbleView(BubbleView* bubble_view) {
  return FindViewById<UIButton>(bubble_view, kBubbleViewSnoozeButtonIdentifier);
}

UIView* GetArrowViewFromBubbleView(BubbleView* bubble_view) {
  return FindViewById(bubble_view, kBubbleViewArrowViewIdentifier);
}

UIButton* GetNextButtonFromBubbleView(BubbleView* bubble_view) {
  return FindViewById<UIButton>(bubble_view, kBubbleViewNextButtonIdentifier);
}

UIStackView* GetPageControlPageBubbleView(BubbleView* bubble_view) {
  return FindViewById<UIStackView>(bubble_view,
                                   kBubbleViewPageControlIdentifier);
}
