// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HAN_KERNING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HAN_KERNING_H_

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/shaping/font_features.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/text/han_kerning_char_type.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class LayoutLocale;
class ShapeResult;
class SimpleFontData;

//
// This class implements the behavior necessary for the CSS `text-spacing-trim`
// property[1].
//
// In short, it's similar to kerning to be applied to Han-derived scripts such
// as Chinese or Japanese. In OpenType, this behavior is defined as the
// `chws`[2] feature.
//
// It's different from the regular kerning that the kerning pairs and amounts
// are computable. There are tools to add the features to existing fonts[3][4].
//
// This class complements the OpenType feature in that:
// 1. Handles the desired beahvior at the font boundaries. OpenType features
//    can't handle kerning at font boundaries by design.
// 2. Emulates the behavior when the font doesn't have the `chws` feature.
//
// [1]: https://drafts.csswg.org/css-text-4/#text-spacing-trim-property
// [2]:
// https://learn.microsoft.com/en-us/typography/opentype/spec/features_ae#tag-chws
// [3]: https://github.com/googlefonts/chws_tool
// [4]: https://github.com/kojiishi/east_asian_spacing
//
class PLATFORM_EXPORT HanKerning {
  STACK_ALLOCATED();

 public:
  struct Options {
    bool is_horizontal = true;
    bool is_line_start = false;
    bool apply_start = false;
    bool apply_end = false;
  };

  HanKerning(const String& text,
             wtf_size_t start,
             wtf_size_t end,
             const FontDescription& font_description)
      : may_apply_(MayApply(StringView(text, start, end - start)) &&
                   font_description.GetTextSpacingTrim() !=
                       TextSpacingTrim::kSpaceAll),
        segment_start_(start),
        segment_end_(end) {}

  bool MayApply() const { return may_apply_; }
  static bool MayApply(StringView text);

  const Vector<unsigned, 32>& UnsafeToBreakBefore() const {
    return unsafe_to_break_before_;
  }
  void ClearUnsafeToBreakBefore() { unsafe_to_break_before_.Shrink(0); }

  bool AppendFontFeatures(const String& text,
                          wtf_size_t start,
                          wtf_size_t end,
                          const SimpleFontData& font_data,
                          const LayoutLocale& locale,
                          Options options,
                          FontFeatureRanges& features);

  void PrepareFallback(const String& text);

  void DidShapeSegment(ShapeResult& result);

  using CharType = HanKerningCharType;

  // Data retrieved from fonts for `HanKerning`.
  struct PLATFORM_EXPORT FontData {
    FontData() = default;
    FontData(const SimpleFontData& font,
             const LayoutLocale& locale,
             bool is_horizontal);

    // True if this font has `halt` (or `vhal` in vertical.)
    // https://learn.microsoft.com/en-us/typography/opentype/spec/features_fj#tag-halt
    bool has_alternate_spacing = false;
    // True if this font has `chws` (or `vchw` in vertical.)
    // https://learn.microsoft.com/en-us/typography/opentype/spec/features_ae#tag-chws
    bool has_contextual_spacing = false;

    // True if quote characters are fullwdith. In a common convention, they are
    // proportional (Latin) in Japanese, but fullwidth in Chinese.
    bool is_quote_fullwidth = false;

    // `CharType` for "fullwidth dot punctuation."
    // https://drafts.csswg.org/css-text-4/#text-spacing-classes
    CharType type_for_dot = CharType::kOther;
    // `CharType` for "fullwidth colon punctuation."
    // Type for colon and semicolon are separated, to support the Adobe's
    // convention for vertical flow, which rotates Japanese colon, but doesn't
    // rotate semicolon.
    CharType type_for_colon = CharType::kOther;
    CharType type_for_semicolon = CharType::kOther;
  };

 private:
  FRIEND_TEST_ALL_PREFIXES(HanKerningTest, MayApply);

  enum class Priority : uint8_t { kText, kCache };

  static CharType GetCharType(UChar ch, const FontData& font_data);
  CharType GetCharType(const String& text,
                       wtf_size_t index,
                       const FontData& font_data,
                       Priority priority = Priority::kText);
  CharType GetCharTypeWithCache(const String& text,
                                wtf_size_t index,
                                const FontData& font_data,
                                Priority priority);

  static bool ShouldKern(CharType type, CharType last_type);
  static bool ShouldKernLast(CharType type, CharType last_type);

  void ApplyKerning(ShapeResult& result);

  bool may_apply_;
  bool is_start_prev_used_ = false;
  bool is_end_next_used_ = false;
  wtf_size_t segment_start_;
  wtf_size_t segment_end_;
  wtf_size_t last_start_ = 0;
  wtf_size_t last_end_ = 0;
  const FontData* last_font_data_ = nullptr;
  Vector<CharType> char_types_;
  Vector<unsigned, 32> unsafe_to_break_before_;
  Vector<wtf_size_t> changed_indexes_;
};

inline bool HanKerning::MayApply(StringView text) {
  return !text.Is8Bit() && !text.IsAllSpecialCharacters<[](UChar ch) {
    return !Character::MaybeHanKerningOpenOrCloseFast(ch);
  }>();
}

inline void HanKerning::DidShapeSegment(ShapeResult& result) {
  if (!changed_indexes_.empty()) [[unlikely]] {
    ApplyKerning(result);
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HAN_KERNING_H_
