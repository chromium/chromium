// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shaping_line_breaker.h"

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"

namespace blink {

ShapingLineBreaker::ShapingLineBreaker(
    scoped_refptr<const ShapeResult> result,
    const LazyLineBreakIterator* break_iterator,
    const Hyphenation* hyphenation,
    ShapeCallback shape_callback,
    void* shape_callback_context)
    : shape_callback_(shape_callback),
      shape_callback_context_(shape_callback_context),
      result_(result),
      break_iterator_(break_iterator),
      hyphenation_(hyphenation),
      is_soft_hyphen_enabled_(true) {
  // Line breaking performance relies on high-performance x-position to
  // character offset lookup. Ensure that the desired cache has been computed.
  DCHECK(result_);
  result_->EnsurePositionData();
}

namespace {

// ShapingLineBreaker computes using visual positions. This function flips
// logical advance to visual, or vice versa.
inline LayoutUnit FlipRtl(LayoutUnit value, TextDirection direction) {
  return IsLtr(direction) ? value : -value;
}

inline float FlipRtl(float value, TextDirection direction) {
  return IsLtr(direction) ? value : -value;
}

inline bool IsBreakableSpace(UChar ch) {
  return LazyLineBreakIterator::IsBreakableSpace(ch);
}

bool IsAllSpaces(const String& text, unsigned start, unsigned end) {
  return StringView(text, start, end - start)
      .IsAllSpecialCharacters<LazyLineBreakIterator::IsBreakableSpace>();
}

bool ShouldHyphenate(const String& text, unsigned start, unsigned end) {
  // Do not hyphenate the last word in a paragraph, except when it's a single
  // word paragraph.
  if (IsAllSpaces(text, end, text.length()))
    return IsAllSpaces(text, 0, start);
  return true;
}

inline void CheckBreakOffset(unsigned offset, unsigned start, unsigned end) {
  // It is critical to move the offset forward, or NGLineBreaker may keep adding
  // NGInlineItemResult until all the memory is consumed.
  CHECK_GT(offset, start);
  // The offset must be within the given range, or NGLineBreaker will fail to
  // sync item with offset.
  CHECK_LE(offset, end);
}

}  // namespace

inline const String& ShapingLineBreaker::GetText() const {
  return break_iterator_->GetString();
}

unsigned ShapingLineBreaker::Hyphenate(unsigned offset,
                                       unsigned word_start,
                                       unsigned word_end,
                                       bool backwards) const {
  DCHECK(hyphenation_);
  DCHECK_GT(word_end, word_start);
  DCHECK_GE(offset, word_start);
  DCHECK_LE(offset, word_end);
  unsigned word_len = word_end - word_start;
  if (word_len <= Hyphenation::kMinimumSuffixLength)
    return 0;

  const String& text = GetText();
  if (backwards) {
    unsigned before_index = offset - word_start;
    if (before_index <= Hyphenation::kMinimumPrefixLength)
      return 0;
    unsigned prefix_length = hyphenation_->LastHyphenLocation(
        StringView(text, word_start, word_len), before_index);
    DCHECK(!prefix_length || prefix_length < before_index);
    return prefix_length;
  } else {
    unsigned after_index = offset - word_start;
    if (word_len <= after_index + Hyphenation::kMinimumSuffixLength)
      return 0;
    unsigned prefix_length = hyphenation_->FirstHyphenLocation(
        StringView(text, word_start, word_len), after_index);
    DCHECK(!prefix_length || prefix_length > after_index);
    return prefix_length;
  }
}

ShapingLineBreaker::BreakOpportunity ShapingLineBreaker::Hyphenate(
    unsigned offset,
    unsigned start,
    bool backwards) const {
  const String& text = GetText();
  unsigned word_end = break_iterator_->NextBreakOpportunity(offset);
  if (word_end == offset) {
    DCHECK_EQ(offset, break_iterator_->PreviousBreakOpportunity(offset, start));
    return {word_end, false};
  }
  unsigned previous_break_opportunity =
      break_iterator_->PreviousBreakOpportunity(offset, start);
  unsigned word_start = previous_break_opportunity;
  // Skip the leading spaces of this word because the break iterator breaks
  // before spaces.
  while (word_start < text.length() &&
         LazyLineBreakIterator::IsBreakableSpace(text[word_start]))
    word_start++;
  if (offset >= word_start &&
      ShouldHyphenate(text, previous_break_opportunity, word_end)) {
    unsigned prefix_length = Hyphenate(offset, word_start, word_end, backwards);
    if (prefix_length)
      return {word_start + prefix_length, true};
  }
  return {backwards ? previous_break_opportunity : word_end, false};
}

ShapingLineBreaker::BreakOpportunity
ShapingLineBreaker::PreviousBreakOpportunity(unsigned offset,
                                             unsigned start) const {
  if (UNLIKELY(!IsSoftHyphenEnabled())) {
    const String& text = GetText();
    for (;; offset--) {
      offset = break_iterator_->PreviousBreakOpportunity(offset, start);
      if (offset <= start || offset >= text.length() ||
          text[offset - 1] != kSoftHyphenCharacter)
        return {offset, false};
    }
  }

  if (UNLIKELY(hyphenation_))
    return Hyphenate(offset, start, true);

  return {break_iterator_->PreviousBreakOpportunity(offset, start), false};
}

ShapingLineBreaker::BreakOpportunity ShapingLineBreaker::NextBreakOpportunity(
    unsigned offset,
    unsigned start,
    unsigned len) const {
  if (UNLIKELY(!IsSoftHyphenEnabled())) {
    const String& text = GetText();
    for (;; offset++) {
      offset = break_iterator_->NextBreakOpportunity(offset);
      if (offset >= text.length() || text[offset - 1] != kSoftHyphenCharacter)
        return {offset, false};
    }
  }

  if (UNLIKELY(hyphenation_))
    return Hyphenate(offset, start, false);

  return {break_iterator_->NextBreakOpportunity(offset, len), false};
}

// Shapes a line of text by finding a valid and appropriate break opportunity
// based on the shaping results for the entire paragraph. Re-shapes the start
// and end of the line as needed.
//
// Definitions:
//   Candidate break opportunity: Ideal point to break, disregarding line
//                                breaking rules. May be in the middle of a word
//                                or inside a ligature.
//    Valid break opportunity:    A point where a break is allowed according to
//                                the relevant breaking rules.
//    Safe-to-break:              A point where a break may occur without
//                                affecting the rendering or metrics of the
//                                text. Breaking at safe-to-break point does not
//                                require reshaping.
//
// For example:
//   Given the string "Line breaking example", an available space of 100px and a
//   mono-space font where each glyph is 10px wide.
//
//   Line breaking example
//   |        |
//   0       100px
//
//   The candidate (or ideal) break opportunity would be at an offset of 10 as
//   the break would happen at exactly 100px in that case.
//   The previous valid break opportunity though is at an offset of 5.
//   If we further assume that the font kerns with space then even though it's a
//   valid break opportunity reshaping is required as the combined width of the
//   two segments "Line " and "breaking" may be different from "Line breaking".
scoped_refptr<const ShapeResultView> ShapingLineBreaker::ShapeLine(
    unsigned start,
    LayoutUnit available_space,
    unsigned options,
    ShapingLineBreaker::Result* result_out) {
  DCHECK_GE(available_space, LayoutUnit(0));
  unsigned range_start = result_->StartIndex();
  unsigned range_end = result_->EndIndex();
  DCHECK_GE(start, range_start);
  DCHECK_LT(start, range_end);
  result_out->is_overflow = false;
  result_out->is_hyphenated = false;
  const String& text = GetText();

  // The start position in the original shape results.
  float start_position = result_->CachedPositionForOffset(start - range_start);

  // Find a candidate break opportunity by identifying the last offset before
  // exceeding the available space and the determine the closest valid break
  // preceding the candidate.
  TextDirection direction = result_->Direction();
  float end_position = start_position + FlipRtl(available_space, direction);
  DCHECK_GE(FlipRtl(LayoutUnit::FromFloatCeil(end_position - start_position),
                    direction),
            LayoutUnit(0));
  unsigned candidate_break =
      result_->CachedOffsetForPosition(end_position) + range_start;

  unsigned first_safe = (options & kDontReshapeStart)
                            ? start
                            : result_->CachedNextSafeToBreakOffset(start);
  DCHECK_GE(first_safe, start);
  if (candidate_break >= range_end) {
    // The |result_| does not have glyphs to fill the available space,
    // and thus unable to compute. Return the result up to range_end.
    DCHECK_EQ(candidate_break, range_end);
    result_out->break_offset = range_end;
    return ShapeToEnd(start, first_safe, range_start, range_end);
  }

  // candidate_break should be >= start, but rounding errors can chime in when
  // comparing floats. See ShapeLineZeroAvailableWidth on Linux/Mac.
  candidate_break = std::max(candidate_break, start);

  // If there are no break opportunity before candidate_break, overflow.
  // Find the next break opportunity after the candidate_break.
  BreakOpportunity break_opportunity =
      PreviousBreakOpportunity(candidate_break, start);
  result_out->is_overflow = break_opportunity.offset <= start;
  if (result_out->is_overflow) {
    if (options & kNoResultIfOverflow)
      return nullptr;
    // No need to scan past range_end for a break oppertunity.
    break_opportunity = NextBreakOpportunity(
        std::max(candidate_break, start + 1), start, range_end);
    // |range_end| may not be a break opportunity, but this function cannot
    // measure beyond it.
    if (break_opportunity.offset >= range_end) {
      result_out->break_offset = range_end;
      return ShapeToEnd(start, first_safe, range_start, range_end);
    }
  }
  CheckBreakOffset(break_opportunity.offset, start, range_end);

  // If the start offset is not at a safe-to-break boundary the content between
  // the start and the next safe-to-break boundary needs to be reshaped and the
  // available space adjusted to take the reshaping into account.
  scoped_refptr<const ShapeResult> line_start_result;
  if (first_safe != start) {
    if (first_safe >= break_opportunity.offset) {
      // There is no safe-to-break, reshape the whole range.
      result_out->break_offset = break_opportunity.offset;
      CheckBreakOffset(result_out->break_offset, start, range_end);
      return ShapeResultView::Create(
          Shape(start, break_opportunity.offset).get());
    }
    float first_safe_position =
        result_->CachedPositionForOffset(first_safe - range_start);
    LayoutUnit original_width = LayoutUnit::FromFloatCeil(
        FlipRtl(first_safe_position - start_position, direction));
    line_start_result = Shape(start, first_safe);
    available_space += line_start_result->SnappedWidth() - original_width;
  }
  DCHECK_GE(first_safe, start);
  DCHECK_LE(first_safe, break_opportunity.offset);

  scoped_refptr<const ShapeResult> line_end_result;
  unsigned last_safe = break_opportunity.offset;
  bool reshape_line_end = true;
  if (options & kDontReshapeEndIfAtSpace) {
    if (IsBreakableSpace(text[break_opportunity.offset]))
      reshape_line_end = false;
  }
  if (reshape_line_end) {
    // If the previous valid break opportunity is not at a safe-to-break
    // boundary reshape between the safe-to-break offset and the valid break
    // offset. If the resulting width exceeds the available space the
    // preceding boundary is tried until the available space is sufficient.
    while (true) {
      DCHECK_LE(start, break_opportunity.offset);
      last_safe =
          result_->CachedPreviousSafeToBreakOffset(break_opportunity.offset);
      // No need to reshape the line end because this opportunity is safe.
      if (last_safe == break_opportunity.offset)
        break;
      if (UNLIKELY(last_safe > break_opportunity.offset)) {
        // TODO(crbug.com/1787026): This should not happen, but we see crashes.
        NOTREACHED();
        break;
      }

      // Moved the opportunity back enough to require reshaping the whole line.
      if (UNLIKELY(last_safe < first_safe)) {
        DCHECK_LT(last_safe, start);
        last_safe = start;
        line_start_result = nullptr;
      }

      // If previously determined to let it overflow, reshape the line end.
      DCHECK_LE(break_opportunity.offset, range_end);
      if (UNLIKELY(result_out->is_overflow)) {
        line_end_result = Shape(last_safe, break_opportunity.offset);
        break;
      }

      // Check if this opportunity can fit after reshaping the line end.
      float safe_position =
          result_->CachedPositionForOffset(last_safe - range_start);
      line_end_result = Shape(last_safe, break_opportunity.offset);
      if (line_end_result->Width() <=
          FlipRtl(end_position - safe_position, direction))
        break;

      // Doesn't fit after the reshape. Try the previous break opportunity.
      line_end_result = nullptr;
      break_opportunity =
          PreviousBreakOpportunity(break_opportunity.offset - 1, start);
      if (break_opportunity.offset > start)
        continue;

      // No suitable break opportunity, not exceeding the available space,
      // found. Any break opportunities beyond candidate_break won't fit
      // either because the ShapeResult has the full context.
      // This line will overflow, but there are multiple choices to break,
      // because none can fit. The one after candidate_break is better for
      // ligatures, but the one before is better for kernings.
      result_out->is_overflow = true;
      break_opportunity = PreviousBreakOpportunity(candidate_break, start);
      if (break_opportunity.offset <= start) {
        break_opportunity = NextBreakOpportunity(
            std::max(candidate_break, start + 1), start, range_end);
        if (break_opportunity.offset >= range_end) {
          result_out->break_offset = range_end;
          return ShapeToEnd(start, first_safe, range_start, range_end);
        }
      }
      // Loop once more to compute last_safe for the new break opportunity.
    }
  }
  // It is critical to move forward, or callers may end up in an infinite loop.
  CheckBreakOffset(break_opportunity.offset, start, range_end);
  DCHECK_GE(break_opportunity.offset, last_safe);
  DCHECK_EQ(break_opportunity.offset - start,
            (line_start_result ? line_start_result->NumCharacters() : 0) +
                (last_safe > first_safe ? last_safe - first_safe : 0) +
                (line_end_result ? line_end_result->NumCharacters() : 0));

  // Create shape results for the line by copying from the re-shaped result (if
  // reshaping was needed) and the original shape results.
  ShapeResultView::Segment segments[3];
  unsigned max_length = std::numeric_limits<unsigned>::max();
  unsigned count = 0;
  if (line_start_result)
    segments[count++] = {line_start_result.get(), 0, max_length};
  if (last_safe > first_safe)
    segments[count++] = {result_.get(), first_safe, last_safe};
  if (line_end_result)
    segments[count++] = {line_end_result.get(), last_safe, max_length};
  auto line_result = ShapeResultView::Create(&segments[0], count);
  DCHECK_EQ(break_opportunity.offset - start, line_result->NumCharacters());

  result_out->break_offset = break_opportunity.offset;
  result_out->is_hyphenated =
      break_opportunity.is_hyphenated ||
      text[break_opportunity.offset - 1] == kSoftHyphenCharacter;
  return line_result;
}

// Shape from the specified offset to the end of the ShapeResult.
// If |start| is safe-to-break, this copies the subset of the result.
scoped_refptr<const ShapeResultView> ShapingLineBreaker::ShapeToEnd(
    unsigned start,
    unsigned first_safe,
    unsigned range_start,
    unsigned range_end) {
  DCHECK(result_);
  DCHECK_EQ(range_start, result_->StartIndex());
  DCHECK_EQ(range_end, result_->EndIndex());
  DCHECK_GE(start, range_start);
  DCHECK_LT(start, range_end);
  DCHECK_GE(first_safe, start);

  // If |start| is at the start of the range the entire result object may be
  // reused, which avoids the sub-range logic and bounds computation.
  if (start == range_start)
    return ShapeResultView::Create(result_.get());

  // If |start| is safe-to-break, no reshape is needed.
  if (start == first_safe)
    return ShapeResultView::Create(result_.get(), start, range_end);

  // If no safe-to-break offset is found in range, reshape the entire range.
  if (first_safe >= range_end) {
    scoped_refptr<ShapeResult> line_result = Shape(start, range_end);
    return ShapeResultView::Create(line_result.get());
  }

  // Otherwise reshape to |first_safe|, then copy the rest.
  scoped_refptr<ShapeResult> line_start = Shape(start, first_safe);
  ShapeResultView::Segment segments[2] = {
      {line_start.get(), 0, std::numeric_limits<unsigned>::max()},
      {result_.get(), first_safe, range_end}};
  return ShapeResultView::Create(&segments[0], 2);
}

}  // namespace blink
