// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_unittest_util.h"

#import "base/mac/foundation_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UIView* GetViewOfClassWithIdentifier(Class uiClass,
                                     NSString* accessibilityIdentifier,
                                     BubbleView* bubbleView) {
  for (UIView* subview in bubbleView.subviews) {
    if ([subview isKindOfClass:uiClass] &&
        subview.accessibilityIdentifier == accessibilityIdentifier) {
      return subview;
    }
  }
  return nil;
}

UIButton* GetCloseButtonFromBubbleView(BubbleView* bubbleView) {
  return base::mac::ObjCCastStrict<UIButton>(GetViewOfClassWithIdentifier(
      [UIButton class], kBubbleViewCloseButtonIdentifier, bubbleView));
}

UILabel* GetTitleLabelFromBubbleView(BubbleView* bubbleView) {
  return base::mac::ObjCCastStrict<UILabel>(GetViewOfClassWithIdentifier(
      [UILabel class], kBubbleViewTitleLabelIdentifier, bubbleView));
}

UIImageView* GetImageViewFromBubbleView(BubbleView* bubbleView) {
  return base::mac::ObjCCastStrict<UIImageView>(GetViewOfClassWithIdentifier(
      [UIImageView class], kBubbleViewImageViewIdentifier, bubbleView));
}

UIButton* GetSnoozeButtonFromBubbleView(BubbleView* bubbleView) {
  return base::mac::ObjCCastStrict<UIButton>(GetViewOfClassWithIdentifier(
      [UIButton class], kBubbleViewSnoozeButtonIdentifier, bubbleView));
}

UIView* GetArrowViewFromBubbleView(BubbleView* bubbleView) {
  return GetViewOfClassWithIdentifier(
      [UIView class], kBubbleViewArrowViewIdentifier, bubbleView);
}
