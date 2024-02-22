// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/fade_truncating_label.h"

#import <CoreText/CoreText.h>

#import <algorithm>

#import "base/i18n/rtl.h"
#import "base/notreached.h"
#import "base/numerics/safe_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/fade_truncating_label+Testing.h"
#import "ios/chrome/browser/shared/ui/util/attributed_string_util.h"

/// The edges where the gradient is applied.
enum class GradientEdge {
  kLeft,   ///< Left edge.
  kRight,  ///< Right edge.
};

namespace {

/// Creates a gradient opacity mask based on direction of `truncate_mode` for
/// `rect`.
UIImage* CreateLinearGradient(CGRect rect, GradientEdge gradient_edge) {
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
  switch (gradient_edge) {
    case GradientEdge::kLeft: {
      CGFloat start_x = minX + fade_width;
      CGPoint start_point = CGPointMake(start_x, CGRectGetMidY(rect));
      CGPoint end_point = CGPointMake(minX, CGRectGetMidY(rect));
      CGContextDrawLinearGradient(context, gradient, start_point, end_point, 0);
      break;
    }
    case GradientEdge::kRight: {
      CGFloat start_x = maxX - fade_width;
      CGPoint start_point = CGPointMake(start_x, CGRectGetMidY(rect));
      CGPoint end_point = CGPointMake(maxX, CGRectGetMidY(rect));
      CGContextDrawLinearGradient(context, gradient, start_point, end_point, 0);
      break;
    }
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

@end

@implementation FadeTruncatingLabel {
  /// The edge where the gradient is applied.
  GradientEdge _gradientEdge;
  /// Current text direction.
  base::i18n::TextDirection _textDirection;
}

- (void)setup {
  self.backgroundColor = [UIColor clearColor];
}

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.lineBreakMode = NSLineBreakByClipping;
    self.lineSpacing = 0;
    _gradientEdge = GradientEdge::kRight;
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
    const CGRect rect =
        CGRectMake(0, 0, self.bounds.size.width, self.bounds.size.height);
    self.gradient = CreateLinearGradient(rect, _gradientEdge);
  }
}

- (void)setText:(NSString*)text {
  [super setText:text];
  [self updateTextDirection];
}

- (void)setAttributedText:(NSAttributedString*)attributedText {
  [super setAttributedText:attributedText];
  [self updateTextDirection];
}

- (void)setTextAlignmentFollowsTextDirection:
    (BOOL)textAlignmentFollowsTextDirection {
  _textAlignmentFollowsTextDirection = textAlignmentFollowsTextDirection;
  if (_textAlignmentFollowsTextDirection) {
    if (_textDirection == base::i18n::RIGHT_TO_LEFT) {
      self.textAlignment = NSTextAlignmentRight;
    } else {
      self.textAlignment = NSTextAlignmentLeft;
    }
  } else {
    self.textAlignment = NSTextAlignmentNatural;
  }
}

#pragma mark - Private

/// Updates the text direction and invalidate the gradient if needed.
- (void)updateTextDirection {
  base::i18n::TextDirection textDirection =
      base::i18n::GetStringDirection(base::SysNSStringToUTF16(self.text));
  if (textDirection != _textDirection) {
    _gradientEdge = textDirection == base::i18n::RIGHT_TO_LEFT
                        ? GradientEdge::kLeft
                        : GradientEdge::kRight;
    self.gradient = nil;
    if (self.textAlignmentFollowsTextDirection) {
      if (textDirection == base::i18n::RIGHT_TO_LEFT) {
        self.textAlignment = NSTextAlignmentRight;
      } else {
        self.textAlignment = NSTextAlignmentLeft;
      }
    }
  }
  _textDirection = textDirection;
}

#pragma mark - Text Drawing

/// Draws `attributedText` with a maximum of `numberOfLines` lines in
/// `requestedRect`.
- (void)drawTextInRect:(CGRect)requestedRect {
  const CGFloat lineHeight = self.font.lineHeight;
  if (!lineHeight || !self.attributedText || CGRectIsEmpty(requestedRect)) {
    return;
  }

  // Force NSLineBreakByWordWrapping to be able to draw multiple lines.
  NSAttributedString* wrappingString =
      [self attributedString:self.attributedText
           withLineBreakMode:NSLineBreakByWordWrapping];

  NSArray<NSValue*>* stringRangeForLines =
      StringRangeInLines(wrappingString, requestedRect.size.width);

  // Like UILabel, always draw a minimum of one line even if there is not enough
  // vertical space.
  NSInteger availableLineCount =
      MAX(1, floor(requestedRect.size.height / lineHeight));

  const NSInteger maxAvailableLineCount =
      self.numberOfLines ? self.numberOfLines : INT_MAX;
  availableLineCount = MIN(availableLineCount, maxAvailableLineCount);

  const NSInteger stringLineCount =
      base::checked_cast<NSInteger>(stringRangeForLines.count);

  const BOOL applyGradient = availableLineCount < stringLineCount;

  const NSInteger lineCount = MIN(availableLineCount, stringLineCount);
  if (lineCount <= 0) {
    return;
  }

  const CGFloat lineSpacing = self.lineSpacing;
  const CGFloat totalLineSpacing = MAX(lineCount - 1, 0) * lineSpacing;
  // Offset to vertical center the text.
  const CGFloat verticalOffset =
      (requestedRect.size.height - lineCount * lineHeight - totalLineSpacing) /
      2;
  const NSInteger lastLine = lineCount - 1;

  /* Draw every line before last line. */
  for (int i = 0; i < lastLine; ++i) {
    const CGRect lineRect =
        CGRectMake(requestedRect.origin.x,
                   requestedRect.origin.y + i * (lineHeight + lineSpacing) +
                       verticalOffset,
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
  const CGRect lastLineRect =
      CGRectMake(requestedRect.origin.x,
                 requestedRect.origin.y +
                     lastLine * (lineHeight + lineSpacing) + verticalOffset,
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
      _textDirection == base::i18n::RIGHT_TO_LEFT
          ? MAX(lastLineString.size.width - lastLineRect.size.width, 0)
          : 0.0;
  [self drawAttributedString:lastLineString
                      inRect:lastLineRect
               applyGradient:applyGradient
             alignmentOffset:rtlOffset];
}

/// Computes the bounding rect necessary to draw text in `bounds` limited to
/// `numberOfLines`.
- (CGRect)textRectForBounds:(CGRect)bounds
     limitedToNumberOfLines:(NSInteger)numberOfLines {
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
  const CGFloat totalLineSpacing =
      MAX((numberOfLinesToDraw - 1), 0) * self.lineSpacing;

  const CGFloat boundingWidth =
      MIN(ceil(singleLineStringSize.width), bounds.size.width);
  CGFloat boundingHeight = ceil(
      singleLineStringSize.height * numberOfLinesToDraw + totalLineSpacing);
  boundingHeight = MIN(boundingHeight, bounds.size.height);
  const CGRect boundingRect = CGRectMake(bounds.origin.x, bounds.origin.y,
                                         boundingWidth, boundingHeight);
  return boundingRect;
}

#pragma mark Text Drawing Private

/// Draws `attributedString` in `requestedRect`.
/// `applyGradient`: Whether gradient should be applied when drawing the text.
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
