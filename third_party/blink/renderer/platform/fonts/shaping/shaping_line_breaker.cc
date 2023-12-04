// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shaping_line_breaker.h"

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/fonts/shaping/text_auto_space.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"

namespace blink {

ShapingLineBreaker::ShapingLineBreaker(
    scoped_refptr<const ShapeResult> result,
    const LazyLineBreakIterator* break_iterator,
    const Hyphenation* hyphenation)
    : result_(result),
      break_iterator_(break_iterator),
      hyphenation_(hyphenation) {
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
  return LazyLineBreakIterator::IsBreakableSpace(ch) ||
         Character::IsOtherSpaceSeparator(ch);
}

bool IsAllSpaces(const String& text, unsigned start, unsigned end) {
  return StringView(text, start, end - start)
      .IsAllSpecialCharacters<IsBreakableSpace>();
}

bool ShouldHyphenate(const String& text,
                     unsigned word_start,
                     unsigned word_end,
                     unsigned line_start) {
  // If this is the first word in this line, allow to hyphenate. Otherwise the
  // word will overflow.
  if (word_start <= line_start)
    return true;
  // Do not hyphenate the last word in a paragraph, except when it's a single
  // word paragraph.
  if (IsAllSpaces(text, word_end, text.length()))
    return IsAllSpaces(text, 0, word_start);
  return true;
}

inline void CheckBreakOffset(unsigned offset, unsigned start, unsigned end) {
  // It is critical to move the offset forward, or LineBreaker may keep adding
  // InlineItemResult until all the memory is consumed.
  CHECK_GT(offset, start);
  // The offset must be within the given range, or LineBreaker will fail to
  // sync item with offset.
  CHECK_LE(offset, end);
}

unsigned FindNonHangableEnd(const String& text, unsigned candidate) {
  DCHECK_LT(candidate, text.length());
  DCHECK(IsBreakableSpace(text[candidate]));

  // Looking for the non-hangable run end
  unsigned non_hangable_end = candidate;
  while (non_hangable_end > 0) {
    if (!IsBreakableSpace(text[--non_hangable_end]))
      return non_hangable_end + 1;
  }
  return non_hangable_end;
}

}  // namespace

inline const String& ShapingLineBreaker::GetText() const {
  return break_iterator_->GetString();
}

inline ShapingLineBreaker::EdgeOffset ShapingLineBreaker::FirstSafeOffset(
    unsigned start) const {
  if (!IsStartOfWrappedLine(start)) {
    // When it's not at the start of a wrapped line, disable reshaping.
    return {start};
  }
  if (UNLIKELY(RuntimeEnabledFeatures::CSSTextSpacingTrimEnabled()) &&
      // TODO(crbug.com/1463891): `MaybeOpen` is likely to hit the performance
      // for non-CJK documents. We should try harder not to require reshaping.
      UNLIKELY(HanKerning::MaybeOpen(GetText()[start])) &&
      text_spacing_trim_ == TextSpacingTrim::kSpaceFirst) {
    // `HanKerning` wants to apply kerning to `kOpen` characters at the start of
    // the line. Reshape it to resolve the `SimpleFontData` and apply
    // `HanKerning` if applicable. Note, it may not actually apply, if the font
    // doesn't have features or the glyph isn't fullwidth.
    if (++start < result_->EndIndex()) {
      return {result_->CachedNextSafeToBreakOffset(start), true};
    }
    return {start, true};
  }
  return {result_->CachedNextSafeToBreakOffset(start)};
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
  if (word_len < hyphenation_->MinWordLength())
    return 0;

  const String& text = GetText();
  const StringView word(text, word_start, word_len);
  const unsigned word_offset = offset - word_start;
  if (backwards) {
    if (word_offset < hyphenation_->MinPrefixLength())
      return 0;
    unsigned prefix_length =
        hyphenation_->LastHyphenLocation(word, word_offset + 1);
    DCHECK(!prefix_length || prefix_length <= word_offset);
    return prefix_length;
  } else {
    if (word_len - word_offset < hyphenation_->MinSuffixLength())
      return 0;
    unsigned prefix_length = hyphenation_->FirstHyphenLocation(
        word, word_offset ? word_offset - 1 : 0);
    DCHECK(!prefix_length || prefix_length >= word_offset);
    return prefix_length;
  }
}

ShapingLineBreaker::BreakOpportunity ShapingLineBreaker::Hyphenate(
    unsigned offset,
    unsigned start,
    bool backwards) const {
  const String& text = GetText();
  unsigned word_end = break_iterator_->NextBreakOpportunity(offset);
  if (word_end != offset && IsBreakableSpace(text[word_end - 1]))
    word_end = std::max(offset, FindNonHangableEnd(text, word_end - 1));
  if (word_end == offset) {
    DCHECK(IsBreakableSpace(text[offset]) ||
           offset == break_iterator_->PreviousBreakOpportunity(offset, start));
    return {word_end, false};
  }
  unsigned previous_break_opportunity =
      break_iterator_->PreviousBreakOpportunity(offset, start);
  unsigned word_start = previous_break_opportunity;
  // Skip the leading spaces of this word because the break iterator breaks
  // before spaces.
  // TODO (jfernandez): This is no longer true, so we should remove this code.
  while (word_start < text.length() &&
         LazyLineBreakIterator::IsBreakableSpace(text[word_start]))
    word_start++;
  if (offset >= word_start &&
      ShouldHyphenate(text, previous_break_opportunity, word_end, start)) {
    unsigned prefix_length = Hyphenate(offset, word_start, word_end, backwards);
    if (prefix_length)
      return {word_start + prefix_length, true};
  }
  return {backwards ? previous_break_opportunity : word_end, false};
}

ShapingLineBreaker::BreakOpportunity
ShapingLineBreaker::PreviousBreakOpportunity(unsigned offset,
                                             unsigned start) const {
  if (UNLIKELY(hyphenation_))
    return Hyphenate(offset, start, true);

  // If the break opportunity is preceded by trailing spaces, find the
  // end of non-hangable character (i.e., start of the space run).
  const String& text = GetText();
  unsigned break_offset =
      break_iterator_->PreviousBreakOpportunity(offset, start);
  if (IsBreakableSpace(text[break_offset - 1]))
    return {break_offset, FindNonHangableEnd(text, break_offset - 1), false};

  return {break_offset, false};
}

ShapingLineBreaker::BreakOpportunity ShapingLineBreaker::NextBreakOpportunity(
    unsigned offset,
    unsigned start,
    unsigned len) const {
  if (UNLIKELY(hyphenation_))
    return Hyphenate(offset, start, false);

  // We should also find the beginning of the space run to find the
  // end of non-hangable character (i.e., start of the space run),
  // which may be useful to avoid reshaping.
  const String& text = GetText();
  unsigned break_offset = break_iterator_->NextBreakOpportunity(offset, len);
  if (IsBreakableSpace(text[break_offset - 1]))
    return {break_offset, FindNonHangableEnd(text, break_offset - 1), false};

  return {break_offset, false};
}

inline void ShapingLineBreaker::SetBreakOffset(unsigned break_offset,
                                               const String& text,
                                               Result* result) {
  result->break_offset = break_offset;
  result->is_hyphenated =
      text[result->break_offset - 1] == kSoftHyphenCharacter;
}

inline void ShapingLineBreaker::SetBreakOffset(
    const BreakOpportunity& break_opportunity,
    const String& text,
    Result* result) {
  result->break_offset = break_opportunity.offset;
  result->is_hyphenated =
      break_opportunity.is_hyphenated ||
      text[result->break_offset - 1] == kSoftHyphenCharacter;
  result->non_hangable_run_end = break_opportunity.non_hangable_run_end;
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
    ShapingLineBreaker::Result* result_out) {
  DCHECK_GE(available_space, LayoutUnit(0));
  const unsigned range_start = result_->StartIndex();
  const unsigned range_end = result_->EndIndex();
  DCHECK_GE(start, range_start);
  DCHECK_LT(start, range_end);
  result_out->is_overflow = false;
  result_out->is_hyphenated = false;
  result_out->has_trailing_spaces = false;
  const String& text = GetText();
  const bool is_break_after_any_space =
      break_iterator_->BreakSpace() == BreakSpaceType::kAfterEverySpace;

  // Compute if the start is safe-to-break.
  const EdgeOffset first_safe = FirstSafeOffset(start);
  DCHECK_GE(first_safe.offset, start);

  // The start position in the original shape results.
  const float start_position =
      result_->CachedPositionForOffset(start - range_start);

  // Find a candidate break opportunity by identifying the last offset before
  // exceeding the available space and the determine the closest valid break
  // preceding the candidate.
  const TextDirection direction = result_->Direction();
  const float end_position =
      start_position + FlipRtl(available_space, direction);
  DCHECK_GE(FlipRtl(LayoutUnit::FromFloatCeil(end_position - start_position),
                    direction),
            LayoutUnit(0));
  unsigned candidate_break =
      result_->CachedOffsetForPosition(end_position) + range_start;
  if (candidate_break < range_end &&
      UNLIKELY(result_->HasAutoSpacingAfter(candidate_break))) {
    // If there's an auto-space after the `candidate_break`, check if it can fit
    // without the auto-space.
    candidate_break =
        result_->AdjustOffsetForAutoSpacing(candidate_break, end_position);
  }
  if (candidate_break >= range_end) {
    // The |result_| does not have glyphs to fill the available space,
    // and thus unable to compute. Return the result up to range_end.
    DCHECK_EQ(candidate_break, range_end);
    SetBreakOffset(range_end, text, result_out);
    return ShapeToEnd(start, first_safe, range_start, range_end);
  }

  // candidate_break should be >= start, but rounding errors can chime in when
  // comparing floats. See ShapeLineZeroAvailableWidth on Linux/Mac.
  candidate_break = std::max(candidate_break, start);

  // If we are in the middle of a trailing space sequence, which are
  // defined by the UAX#14 spec as Break After (A) class, we should
  // look for breaking opportunityes after the end of the sequence.
  // https://www.unicode.org/reports/tr14/#BA
  // TODO(jfernandez): if break-spaces, do special handling.
  BreakOpportunity break_opportunity;
  const bool use_previous_break_opportunity =
      !IsBreakableSpace(text[candidate_break]) || is_break_after_any_space;
  if (use_previous_break_opportunity) {
    break_opportunity = PreviousBreakOpportunity(candidate_break, start);

    // Overflow if there are no break opportunity before candidate_break.
    // Find the next break opportunity after the candidate_break.
    // TODO: (jfernandez): Maybe also non_hangable_run_end <= start ?
    result_out->is_overflow = break_opportunity.offset <= start;
    if (result_out->is_overflow) {
      DCHECK(use_previous_break_opportunity);
      if (no_result_if_overflow_) {
        return nullptr;
      }
      // No need to scan past range_end for a break opportunity.
      break_opportunity = NextBreakOpportunity(
          std::max(candidate_break, start + 1), start, range_end);
    }
  } else {
    break_opportunity = NextBreakOpportunity(
        std::max(candidate_break, start + 1), start, range_end);
    DCHECK_GT(break_opportunity.offset, start);
    DCHECK(!result_out->is_overflow);

    // If we were looking for a next break opportunity and found one that is
    // after candidate_break but doesn't have a corresponding non-hangable run
    // that spans to at least candidate_break, that would be an overflow.
    // However, there might still be break opportunities before candidate_break
    // that we haven't checked, so we look for them first.
    if (break_opportunity.offset > candidate_break &&
        (!break_opportunity.non_hangable_run_end ||
         *break_opportunity.non_hangable_run_end > candidate_break)) {
      BreakOpportunity previous_opportunity =
          PreviousBreakOpportunity(candidate_break, start);
      if (previous_opportunity.offset > start) {
        break_opportunity = previous_opportunity;
      } else {
        result_out->is_overflow = true;
        if (no_result_if_overflow_) {
          return nullptr;
        }
      }
    }

    // We don't care whether this result contains only spaces if we
    // are breaking after any space. We shouldn't early return either
    // in that case.
    DCHECK(!is_break_after_any_space);
    DCHECK(IsBreakableSpace(text[candidate_break]));
    if (break_opportunity.non_hangable_run_end &&
        break_opportunity.non_hangable_run_end <= start) {
      // TODO (jfenandez): There may be cases where candidate_break is
      // not a breakable space but we also want to early return for
      // triggering the trailing spaces handling
      result_out->has_trailing_spaces = true;
      result_out->break_offset = std::min(range_end, break_opportunity.offset);
      result_out->non_hangable_run_end = break_opportunity.non_hangable_run_end;
#if DCHECK_IS_ON()
      DCHECK(IsAllSpaces(text, start, result_out->break_offset));
#endif
      result_out->is_hyphenated = false;
      return ShapeResultView::Create(result_.get(), start,
                                     result_out->break_offset);
    }
  }

  bool reshape_line_end = true;
  // |range_end| may not be a break opportunity, but this function cannot
  // measure beyond it.
  if (break_opportunity.offset >= range_end) {
    SetBreakOffset(range_end, text, result_out);
    if (result_out->is_overflow)
      return ShapeToEnd(start, first_safe, range_start, range_end);
    break_opportunity.offset = range_end;
    // Avoid re-shape if at the end of the range.
    // eg. <span>abc</span>def ghi
    // then `range_end` is at the end of the `<span>`, while break opportunity
    // is at the space.
    reshape_line_end = false;
    if (break_opportunity.non_hangable_run_end &&
        range_end < break_opportunity.non_hangable_run_end) {
      break_opportunity.non_hangable_run_end = absl::nullopt;
    }
    if (IsBreakableSpace(text[range_end - 1])) {
      break_opportunity.non_hangable_run_end =
          FindNonHangableEnd(text, range_end - 1);
    }
  }

  // We may have options that imply avoiding re-shape.
  // Note: we must evaluate the need of re-shaping the end of the line, before
  // we consider the non-hangable-run-end.
  if (dont_reshape_end_if_at_space_) {
    // If the actual offset is in a breakable-space sequence, we may need to run
    // the re-shape logic and consider the non-hangable-run-end.
    reshape_line_end &= !IsBreakableSpace(text[break_opportunity.offset - 1]);
  }

  // Use the non-hanable-run end as breaking offset (unless we break after eny
  // space)
  if (!is_break_after_any_space && break_opportunity.non_hangable_run_end) {
    break_opportunity.offset =
        std::max(start + 1, *break_opportunity.non_hangable_run_end);
  }
  CheckBreakOffset(break_opportunity.offset, start, range_end);

  // If the start offset is not at a safe-to-break boundary the content between
  // the start and the next safe-to-break boundary needs to be reshaped and the
  // available space adjusted to take the reshaping into account.
  scoped_refptr<const ShapeResult> line_start_result;
  if (first_safe.offset != start) {
    const ShapeOptions options{.han_kerning_start = first_safe.han_kerning};
    if (first_safe.offset >= break_opportunity.offset) {
      // There is no safe-to-break, reshape the whole range.
      SetBreakOffset(break_opportunity, text, result_out);
      CheckBreakOffset(result_out->break_offset, start, range_end);
      return ShapeResultView::Create(
          Shape(start, break_opportunity.offset, options).get());
    }
    float first_safe_position =
        result_->CachedPositionForOffset(first_safe.offset - range_start);
    LayoutUnit original_width = LayoutUnit::FromFloatCeil(
        FlipRtl(first_safe_position - start_position, direction));
    line_start_result = Shape(start, first_safe.offset, options);
    available_space += line_start_result->SnappedWidth() - original_width;
  }
  DCHECK_GE(first_safe.offset, start);
  DCHECK_LE(first_safe.offset, break_opportunity.offset);

  scoped_refptr<const ShapeResult> line_end_result;
  unsigned last_safe = break_opportunity.offset;
  if (reshape_line_end) {
    // If the previous valid break opportunity is not at a safe-to-break
    // boundary reshape between the safe-to-break offset and the valid break
    // offset. If the resulting width exceeds the available space the
    // preceding boundary is tried until the available space is sufficient.
    while (true) {
      DCHECK_LE(start, break_opportunity.offset);
      if (!is_break_after_any_space && break_opportunity.non_hangable_run_end) {
        break_opportunity.offset =
            std::max(start + 1, *break_opportunity.non_hangable_run_end);
      }
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
      if (UNLIKELY(last_safe < first_safe.offset)) {
        DCHECK(last_safe == 0 || last_safe < start);
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
      // TODO (jfernandez): Would be possible to refactor this logic
      // with the one performed prior tp the reshape
      // (FindBreakingOpportuntty() + overflow handling)?
      break_opportunity = PreviousBreakOpportunity(candidate_break, start);
      if (break_opportunity.offset <= start) {
        break_opportunity = NextBreakOpportunity(
            std::max(candidate_break, start + 1), start, range_end);
        if (break_opportunity.offset >= range_end) {
          SetBreakOffset(range_end, text, result_out);
          return ShapeToEnd(start, first_safe, range_start, range_end);
        }
      }
      // Loop once more to compute last_safe for the new break opportunity.
    }
  }

  if (!line_end_result) {
    DCHECK_EQ(break_opportunity.offset, last_safe);
    DCHECK_GT(last_safe, start);
    if (UNLIKELY(result_->HasAutoSpacingBefore(last_safe))) {
      last_safe = result_->CachedPreviousSafeToBreakOffset(last_safe - 1);
      DCHECK_LT(last_safe, break_opportunity.offset);
      line_end_result =
          result_->UnapplyAutoSpacing(last_safe, break_opportunity.offset);
    }
  }

  // It is critical to move forward, or callers may end up in an infinite loop.
  CheckBreakOffset(break_opportunity.offset, start, range_end);
  DCHECK_GE(break_opportunity.offset, last_safe);
  DCHECK_EQ(
      break_opportunity.offset - start,
      (line_start_result ? line_start_result->NumCharacters() : 0) +
          (last_safe > first_safe.offset ? last_safe - first_safe.offset : 0) +
          (line_end_result ? line_end_result->NumCharacters() : 0));
  SetBreakOffset(break_opportunity, text, result_out);

  // Create shape results for the line by copying from the re-shaped result (if
  // reshaping was needed) and the original shape results.
  return ConcatShapeResults(start, break_opportunity.offset, first_safe.offset,
                            last_safe, std::move(line_start_result),
                            std::move(line_end_result));
}
scoped_refptr<const ShapeResultView> ShapingLineBreaker::ConcatShapeResults(
    unsigned start,
    unsigned end,
    unsigned first_safe,
    unsigned last_safe,
    scoped_refptr<const ShapeResult> line_start_result,
    scoped_refptr<const ShapeResult> line_end_result) {
  ShapeResultView::Segment segments[3];
  constexpr unsigned max_length = std::numeric_limits<unsigned>::max();
  unsigned count = 0;
  if (line_start_result) {
    segments[count++] = {line_start_result.get(), 0, max_length};
  }
  if (last_safe > first_safe) {
    segments[count++] = {result_.get(), first_safe, last_safe};
  }
  if (line_end_result) {
    segments[count++] = {line_end_result.get(), last_safe, max_length};
  }
  auto line_result = ShapeResultView::Create({&segments[0], count});
  DCHECK_EQ(end - start, line_result->NumCharacters());
  return line_result;
}

// Shape from the specified offset to the end of the ShapeResult.
// If |start| is safe-to-break, this copies the subset of the result.
scoped_refptr<const ShapeResultView> ShapingLineBreaker::ShapeToEnd(
    unsigned start,
    const EdgeOffset& first_safe,
    unsigned range_start,
    unsigned range_end) {
  DCHECK(result_);
  DCHECK_EQ(range_start, result_->StartIndex());
  DCHECK_EQ(range_end, result_->EndIndex());
  DCHECK_GE(start, range_start);
  DCHECK_LT(start, range_end);
  DCHECK_GE(first_safe.offset, start);

  // If |start| is safe-to-break, no reshape is needed.
  if (start == first_safe.offset) {
    // If |start| is at the start of the range the entire result object may be
    // reused, which avoids the sub-range logic and bounds computation.
    if (start == range_start) {
      return ShapeResultView::Create(result_.get());
    }
    return ShapeResultView::Create(result_.get(), start, range_end);
  }

  // If no safe-to-break offset is found in range, reshape the entire range.
  const ShapeOptions options{.han_kerning_start = first_safe.han_kerning};
  if (first_safe.offset >= range_end) {
    scoped_refptr<ShapeResult> line_result = Shape(start, range_end, options);
    return ShapeResultView::Create(line_result.get());
  }

  // Otherwise reshape to |first_safe|, then copy the rest.
  scoped_refptr<ShapeResult> line_start =
      Shape(start, first_safe.offset, options);
  ShapeResultView::Segment segments[2] = {
      {line_start.get(), 0, std::numeric_limits<unsigned>::max()},
      {result_.get(), first_safe.offset, range_end}};
  return ShapeResultView::Create(segments);
}

scoped_refptr<const ShapeResultView> ShapingLineBreaker::ShapeLineAt(
    unsigned start,
    unsigned end) {
  DCHECK_GT(end, start);

  const EdgeOffset first_safe = FirstSafeOffset(start);
  DCHECK_GE(first_safe.offset, start);
  scoped_refptr<const ShapeResult> line_start_result;
  if (first_safe.offset != start) {
    const ShapeOptions options{.han_kerning_start = first_safe.han_kerning};
    if (first_safe.offset >= end) {
      // There is no safe-to-break, reshape the whole range.
      scoped_refptr<ShapeResult> line_result = Shape(start, end, options);
      return ShapeResultView::Create(line_result.get());
    }
    line_start_result = Shape(start, first_safe.offset, options);
  }

  unsigned last_safe;
  scoped_refptr<const ShapeResult> line_end_result;
  if (dont_reshape_end_if_at_space_ && IsBreakableSpace(GetText()[end - 1])) {
    last_safe = end;
  } else {
    last_safe = result_->CachedPreviousSafeToBreakOffset(end);
    DCHECK_GE(last_safe, first_safe.offset);
    if (last_safe != end) {
      line_end_result = Shape(last_safe, end);
    }
  }

  return ConcatShapeResults(start, end, first_safe.offset, last_safe,
                            std::move(line_start_result),
                            std::move(line_end_result));
}

}  // namespace blink
