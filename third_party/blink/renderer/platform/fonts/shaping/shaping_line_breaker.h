// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPING_LINE_BREAKER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPING_LINE_BREAKER_H_

#include "third_party/blink/renderer/platform/fonts/shaping/run_segmenter.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class ShapeResult;
class ShapeResultView;
class Hyphenation;
class LazyLineBreakIterator;
enum class LineBreakType;

// Shapes a line of text by finding the ideal break position as indicated by the
// available space and the shape results for the entire paragraph. Once an ideal
// break position has been found the text is scanned backwards until a valid and
// and appropriate break opportunity is identified. Unless the break opportunity
// is at a safe-to-break boundary (as identified by HarfBuzz) the beginning and/
// or end of the line is reshaped to account for differences caused by breaking.
//
// This allows for significantly faster and more efficient line breaking by only
// reshaping when absolutely necessarily and by only evaluating likely candidate
// break opportunities instead of measuring and evaluating all possible options.
class PLATFORM_EXPORT ShapingLineBreaker final {
  STACK_ALLOCATED();

 public:
  // Callback function to reshape line edges.
  //
  // std::function is forbidden in Chromium and base::Callback is way too
  // expensive so we resort to a good old function pointer instead.
  using ShapeCallback = scoped_refptr<ShapeResult> (*)(void* context,
                                                       unsigned start,
                                                       unsigned end);

  // Construct a ShapingLineBreaker.
  ShapingLineBreaker(scoped_refptr<const ShapeResult> result,
                     const LazyLineBreakIterator* break_iterator,
                     const Hyphenation* hyphenation,
                     ShapeCallback shape_callback,
                     void* shape_callback_context);

  // Represents details of the result of |ShapeLine()|.
  struct Result {
    STACK_ALLOCATED();

   public:
    // Indicates the resulting break offset.
    unsigned break_offset;

    // True if there were no break opportunities that can fit. When this is
    // false, the result width should be smaller than or equal to the available
    // space.
    bool is_overflow;

    // True if the break is hyphenated, either by automatic hyphenation or
    // soft-hyphen characters.
    // The hyphen glyph is not included in the |ShapeResult|, and that appending
    // a hyphen glyph may overflow the specified available space.
    bool is_hyphenated;
  };

  // Shapes a line of text by finding a valid and appropriate break opportunity
  // based on the shaping results for the entire paragraph.
  enum Options {
    kDefaultOptions = 0,
    // Disable reshpaing the start edge even if the start offset is not safe-
    // to-break. Set if this is not at the start edge of a wrapped line.
    kDontReshapeStart = 1 << 0,
    // Disable reshaping the end edge if it is at a breakable space, even if it
    // is not safe-to-break. Good for performance if accurate width is not
    // critical.
    kDontReshapeEndIfAtSpace = 1 << 1,
    // Returns nullptr if this line overflows. When the word is very long, such
    // as URL or data, creating ShapeResult is expensive. Set this option to
    // suppress if ShapeResult is not needed when this line overflows.
    kNoResultIfOverflow = 1 << 2,
  };
  scoped_refptr<const ShapeResultView> ShapeLine(unsigned start_offset,
                                                 LayoutUnit available_space,
                                                 unsigned options,
                                                 Result* result_out);
  scoped_refptr<const ShapeResultView> ShapeLine(unsigned start_offset,
                                                 LayoutUnit available_space,
                                                 Result* result_out) {
    return ShapeLine(start_offset, available_space, kDefaultOptions,
                     result_out);
  }

  // Disable breaking at soft hyphens (U+00AD).
  bool IsSoftHyphenEnabled() const { return is_soft_hyphen_enabled_; }
  void DisableSoftHyphen() { is_soft_hyphen_enabled_ = false; }

 private:
  const String& GetText() const;

  // Represents a break opportunity offset and its properties.
  struct BreakOpportunity {
    STACK_ALLOCATED();

   public:
    unsigned offset;
    bool is_hyphenated;
  };
  BreakOpportunity PreviousBreakOpportunity(unsigned offset,
                                            unsigned start) const;
  BreakOpportunity NextBreakOpportunity(unsigned offset,
                                        unsigned start,
                                        unsigned len) const;
  BreakOpportunity Hyphenate(unsigned offset,
                             unsigned start,
                             bool backwards) const;
  unsigned Hyphenate(unsigned offset,
                     unsigned word_start,
                     unsigned word_end,
                     bool backwards) const;

  scoped_refptr<ShapeResult> Shape(unsigned start, unsigned end) {
    return (*shape_callback_)(shape_callback_context_, start, end);
  }
  scoped_refptr<const ShapeResultView> ShapeToEnd(unsigned start,
                                                  unsigned first_safe,
                                                  unsigned range_start,
                                                  unsigned range_end);

  const ShapeCallback shape_callback_;
  void* shape_callback_context_;
  scoped_refptr<const ShapeResult> result_;
  const LazyLineBreakIterator* break_iterator_;
  const Hyphenation* hyphenation_;
  bool is_soft_hyphen_enabled_;

  friend class ShapingLineBreakerTest;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPING_LINE_BREAKER_H_
