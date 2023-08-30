// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_SPACING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_SPACING_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class FontDescription;

// A context object to apply letter-spacing, word-spacing, and justification to
// ShapeResult.
template <typename TextContainerType>
class PLATFORM_EXPORT ShapeResultSpacing final {
  STACK_ALLOCATED();

 public:
  explicit ShapeResultSpacing(const TextContainerType& text,
                              bool allow_word_spacing_anywhere = false)
      : text_(text),
        letter_spacing_(0),
        word_spacing_(0),
        expansion_(0),
        expansion_per_opportunity_(0),
        expansion_opportunity_count_(0),
        has_spacing_(false),
        normalize_space_(false),
        allow_tabs_(false),
        is_after_expansion_(false),
        allow_word_spacing_anywhere_(allow_word_spacing_anywhere) {}

  const TextContainerType& Text() const { return text_; }
  float LetterSpacing() const { return has_spacing_ ? letter_spacing_ : .0f; }
  float WordSpacing() const { return has_spacing_ ? word_spacing_ : .0f; }
  bool HasSpacing() const { return has_spacing_; }
  bool HasExpansion() const { return expansion_opportunity_count_; }
  unsigned ExpansionOppotunityCount() const {
    return expansion_opportunity_count_;
  }

  // Set letter-spacing and word-spacing.
  bool SetSpacing(const FontDescription&);
  bool SetSpacing(float letter_spacing, float word_spacing);

  // Set the expansion for the justification.
  void SetExpansion(float expansion,
                    TextDirection,
                    bool allows_leading_expansion = false,
                    bool allows_trailing_expansion = false);

  // Set letter-spacing, word-spacing, and
  // justification. Available only for TextRun.
  void SetSpacingAndExpansion(const FontDescription&);

  // Compute the sum of all spacings for the specified |index|.
  // The |index| is for the |TextContainerType| given in the constructor.
  // For justification, this function must be called incrementally since it
  // keeps states and counts consumed justification opportunities.
  struct ComputeSpacingParameters {
    unsigned index;
    float original_advance = 0.0;
  };
  float ComputeSpacing(unsigned index, float& offset) {
    return ComputeSpacing(ComputeSpacingParameters{.index = index}, offset);
  }
  float ComputeSpacing(const ComputeSpacingParameters& parameters,
                       float& offset);

 private:
  bool IsAfterExpansion() const { return is_after_expansion_; }

  void ComputeExpansion(bool allows_leading_expansion,
                        bool allows_trailing_expansion,
                        TextDirection);

  float NextExpansion();

  const TextContainerType& text_;
  float letter_spacing_;
  float word_spacing_;
  float expansion_;
  float expansion_per_opportunity_;
  unsigned expansion_opportunity_count_;
  bool has_spacing_;
  bool normalize_space_;
  bool allow_tabs_;
  bool is_after_expansion_;
  bool allow_word_spacing_anywhere_;
};

// Forward declare so no implicit instantiations happen before the
// first explicit instantiation (which would be a C++ violation).
template <>
void ShapeResultSpacing<TextRun>::SetSpacingAndExpansion(
    const FontDescription&);
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_SPACING_H_
