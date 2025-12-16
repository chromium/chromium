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

ShapeResultSpacing::ExpansionSetup::ExpansionSetup(
    InlineLayoutUnit expansion,
    ShapeResultSpacing* spacing,
    bool allows_leading_expansion,
    bool allows_trailing_expansion)
    : spacing_(spacing),
      allows_trailing_expansion_(allows_trailing_expansion),
      justification_context_(JustificationContext::Type::kNormal,
                             !allows_leading_expansion) {
  DCHECK_GT(expansion, InlineLayoutUnit());
  spacing_->expansion_ = expansion;
  spacing_->expansion_opportunity_count_ = 0;
  spacing_->justification_context_ = justification_context_;
}

ShapeResultSpacing::ExpansionSetup::~ExpansionSetup() {
  if (justification_context_.is_after_opportunity &&
      !allows_trailing_expansion_ &&
      spacing_->expansion_opportunity_count_ > 0) {
    --spacing_->expansion_opportunity_count_;
  }
  if (spacing_->expansion_opportunity_count_) {
    spacing_->expansion_per_opportunity_ =
        (spacing_->expansion_ / spacing_->expansion_opportunity_count_)
            .To<TextRunLayoutUnit>();
  }
  spacing_->has_spacing_ |= spacing_->HasExpansion();
}

void ShapeResultSpacing::ExpansionSetup::CountOpportunities(
    TextJustify method,
    StringView text,
    TextDirection direction) {
  if (text.Is8Bit()) {
    spacing_->expansion_opportunity_count_ +=
        Character::ExpansionOpportunityCount(method, text.Span8(), direction,
                                             justification_context_);
  } else {
    spacing_->expansion_opportunity_count_ +=
        Character::ExpansionOpportunityCount(method, text.Span16(), direction,
                                             justification_context_);
  }
}

void ShapeResultSpacing::ExpansionSetup::CountOpportunities(TextJustify method,
                                                            UChar ch) {
  spacing_->expansion_opportunity_count_ +=
      CountJustificationOpportunity16(method, ch, justification_context_);
}

void ShapeResultSpacing::SetExpansion(TextJustify method,
                                      InlineLayoutUnit expansion,
                                      TextDirection direction,
                                      bool allows_leading_expansion,
                                      bool allows_trailing_expansion) {
  ExpansionSetup setup(expansion, this, allows_leading_expansion,
                       allows_trailing_expansion);
  setup.CountOpportunities(method, text_, direction);
}

void ShapeResultSpacing::SetSpacing(const FontDescription& font_description,
                                    bool normalize_space) {
  if (SetSpacing(font_description)) {
    normalize_space_ = normalize_space;
    allow_tabs_ = false;
  }
}

TextRunLayoutUnit ShapeResultSpacing::NextExpansion() {
  if (!expansion_opportunity_count_) {
    NOTREACHED();
  }

  justification_context_.is_after_opportunity = true;

  if (!--expansion_opportunity_count_) [[unlikely]] {
    const TextRunLayoutUnit remaining = expansion_.To<TextRunLayoutUnit>();
    expansion_ = InlineLayoutUnit();
    return remaining;
  }

  expansion_ -= expansion_per_opportunity_.To<InlineLayoutUnit>();
  return expansion_per_opportunity_;
}

TextRunLayoutUnit ShapeResultSpacing::ComputeSpacing(unsigned index,
                                                     bool is_cursive_script) {
  DCHECK(has_spacing_);
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

std::pair<TextRunLayoutUnit, TextRunLayoutUnit>
ShapeResultSpacing::ComputeExpansion(TextJustify method,
                                     unsigned index,
                                     bool is_cursive_script) {
  if (!HasExpansion() || index >= text_.length()) {
    return {TextRunLayoutUnit(), TextRunLayoutUnit()};
  }
  DCHECK(!normalize_space_);
  DCHECK(!allow_tabs_);

  bool opportunity_before = false;
  bool opportunity_after = false;
  if (text_.Is8Bit()) {
    auto pair = CheckJustificationOpportunity8(method, text_[index],
                                               justification_context_);
    opportunity_before = pair.first;
    opportunity_after = pair.second;
  } else {
    auto pair = CheckJustificationOpportunity16(
        method, CodePointAt(text_.Span16(), index), justification_context_);
    opportunity_before = pair.first;
    opportunity_after = pair.second;
  }

  return FinalizeComputeExpansion(opportunity_before, opportunity_after);
}

std::pair<TextRunLayoutUnit, TextRunLayoutUnit>
ShapeResultSpacing::ComputeExpansion(TextJustify method, UChar ch) {
  if (!HasExpansion()) {
    return {TextRunLayoutUnit(), TextRunLayoutUnit()};
  }
  DCHECK(!normalize_space_);
  DCHECK(!allow_tabs_);
  auto [opportunity_before, opportunity_after] =
      CheckJustificationOpportunity16(method, ch, justification_context_);
  return FinalizeComputeExpansion(opportunity_before, opportunity_after);
}

std::pair<TextRunLayoutUnit, TextRunLayoutUnit>
ShapeResultSpacing::FinalizeComputeExpansion(bool opportunity_before,
                                             bool opportunity_after) {
  TextRunLayoutUnit spacing_before;
  TextRunLayoutUnit spacing_after;
  if (opportunity_before) {
    // Take the expansion opportunity before this ideograph.
    spacing_before = NextExpansion();
    if (!HasExpansion()) {
      return {spacing_before, spacing_after};
    }
  }
  if (opportunity_after) {
    spacing_after = NextExpansion();
  }
  return {spacing_before, spacing_after};
}

}  // namespace blink
