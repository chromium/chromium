// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_SPACING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_SPACING_H_

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/text/justification_opportunity.h"
#include "third_party/blink/renderer/platform/text/text_justify.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class FontDescription;

// A context object to apply letter-spacing, word-spacing, and justification to
// ShapeResult.
class PLATFORM_EXPORT ShapeResultSpacing final {
  STACK_ALLOCATED();

 public:
  explicit ShapeResultSpacing(const String& text,
                              bool allow_word_spacing_anywhere = false)
      : text_(text),
        allow_word_spacing_anywhere_(allow_word_spacing_anywhere) {}

  const String& Text() const LIFETIME_BOUND { return text_; }
  TextRunLayoutUnit LetterSpacing() const {
    return has_spacing_ ? letter_spacing_ : TextRunLayoutUnit();
  }
  TextRunLayoutUnit WordSpacing() const {
    return has_spacing_ ? word_spacing_ : TextRunLayoutUnit();
  }
  bool HasSpacing() const { return has_spacing_; }
  bool IsLetterSpacingAppliedForTesting() const {
    return is_letter_spacing_applied_;
  }
  bool IsWordSpacingAppliedForTesting() const {
    return is_word_spacing_applied_;
  }
  bool HasExpansion() const { return expansion_opportunity_count_; }
  unsigned ExpansionOppotunityCount() const {
    return expansion_opportunity_count_;
  }

  // Set letter-spacing and word-spacing.
  bool SetSpacing(const FontDescription&);
  bool SetSpacing(TextRunLayoutUnit letter_spacing,
                  TextRunLayoutUnit word_spacing);
  // Set letter-spacing, word-spacing for PlainTextPainter.
  void SetSpacing(const FontDescription&, bool normalize_space);

  // Set the expansion for the justification.
  // This function scans the whole `text_`.
  void SetExpansion(TextJustify method,
                    InlineLayoutUnit expansion,
                    TextDirection,
                    bool allows_leading_expansion = false,
                    bool allows_trailing_expansion = false);

  // An RAII class to prepare expansion.
  // This is useful when pouring strings manually.
  class PLATFORM_EXPORT ExpansionSetup {
    STACK_ALLOCATED();

   public:
    ExpansionSetup(InlineLayoutUnit expansion,
                   ShapeResultSpacing* spacing,
                   bool allows_leading_expansion = false,
                   bool allows_trailing_expansion = false);
    ~ExpansionSetup();

    ShapeResultSpacing* Spacing() const { return spacing_; }
    void CountOpportunities(TextJustify method, StringView text, TextDirection);
    void CountOpportunities(TextJustify method, UChar ch);

   private:
    ShapeResultSpacing* const spacing_;
    const bool allows_trailing_expansion_;
    JustificationContext justification_context_;
  };

  // Compute spacings for the specified `index`.
  // This function returns space amount to be added after the glyph.
  //
  // The `index` is for the string given in the constructor.
  TextRunLayoutUnit ComputeSpacing(unsigned index,
                                   bool is_cursive_script = false);

  // Compute spacings to justify a glyph at the specified `index`.
  // This function returns a pair of
  //  * Space amount to be added before the glyph, and
  //  * Space amount to be added after the glyph.
  //
  // The `index` is for the string given in the constructor.
  // This function must be called incrementally since it keeps states and
  // counts consumed justification opportunities.
  std::pair<TextRunLayoutUnit, TextRunLayoutUnit> ComputeExpansion(
      TextJustify method,
      unsigned index,
      bool is_cursive_script = false);
  // Compute spacings to justify the specified character.
  // This function returns a pair of
  //  * Space amount to be added before the glyph, and
  //  * Space amount to be added after the glyph.
  std::pair<TextRunLayoutUnit, TextRunLayoutUnit> ComputeExpansion(
      TextJustify method,
      UChar ch);

 private:
  // A helper for ComputeExpansion().
  std::pair<TextRunLayoutUnit, TextRunLayoutUnit> FinalizeComputeExpansion(
      bool opportunity_before,
      bool opportunity_after);
  TextRunLayoutUnit NextExpansion();

  String text_;
  TextRunLayoutUnit letter_spacing_;
  TextRunLayoutUnit word_spacing_;
  InlineLayoutUnit expansion_;
  TextRunLayoutUnit expansion_per_opportunity_;
  unsigned expansion_opportunity_count_ = 0;
  JustificationContext justification_context_;
  bool has_spacing_ = false;
  bool is_letter_spacing_applied_ = false;
  bool is_word_spacing_applied_ = false;
  bool normalize_space_ = false;
  bool allow_tabs_ = false;
  bool allow_word_spacing_anywhere_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_SPACING_H_
