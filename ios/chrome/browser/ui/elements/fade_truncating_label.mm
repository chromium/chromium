// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/elements/fade_truncating_label.h"

#include <algorithm>

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FadeTruncatingLabel ()

// Gradient used to create fade effect. Changes based on view.frame size.
@property(nonatomic, strong) UIImage* gradient;

@end

@implementation FadeTruncatingLabel

- (void)setup {
  self.backgroundColor = [UIColor clearColor];
  _truncateMode = FadeTruncatingTail;
}

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.lineBreakMode = NSLineBreakByClipping;
    [self setup];
  }
  return self;
}

- (void)awakeFromNib {
  [super awakeFromNib];
  [self setup];
}

- (void)layoutSubviews {
  [super layoutSubviews];

  // Cache the fade gradient when the bounds change.
  if (!CGRectIsEmpty(self.bounds) &&
      (!self.gradient ||
       !CGSizeEqualToSize([self.gradient size], self.bounds.size))) {
    CGRect rect =
        CGRectMake(0, 0, self.bounds.size.width, self.bounds.size.height);
    self.gradient = [self getLinearGradient:rect];
  }
}

// Draw fade gradient mask if attributedText is wider than rect.
- (void)drawTextInRect:(CGRect)requestedRect {
  CGContextRef context = UIGraphicsGetCurrentContext();
  CGContextSaveGState(context);

  if ([self.attributedText size].width > requestedRect.size.width)
    CGContextClipToMask(context, self.bounds, [self.gradient CGImage]);

  // Add the specified line break and alignment attributes to attributedText and
  // draw the result.
  NSMutableAttributedString* attributedString =
      [self.attributedText mutableCopy];
  NSMutableParagraphStyle* textStyle =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  textStyle.lineBreakMode = self.lineBreakMode;
  textStyle.alignment = self.textAlignment;
  // URLs have their text direction set to to LTR (avoids RTL characters
  // making the URL render from right to left, as per RFC 3987 Section 4.1).
  if (self.displayAsURL)
    textStyle.baseWritingDirection = NSWritingDirectionLeftToRight;
  [attributedString addAttribute:NSParagraphStyleAttributeName
                           value:textStyle
                           range:NSMakeRange(0, [self.text length])];
  [attributedString drawInRect:requestedRect];

  CGContextRestoreGState(context);
}

- (void)setTextAlignment:(NSTextAlignment)textAlignment {
  if (textAlignment == NSTextAlignmentLeft) {
    self.truncateMode = FadeTruncatingTail;
  } else if (textAlignment == NSTextAlignmentRight) {
    self.truncateMode = FadeTruncatingHead;
  } else if (textAlignment == NSTextAlignmentNatural) {
    self.truncateMode = FadeTruncatingTail;
  } else {
    NOTREACHED();
  }

  if (textAlignment != self.textAlignment)
    self.gradient = nil;

  [super setTextAlignment:textAlignment];
}

// Create gradient opacity mask based on direction.
- (UIImage*)getLinearGradient:(CGRect)rect {
  // Create an opaque context.
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceGray();
  CGContextRef context =
      CGBitmapContextCreate(nullptr, rect.size.width, rect.size.height, 8,
                            4 * rect.size.width, colorSpace, kCGImageAlphaNone);

  // White background will mask opaque, black gradient will mask transparent.
  CGContextSetFillColorWithColor(context, [UIColor whiteColor].CGColor);
  CGContextFillRect(context, rect);

  // Create gradient from white to black.
  CGFloat locs[2] = {0.0f, 1.0f};
  CGFloat components[4] = {1.0f, 1.0f, 0.0f, 1.0f};
  CGGradientRef gradient =
      CGGradientCreateWithColorComponents(colorSpace, components, locs, 2);
  CGColorSpaceRelease(colorSpace);

  // Draw head and/or tail gradient.
  CGFloat fadeWidth =
      std::min(rect.size.height * 2, (CGFloat)floor(rect.size.width / 4));
  CGFloat minX = CGRectGetMinX(rect);
  CGFloat maxX = CGRectGetMaxX(rect);
  if (self.truncateMode & FadeTruncatingTail) {
    CGFloat startX = maxX - fadeWidth;
    CGPoint startPoint = CGPointMake(startX, CGRectGetMidY(rect));
    CGPoint endPoint = CGPointMake(maxX, CGRectGetMidY(rect));
    CGContextDrawLinearGradient(context, gradient, startPoint, endPoint, 0);
  }
  if (self.truncateMode & FadeTruncatingHead) {
    CGFloat startX = minX + fadeWidth;
    CGPoint startPoint = CGPointMake(startX, CGRectGetMidY(rect));
    CGPoint endPoint = CGPointMake(minX, CGRectGetMidY(rect));
    CGContextDrawLinearGradient(context, gradient, startPoint, endPoint, 0);
  }
  CGGradientRelease(gradient);

  // Clean up, return image.
  CGImageRef ref = CGBitmapContextCreateImage(context);
  UIImage* image = [UIImage imageWithCGImage:ref];
  CGImageRelease(ref);
  CGContextRelease(context);
  return image;
}

@end
