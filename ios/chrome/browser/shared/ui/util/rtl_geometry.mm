// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"

#import <UIKit/UIKit.h>

#import "base/check_op.h"

bool UseRTLLayout() {
  return base::i18n::IsRTL();
}

base::i18n::TextDirection LayoutDirection() {
  return UseRTLLayout() ? base::i18n::RIGHT_TO_LEFT : base::i18n::LEFT_TO_RIGHT;
}

#pragma mark - LayoutRects.

LayoutRectPosition LayoutRectPositionMake(CGFloat leading, CGFloat originY) {
  return {leading, originY};
}

const LayoutRect LayoutRectZero = {0.0, {0.0, 0.0}, {0.0, 0.0}};

LayoutRect LayoutRectMake(CGFloat leading,
                          CGFloat boundingWidth,
                          CGFloat originY,
                          CGFloat width,
                          CGFloat height) {
  return {boundingWidth, LayoutRectPositionMake(leading, originY),
          CGSizeMake(width, height)};
}

CGRect LayoutRectGetRectUsingDirection(LayoutRect layout,
                                       base::i18n::TextDirection direction) {
  CGRect rect;
  if (direction == base::i18n::RIGHT_TO_LEFT) {
    CGFloat trailing =
        layout.boundingWidth - (layout.position.leading + layout.size.width);
    rect = CGRectMake(trailing, layout.position.originY, layout.size.width,
                      layout.size.height);
  } else {
    DCHECK_EQ(direction, base::i18n::LEFT_TO_RIGHT);
    rect = CGRectMake(layout.position.leading, layout.position.originY,
                      layout.size.width, layout.size.height);
  }
  return rect;
}

CGRect LayoutRectGetRect(LayoutRect layout) {
  return LayoutRectGetRectUsingDirection(layout, LayoutDirection());
}

LayoutRect LayoutRectForRectInBoundingRectUsingDirection(
    CGRect rect,
    CGRect contextRect,
    base::i18n::TextDirection direction) {
  LayoutRect layout;
  if (direction == base::i18n::RIGHT_TO_LEFT) {
    layout.position.leading = contextRect.size.width - CGRectGetMaxX(rect);
  } else {
    layout.position.leading = CGRectGetMinX(rect);
  }
  layout.boundingWidth = contextRect.size.width;
  layout.position.originY = rect.origin.y;
  layout.size = rect.size;
  return layout;
}

LayoutRect LayoutRectForRectInBoundingRect(CGRect rect, CGRect contextRect) {
  return LayoutRectForRectInBoundingRectUsingDirection(rect, contextRect,
                                                       LayoutDirection());
}

#pragma mark - UIKit utilities

CGFloat UIEdgeInsetsGetLeading(UIEdgeInsets insets) {
  return UseRTLLayout() ? insets.right : insets.left;
}

BOOL EdgeLeadsEdge(CGFloat a, CGFloat b, base::i18n::TextDirection direction) {
  return direction == base::i18n::RIGHT_TO_LEFT ? a > b : a < b;
}

BOOL EdgeLeadsEdge(CGFloat a, CGFloat b) {
  return EdgeLeadsEdge(a, b, LayoutDirection());
}

NSTextAlignment DetermineBestAlignmentForText(NSString* text) {
  if (text.length) {
    NSString* lang = CFBridgingRelease(CFStringTokenizerCopyBestStringLanguage(
        (CFStringRef)text, CFRangeMake(0, text.length)));

    if ([NSLocale characterDirectionForLanguage:lang] ==
        NSLocaleLanguageDirectionRightToLeft) {
      return NSTextAlignmentRight;
    }
  }
  return NSTextAlignmentLeft;
}
