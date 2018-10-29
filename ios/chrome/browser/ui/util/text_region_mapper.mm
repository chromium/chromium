// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/text_region_mapper.h"

#import <CoreText/CoreText.h>
#import <QuartzCore/QuartzCore.h>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/util/core_text_util.h"
#import "ios/chrome/browser/ui/util/manual_text_framer.h"
#import "ios/chrome/browser/ui/util/text_frame.h"
#include "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CoreTextRegionMapper ()

// The TextFrame used to calculate rects.
@property(strong, nonatomic, readonly) id<TextFrame> textFrame;

@end

@implementation CoreTextRegionMapper

@synthesize textFrame = _textFrame;

- (instancetype)initWithAttributedString:(NSAttributedString*)string
                                  bounds:(CGRect)bounds {
  if ((self = [super init])) {
    base::ScopedCFTypeRef<CTFrameRef> ctFrame =
        core_text_util::CreateTextFrameForStringInBounds(string, bounds);
    ManualTextFramer* framer =
        [[ManualTextFramer alloc] initWithString:string inBounds:bounds];
    [framer frameText];
    if (core_text_util::IsTextFrameValid(ctFrame, framer, string)) {
      _textFrame = [[CoreTextTextFrame alloc] initWithString:string
                                                      bounds:bounds
                                                       frame:ctFrame];
    } else {
      // Use ManualTextFramer if |ctFrame| is invalid.
      _textFrame = [framer textFrame];
    }
    DCHECK(self.textFrame);
  }
  return self;
}

- (NSArray*)rectsForRange:(NSRange)range {
  NSRange framedRange = self.textFrame.framedRange;
  if (!range.length || range.location + range.length > framedRange.length)
    return @[];

  NSMutableArray* rects = [NSMutableArray array];
  // CoreText uses Quartz coordinates, so they will need to be flipped back to
  // UIKit-space.
  CGAffineTransform transformForCoreText = CGAffineTransformScale(
      CGAffineTransformMakeTranslation(0, self.textFrame.bounds.size.height),
      1.f, -1.f);

  CGFloat ascent = 0.0f;   // height of text above the baseline.
  CGFloat descent = 0.0f;  // height of text below the baseline.

  // Find any parts of the link on each line.
  for (FramedLine* line in self.textFrame.lines) {
    CGFloat lineWidth = static_cast<CGFloat>(
        CTLineGetTypographicBounds(line.line, &ascent, &descent, nullptr));
    if (!lineWidth)
      break;

    NSRange overlap = NSIntersectionRange(range, line.stringRange);
    if (overlap.length > 0) {
      // Some of the range is on this line; find where it starts and ends.
      CFIndex stringOffset =
          [line lineOffsetForStringLocation:overlap.location];
      DCHECK_NE(stringOffset, kCFNotFound);
      CGFloat start =
          CTLineGetOffsetForStringIndex(line.line, stringOffset, nullptr);
      CGFloat end = CTLineGetOffsetForStringIndex(
          line.line, stringOffset + overlap.length, nullptr);
      CGRect flippedRangeRect =
          CGRectMake(line.origin.x + start,
                     line.origin.y - descent,  // Lower extent of text.
                     end - start,              // Length of link text.
                     ascent + descent);        // Total height of text.
      // Flip to UIKit coordinates.
      CGRect rangeRect =
          CGRectApplyAffineTransform(flippedRangeRect, transformForCoreText);
      [rects addObject:[NSValue valueWithCGRect:rangeRect]];
    }
  }
  return rects;
}

@end
