// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"

#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/text/text_run.h"

namespace blink {

template <typename TextContainerType>
bool ShapeResultSpacing<TextContainerType>::SetSpacing(
    const FontDescription& font_description) {
  return SetSpacing(font_description.LetterSpacing(),
                    font_description.WordSpacing());
}

template <typename TextContainerType>
bool ShapeResultSpacing<TextContainerType>::SetSpacing(float letter_spacing,
                                                       float word_spacing) {
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

template <typename TextContainerType>
void ShapeResultSpacing<TextContainerType>::SetExpansion(
    float expansion,
    TextDirection direction,
    bool allows_leading_expansion,
    bool allows_trailing_expansion) {
  DCHECK_GT(expansion, 0);
  expansion_ = expansion;
  ComputeExpansion(allows_leading_expansion, allows_trailing_expansion,
                   direction);
  has_spacing_ |= HasExpansion();
}

template <typename TextContainerType>
void ShapeResultSpacing<TextContainerType>::SetSpacingAndExpansion(
    const FontDescription& font_description) {
  // Available only for TextRun since it has expansion data.
  NOTREACHED();
}

template <>
void ShapeResultSpacing<TextRun>::SetSpacingAndExpansion(
    const FontDescription& font_description) {
  letter_spacing_ = font_description.LetterSpacing();
  word_spacing_ = font_description.WordSpacing();
  expansion_ = 0;
  has_spacing_ = letter_spacing_ || word_spacing_;
  if (!has_spacing_)
    return;

  normalize_space_ = text_.NormalizeSpace();
  allow_tabs_ = false;
}

template <typename TextContainerType>
void ShapeResultSpacing<TextContainerType>::ComputeExpansion(
    bool allows_leading_expansion,
    bool allows_trailing_expansion,
    TextDirection direction) {
  DCHECK_GT(expansion_, 0);

  is_after_expansion_ = !allows_leading_expansion;
  bool is_after_expansion = is_after_expansion_;
  if (text_.Is8Bit()) {
    expansion_opportunity_count_ = Character::ExpansionOpportunityCount(
        text_.Span8(), direction, is_after_expansion);
  } else {
    expansion_opportunity_count_ = Character::ExpansionOpportunityCount(
        text_.Span16(), direction, is_after_expansion);
  }
  if (is_after_expansion && !allows_trailing_expansion) {
    DCHECK_GT(expansion_opportunity_count_, 0u);
    --expansion_opportunity_count_;
  }

  if (expansion_opportunity_count_)
    expansion_per_opportunity_ = expansion_ / expansion_opportunity_count_;
}

template <typename TextContainerType>
float ShapeResultSpacing<TextContainerType>::NextExpansion() {
  if (!expansion_opportunity_count_) {
    NOTREACHED();
    return 0;
  }

  is_after_expansion_ = true;

  if (!--expansion_opportunity_count_) {
    float remaining = expansion_;
    expansion_ = 0;
    return remaining;
  }

  expansion_ -= expansion_per_opportunity_;
  return expansion_per_opportunity_;
}

template <typename TextContainerType>
float ShapeResultSpacing<TextContainerType>::ComputeSpacing(
    const ComputeSpacingParameters& parameters,
    float& offset) {
  DCHECK(has_spacing_);
  unsigned index = parameters.index;
  UChar32 character = text_[index];
  bool treat_as_space =
      (Character::TreatAsSpace(character) ||
       (normalize_space_ &&
        Character::IsNormalizedCanvasSpaceCharacter(character))) &&
      (character != '\t' || !allow_tabs_);
  if (treat_as_space && character != kNoBreakSpaceCharacter)
    character = kSpaceCharacter;

  float spacing = 0;

  bool has_letter_spacing = letter_spacing_;
  if (has_letter_spacing && !Character::TreatAsZeroWidthSpace(character))
    spacing += letter_spacing_;

  if (treat_as_space && (allow_word_spacing_anywhere_ || index ||
                         character == kNoBreakSpaceCharacter))
    spacing += word_spacing_;

  if (!HasExpansion())
    return spacing;

  if (treat_as_space)
    return spacing + NextExpansion();

  if (text_.Is8Bit())
    return spacing;

  // isCJKIdeographOrSymbol() has expansion opportunities both before and
  // after each character.
  // http://www.w3.org/TR/jlreq/#line_adjustment
  if (U16_IS_LEAD(character) && index + 1 < text_.length() &&
      U16_IS_TRAIL(text_[index + 1]))
    character = U16_GET_SUPPLEMENTARY(character, text_[index + 1]);
  if (!Character::IsCJKIdeographOrSymbol(character)) {
    is_after_expansion_ = false;
    return spacing;
  }

  if (!is_after_expansion_) {
    // Take the expansion opportunity before this ideograph.
    float expand_before = NextExpansion();
    if (expand_before) {
      offset += expand_before;
      spacing += expand_before;
    }
    if (!HasExpansion())
      return spacing;
  }

  return spacing + NextExpansion();
}

// Instantiate the template class.
template class ShapeResultSpacing<TextRun>;
template class ShapeResultSpacing<String>;

}  // namespace blink
