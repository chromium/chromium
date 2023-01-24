// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/elements/fade_truncating_label.h"

#import <algorithm>

#import "base/notreached.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

/// Creates a gradient opacity mask based on direction of `truncate_mode` for
/// `rect`.
UIImage* CreateLinearGradient(CGRect rect, BOOL fade_left, BOOL fade_right) {
  // Create an opaque context.
  CGColorSpaceRef color_space = CGColorSpaceCreateDeviceGray();
  CGContextRef context = CGBitmapContextCreate(
      nullptr, rect.size.width, rect.size.height, 8, 4 * rect.size.width,
      color_space, kCGImageAlphaNone);

  // White background will mask opaque, black gradient will mask transparent.
  CGContextSetFillColorWithColor(context, [UIColor whiteColor].CGColor);
  CGContextFillRect(context, rect);

  // Create gradient from white to black.
  CGFloat locs[2] = {0.0f, 1.0f};
  CGFloat components[4] = {1.0f, 1.0f, 0.0f, 1.0f};
  CGGradientRef gradient =
      CGGradientCreateWithColorComponents(color_space, components, locs, 2);
  CGColorSpaceRelease(color_space);

  // Draw head and/or tail gradient.
  CGFloat fade_width =
      std::min(rect.size.height * 2, (CGFloat)floor(rect.size.width / 4));
  CGFloat minX = CGRectGetMinX(rect);
  CGFloat maxX = CGRectGetMaxX(rect);
  if (fade_right) {
    CGFloat start_x = maxX - fade_width;
    CGPoint start_point = CGPointMake(start_x, CGRectGetMidY(rect));
    CGPoint end_point = CGPointMake(maxX, CGRectGetMidY(rect));
    CGContextDrawLinearGradient(context, gradient, start_point, end_point, 0);
  }
  if (fade_left) {
    CGFloat start_x = minX + fade_width;
    CGPoint start_point = CGPointMake(start_x, CGRectGetMidY(rect));
    CGPoint end_point = CGPointMake(minX, CGRectGetMidY(rect));
    CGContextDrawLinearGradient(context, gradient, start_point, end_point, 0);
  }
  CGGradientRelease(gradient);

  // Clean up, return image.
  CGImageRef ref = CGBitmapContextCreateImage(context);
  UIImage* image = [UIImage imageWithCGImage:ref];
  CGImageRelease(ref);
  CGContextRelease(context);
  return image;
}

/// Add attributes to a copy of `attributed_string`.
NSAttributedString* AttributedStringCopyWithAttributes(
    NSAttributedString* attributed_string,
    NSLineBreakMode line_break_mode,
    NSTextAlignment text_alignment,
    BOOL force_left_to_right) {
  NSMutableAttributedString* text_copy = attributed_string.mutableCopy;
  NSMutableParagraphStyle* text_style =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  text_style.lineBreakMode = line_break_mode;
  text_style.alignment = text_alignment;
  if (force_left_to_right) {
    text_style.baseWritingDirection = NSWritingDirectionLeftToRight;
  }
  [text_copy addAttribute:NSParagraphStyleAttributeName
                    value:text_style
                    range:NSMakeRange(0, attributed_string.length)];
  return text_copy;
}

}  // namespace

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
    self.gradient = CreateLinearGradient(
        rect, /*fade_left=*/self.truncateMode & FadeTruncatingHead,
        /*fade_right=*/self.truncateMode & FadeTruncatingTail);
  }
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

#pragma mark - Text Drawing

/// Draws `attributedText` in `requestedRect` and apply gradient mask if the
/// text is wider than rect.
- (void)drawTextInRect:(CGRect)requestedRect {
  NSAttributedString* configuredString =
      [self attributedString:self.attributedText
           withLineBreakMode:self.lineBreakMode];

  // Draw fade gradient mask if attributedText is wider than rect.
  BOOL shouldApplyGradient =
      self.attributedText.size.width > requestedRect.size.width;

  [self drawAttributedString:configuredString
                      inRect:requestedRect
               applyGradient:shouldApplyGradient];
}

#pragma mark Text Drawing Private

/// Draws `attributedString` in `requestedRect` and `applyGradient`.
- (void)drawAttributedString:(NSAttributedString*)attributedString
                      inRect:(CGRect)requestedRect
               applyGradient:(BOOL)applyGradient {
  CGContextRef context = UIGraphicsGetCurrentContext();
  CGContextSaveGState(context);

  if (applyGradient) {
    CGContextClipToMask(context, requestedRect, [self.gradient CGImage]);
  }
  [attributedString drawInRect:requestedRect];

  CGContextRestoreGState(context);
}

#pragma mark - Private methods

/// Adds specified attributes to a copy of `attributedString` and sets line
/// break mode to `lineBreakMode`.
- (NSAttributedString*)attributedString:(NSAttributedString*)attributedString
                      withLineBreakMode:(NSLineBreakMode)lineBreakMode {
  // URLs have their text direction set to to LTR (avoids RTL characters
  // making the URL render from right to left, as per RFC 3987 Section 4.1).
  return AttributedStringCopyWithAttributes(
      attributedString, lineBreakMode, self.textAlignment,
      /*force_left_to_right=*/self.displayAsURL);
}

@end
