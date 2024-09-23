// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/bubble_unittest_util.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"

UIView* GetViewOfClassWithIdentifier(Class ui_class,
                                     NSString* accessibility_identifier,
                                     BubbleView* bubble_view) {
  for (UIView* subview in bubble_view.subviews) {
    if ([subview isKindOfClass:ui_class] &&
        subview.accessibilityIdentifier == accessibility_identifier) {
      return subview;
    }
  }
  return nil;
}

UIButton* GetCloseButtonFromBubbleView(BubbleView* bubble_view) {
  return base::apple::ObjCCastStrict<UIButton>(GetViewOfClassWithIdentifier(
      [UIButton class], kBubbleViewCloseButtonIdentifier, bubble_view));
}

UILabel* GetTitleLabelFromBubbleView(BubbleView* bubble_view) {
  return base::apple::ObjCCastStrict<UILabel>(GetViewOfClassWithIdentifier(
      [UILabel class], kBubbleViewTitleLabelIdentifier, bubble_view));
}

UIImageView* GetImageViewFromBubbleView(BubbleView* bubble_view) {
  return base::apple::ObjCCastStrict<UIImageView>(GetViewOfClassWithIdentifier(
      [UIImageView class], kBubbleViewImageViewIdentifier, bubble_view));
}

UIButton* GetSnoozeButtonFromBubbleView(BubbleView* bubble_view) {
  return base::apple::ObjCCastStrict<UIButton>(GetViewOfClassWithIdentifier(
      [UIButton class], kBubbleViewSnoozeButtonIdentifier, bubble_view));
}

UIView* GetArrowViewFromBubbleView(BubbleView* bubble_view) {
  return GetViewOfClassWithIdentifier(
      [UIView class], kBubbleViewArrowViewIdentifier, bubble_view);
}
