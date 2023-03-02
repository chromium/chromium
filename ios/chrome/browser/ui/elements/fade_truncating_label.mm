// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/elements/fade_truncating_label.h"

#import <CoreText/CoreText.h>
#import <algorithm>

#import "base/notreached.h"
#import "base/numerics/safe_conversions.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/attributed_string_util.h"

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

/// Returns the substring ranges to draw `attributed_string` with lines of
/// `limited_width`.
NSArray<NSValue*>* StringRangeInLines(NSAttributedString* attributed_string,
                                      CGFloat limited_width) {
  NSMutableArray<NSValue*>* line_ranges = [[NSMutableArray alloc] init];
  CTFramesetterRef frame_setter = CTFramesetterCreateWithAttributedString(
      (CFAttributedStringRef)attributed_string);
  UIBezierPath* path = [UIBezierPath
      bezierPathWithRect:CGRectMake(0, 0, limited_width, FLT_MAX)];
  CTFrameRef frame = CTFramesetterCreateFrame(frame_setter, CFRangeMake(0, 0),
                                              path.CGPath, NULL);
  NSArray* lines = CFBridgingRelease(CTFrameGetLines(frame));
  for (id line in lines) {
    CTLineRef line_ref = (__bridge CTLineRef)line;
    CFRange line_range = CTLineGetStringRange(line_ref);
    NSRange range = NSMakeRange(line_range.location, line_range.length);
    [line_ranges addObject:[NSValue valueWithRange:range]];
  }
  CFRelease(frame_setter);
  return line_ranges;
}

}  // namespace

@interface FadeTruncatingLabel ()

// Gradient used to create fade effect. Changes based on view.frame size.
@property(nonatomic, strong) UIImage* gradient;
// /// Returns `YES` if multiline flag is enabled.
@property(nonatomic, assign) BOOL isMultilineEnabled;

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
    _isMultilineEnabled =
        base::FeatureList::IsEnabled(kMultilineFadeTruncatingLabel);
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

  self.isMultilineEnabled =
      base::FeatureList::IsEnabled(kMultilineFadeTruncatingLabel);
  // Cache the fade gradient when the bounds change.
  if (!CGRectIsEmpty(self.bounds) &&
      (!self.gradient ||
       !CGSizeEqualToSize([self.gradient size], self.bounds.size))) {
    const CGRect rect =
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
  if (self.isMultilineEnabled) {
    [self drawMultilineInRect:requestedRect];
  } else {
    NSAttributedString* configuredString =
        [self attributedString:self.attributedText
             withLineBreakMode:self.lineBreakMode];

    // Draw fade gradient mask if `attributedText` is wider than rect.
    const BOOL shouldApplyGradient =
        [self.attributedText size].width > requestedRect.size.width;
    [self drawAttributedString:configuredString
                        inRect:requestedRect
                 applyGradient:shouldApplyGradient
               alignmentOffset:0.0];
  }
}

/// Computes the bounding rect necessary to draw text in `bounds` limited to
/// `numberOfLines`.
- (CGRect)textRectForBounds:(CGRect)bounds
     limitedToNumberOfLines:(NSInteger)numberOfLines {
  if (!self.isMultilineEnabled) {
    return [super textRectForBounds:bounds
             limitedToNumberOfLines:numberOfLines];
  }
  NSInteger maxNumberOfLines = numberOfLines ? numberOfLines : INT_MAX;
  // Force NSLineBreakByWordWrapping to be able to draw multiple lines.
  NSAttributedString* wrappingString =
      [self attributedString:self.attributedText
           withLineBreakMode:NSLineBreakByWordWrapping];
  // Compute the number of lines needed to draw the string with limited width.
  const CGSize wrappingStringSize =
      [wrappingString boundingRectWithSize:CGSizeMake(bounds.size.width, 0)
                                   options:NSStringDrawingUsesLineFragmentOrigin
                                   context:nil]
          .size;

  const CGSize singleLineStringSize = wrappingString.size;
  const NSInteger wrappingStringNumberOfLines =
      round(wrappingStringSize.height / singleLineStringSize.height);
  const NSInteger numberOfLinesToDraw =
      MIN(maxNumberOfLines, wrappingStringNumberOfLines);

  const CGFloat boundingWidth =
      MIN(ceil(singleLineStringSize.width), bounds.size.width);
  CGFloat boundingHeight =
      ceil(singleLineStringSize.height * numberOfLinesToDraw);
  boundingHeight = MIN(boundingHeight, bounds.size.height);
  const CGRect boundingRect = CGRectMake(bounds.origin.x, bounds.origin.y,
                                         boundingWidth, boundingHeight);
  return boundingRect;
}

#pragma mark Text Drawing Private

/// Draws `attributedString` in `requestedRect`.
/// `applyGradient`: Wheter gradient should be applied when drawing the text.
/// `alignmentOffset`: offset added to draw the text on the left of
/// `requestedRect`. Note: with NSLineBreakByClipping the text is always clipped
/// to the right even when the text is aligned to the right, with the offset the
/// text starts to draw on the left of `requestedRect`, this allow the text to
/// end inside of `requestedRect` clipping it on the left.
- (void)drawAttributedString:(NSAttributedString*)attributedString
                      inRect:(CGRect)requestedRect
               applyGradient:(BOOL)applyGradient
             alignmentOffset:(CGFloat)alignmentOffset {
  CGContextRef context = UIGraphicsGetCurrentContext();
  CGContextSaveGState(context);

  if (applyGradient) {
    CGContextClipToMask(context, requestedRect, [self.gradient CGImage]);
  }

  CGRect drawingRect = requestedRect;
  if (alignmentOffset != 0) {
    drawingRect = CGRectMake(
        requestedRect.origin.x - alignmentOffset, requestedRect.origin.y,
        requestedRect.size.width + alignmentOffset, requestedRect.size.height);
  }
  [attributedString drawInRect:drawingRect];

  CGContextRestoreGState(context);
}

/// Draws a maximum of `numberOfLines` lines in `requestedRect`.
- (void)drawMultilineInRect:(CGRect)requestedRect {
  DCHECK(self.isMultilineEnabled);
  // Force NSLineBreakByWordWrapping to be able to draw multiple lines.
  NSAttributedString* wrappingString =
      [self attributedString:self.attributedText
           withLineBreakMode:NSLineBreakByWordWrapping];
  const CGSize wrappingStringSize =
      [wrappingString
          boundingRectWithSize:CGSizeMake(requestedRect.size.width, 0)
                       options:NSStringDrawingUsesLineFragmentOrigin
                       context:nil]
          .size;

  // Apply gradient if the height needed to draw `attributedText` exceeds the
  // available height.
  const BOOL applyGradient =
      floor(wrappingStringSize.height) > floor(requestedRect.size.height);

  NSArray<NSValue*>* stringRangeForLines =
      StringRangeInLines(wrappingString, requestedRect.size.width);
  const CGFloat lineHeight = self.font.lineHeight;
  if (!lineHeight) {
    return;
  }

  // Like UILabel, always draw a minimum of one line even if there is not enough
  // vertical space.
  NSInteger lineCount = MAX(floor(requestedRect.size.height / lineHeight), 1);
  lineCount =
      MIN(lineCount, base::checked_cast<NSInteger>(stringRangeForLines.count));
  if (lineCount <= 0) {
    return;
  }

  // Offset to vertical center the text.
  const CGFloat verticalOffset =
      (requestedRect.size.height - lineCount * lineHeight) / 2;
  const NSInteger lastLine = lineCount - 1;

  /* Draw every line before last line. */
  for (int i = 0; i < lastLine; ++i) {
    const CGRect lineRect =
        CGRectMake(requestedRect.origin.x,
                   requestedRect.origin.y + i * lineHeight + verticalOffset,
                   requestedRect.size.width, lineHeight);
    const NSRange stringRange = stringRangeForLines[i].rangeValue;
    NSAttributedString* subString =
        [wrappingString attributedSubstringFromRange:stringRange];
    [self drawAttributedString:subString
                        inRect:lineRect
                 applyGradient:NO
               alignmentOffset:0.0];
  }

  /*  Draw last line. */
  const CGRect lastLineRect = CGRectMake(
      requestedRect.origin.x,
      requestedRect.origin.y + lastLine * lineHeight + verticalOffset,
      requestedRect.size.width, lineHeight);
  // Last line takes all the remaining text, from start of last line to end of
  // `attributedText`.
  const NSRange lastLineRange =
      NSMakeRange(stringRangeForLines[lastLine].rangeValue.location,
                  wrappingString.length -
                      stringRangeForLines[lastLine].rangeValue.location);
  NSAttributedString* lastLineString =
      [wrappingString attributedSubstringFromRange:lastLineRange];
  // Last line is clipped instead of wrapped.
  lastLineString = [self attributedString:lastLineString
                        withLineBreakMode:NSLineBreakByClipping];
  const CGFloat rtlOffset =
      self.semanticContentAttribute ==
              UISemanticContentAttributeForceRightToLeft
          ? lastLineString.size.width - lastLineRect.size.width
          : 0.0;
  [self drawAttributedString:lastLineString
                      inRect:lastLineRect
               applyGradient:applyGradient
             alignmentOffset:rtlOffset];
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
