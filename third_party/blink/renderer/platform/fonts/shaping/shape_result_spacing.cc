// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"

#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/text/justification_opportunity.h"
#include "third_party/blink/renderer/platform/wtf/text/utf16.h"

namespace blink {

bool ShapeResultSpacing::SetSpacing(const FontDescription& font_description) {
  return SetSpacing(TextRunLayoutUnit(font_description.LetterSpacing()),
                    TextRunLayoutUnit(font_description.WordSpacing()));
}

bool ShapeResultSpacing::SetSpacing(TextRunLayoutUnit letter_spacing,
                                    TextRunLayoutUnit word_spacing) {
  if (!letter_spacing && !word_spacing) {
    has_spacing_ = false;
    return false;
  }

  letter_spacing_ = letter_spacing;
  word_spacing_ = word_spacing;
  DCHECK(!normalize_space_);
  allow_tabs_ = true;
  has_spacing_ = true;
  return true;
}

void ShapeResultSpacing::SetExpansion(TextJustify method,
                                      InlineLayoutUnit expansion,
                                      TextDirection direction,
                                      bool allows_leading_expansion,
                                      bool allows_trailing_expansion) {
  DCHECK_GT(expansion, InlineLayoutUnit());
  justification_method_ = method;
  expansion_ = expansion;
  ComputeExpansion(allows_leading_expansion, allows_trailing_expansion,
                   direction);
  has_spacing_ |= HasExpansion();
}

void ShapeResultSpacing::SetSpacing(const FontDescription& font_description,
                                    bool normalize_space) {
  if (SetSpacing(font_description)) {
    normalize_space_ = normalize_space;
    allow_tabs_ = false;
  }
}

void ShapeResultSpacing::ComputeExpansion(bool allows_leading_expansion,
                                          bool allows_trailing_expansion,
                                          TextDirection direction) {
  DCHECK_GT(expansion_, InlineLayoutUnit());

  is_after_expansion_ = !allows_leading_expansion;
  bool is_after_expansion = is_after_expansion_;
  if (text_.Is8Bit()) {
    expansion_opportunity_count_ = Character::ExpansionOpportunityCount(
        justification_method_, text_.Span8(), direction, is_after_expansion);
  } else {
    expansion_opportunity_count_ = Character::ExpansionOpportunityCount(
        justification_method_, text_.Span16(), direction, is_after_expansion);
  }
  if (is_after_expansion && !allows_trailing_expansion &&
      expansion_opportunity_count_ > 0) {
    --expansion_opportunity_count_;
  }

  if (expansion_opportunity_count_) {
    expansion_per_opportunity_ =
        (expansion_ / expansion_opportunity_count_).To<TextRunLayoutUnit>();
  }
}

TextRunLayoutUnit ShapeResultSpacing::NextExpansion() {
  if (!expansion_opportunity_count_) {
    NOTREACHED();
  }

  is_after_expansion_ = true;

  if (!--expansion_opportunity_count_) [[unlikely]] {
    const TextRunLayoutUnit remaining = expansion_.To<TextRunLayoutUnit>();
    expansion_ = InlineLayoutUnit();
    return remaining;
  }

  expansion_ -= expansion_per_opportunity_.To<InlineLayoutUnit>();
  return expansion_per_opportunity_;
}

TextRunLayoutUnit ShapeResultSpacing::ComputeSpacing(
    const ComputeSpacingParameters& parameters,
    float& offset,
    bool is_cursive_script) {
  DCHECK(has_spacing_);
  unsigned index = parameters.index;
  UChar32 character = text_[index];
  bool treat_as_space =
      (Character::TreatAsSpace(character) ||
       (normalize_space_ &&
        Character::IsNormalizedCanvasSpaceCharacter(character))) &&
      (character != '\t' || !allow_tabs_);
  if (treat_as_space && character != uchar::kNoBreakSpace) {
    character = uchar::kSpace;
  }

  TextRunLayoutUnit spacing;

  bool has_letter_spacing = letter_spacing_;
  bool apply_letter_spacing =
      RuntimeEnabledFeatures::IgnoreLetterSpacingInCursiveScriptsEnabled()
          ? !is_cursive_script
          : true;
  if (has_letter_spacing && !Character::TreatAsZeroWidthSpace(character) &&
      apply_letter_spacing) {
    spacing += letter_spacing_;
    is_letter_spacing_applied_ = true;
  }

  if (treat_as_space && (allow_word_spacing_anywhere_ || index ||
                         character == uchar::kNoBreakSpace)) {
    spacing += word_spacing_;
    is_word_spacing_applied_ = true;
  }

  return spacing;
}

std::pair<float, TextRunLayoutUnit> ShapeResultSpacing::ComputeExpansion(
    unsigned index,
    bool is_cursive_script) {
  if (!HasExpansion() || index >= text_.length()) {
    return {0.0f, TextRunLayoutUnit()};
  }
  DCHECK(!normalize_space_);
  DCHECK(!allow_tabs_);

  float spacing_before = 0;
  TextRunLayoutUnit spacing_after;

  bool opportunity_before = false;
  bool opportunity_after = false;
  if (text_.Is8Bit()) {
    auto pair = CheckJustificationOpportunity8(
        justification_method_, text_[index], is_after_expansion_);
    opportunity_before = pair.first;
    opportunity_after = pair.second;
  } else {
    VLOG(0) << __func__ << " size=" << text_.Span16().size()
            << " index=" << index;
    auto pair = CheckJustificationOpportunity16(
        justification_method_, CodePointAt(text_.Span16(), index),
        is_after_expansion_);
    opportunity_before = pair.first;
    opportunity_after = pair.second;
  }

  if (opportunity_before) {
    // Take the expansion opportunity before this ideograph.
    TextRunLayoutUnit expand_before = NextExpansion();
    if (expand_before) {
      spacing_before += expand_before.ToFloat();
      spacing_after += expand_before;
    }
    if (!HasExpansion()) {
      return {spacing_before, spacing_after};
    }
  }
  if (opportunity_after) {
    return {spacing_before, spacing_after + NextExpansion()};
  }
  return {spacing_before, spacing_after};
}

}  // namespace blink
