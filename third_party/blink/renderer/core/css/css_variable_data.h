// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VARIABLE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VARIABLE_DATA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenized_value.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CSSSyntaxDefinition;
enum class SecureContextMode;

class CORE_EXPORT CSSVariableData : public RefCounted<CSSVariableData> {
  USING_FAST_MALLOC(CSSVariableData);

 public:
  static scoped_refptr<CSSVariableData> Create() {
    return base::AdoptRef(new CSSVariableData());
  }

  // This is the fastest (non-trivial) constructor if you've got the has_* data
  // already, e.g. because you extracted them while tokenizing (see
  // ExtractFeatures()) or got them from another CSSVariableData instance during
  // substitution.
  static scoped_refptr<CSSVariableData> Create(
      StringView original_text,
      int num_tokens_for_ablation,  // -1 for no experiment.
      bool is_animation_tainted,
      bool needs_variable_resolution,
      bool has_font_units,
      bool has_root_font_units,
      bool has_line_height_units) {
    if (original_text.length() > kMaxVariableBytes) {
      // This should have been blocked off during variable substitution.
      NOTREACHED();
      return nullptr;
    }

    wtf_size_t bytes_needed =
        sizeof(CSSVariableData) + (original_text.Is8Bit()
                                       ? original_text.length()
                                       : 2 * original_text.length());
    if (num_tokens_for_ablation >= 0) {
      // Allocate more memory for studying the difference between
      // storing the tokens or not. (We don't measure the CPU costs
      // or savings, since that rapidly becomes complex when considering
      // that the old code didn't actually give the right result.)
      // We used to need an AtomicString for backing string tokens;
      // we don't model its contents nor costs in the global table.
      // We also used a separate String object for original_text,
      // but we don't consider memory allocator overhead. Finally,
      // there's a token counter and the actual tokens.
      //
      // We also moved some elements around to save on padding
      // and similar, but we don't model this.
      bytes_needed += sizeof(AtomicString) + sizeof(String) +
                      sizeof(wtf_size_t) +
                      num_tokens_for_ablation * sizeof(CSSParserToken);
    }
    void* buf = WTF::Partitions::FastMalloc(
        bytes_needed, WTF::GetStringWithTypeName<CSSVariableData>());
    return base::AdoptRef(new (buf) CSSVariableData(
        original_text, is_animation_tainted, needs_variable_resolution,
        has_font_units, has_root_font_units, has_line_height_units));
  }

  // Second-fastest; scans through all the tokens to determine the has_* data.
  // (The tokens are not used apart from that; only the original string is
  // stored.) The tokens must correspond to the given string.
  static scoped_refptr<CSSVariableData> Create(CSSTokenizedValue value,
                                               bool is_animation_tainted,
                                               bool needs_variable_resolution);

  // Like the previous, but also needs to tokenize the string.
  static scoped_refptr<CSSVariableData> Create(const String& original_text,
                                               bool is_animation_tainted,
                                               bool needs_variable_resolution);

  StringView OriginalText() const {
    if (is_8bit_) {
      return StringView(reinterpret_cast<const LChar*>(this + 1), length_);
    } else {
      return StringView(reinterpret_cast<const UChar*>(this + 1), length_);
    }
  }

  String Serialize() const;

  bool operator==(const CSSVariableData& other) const;

  bool IsAnimationTainted() const { return is_animation_tainted_; }

  bool NeedsVariableResolution() const { return needs_variable_resolution_; }

  // True if the CSSVariableData has tokens with units that are relative to the
  // font-size of the current element, e.g. 'em'.
  bool HasFontUnits() const { return has_font_units_; }

  // True if the CSSVariableData has tokens with units that are relative to the
  // font-size of the root element, e.g. 'rem'.
  bool HasRootFontUnits() const { return has_root_font_units_; }

  // True if the CSSVariableData has tokens with 'lh' units which are relative
  // to line-height property.
  bool HasLineHeightUnits() const { return has_line_height_units_; }

  const CSSValue* ParseForSyntax(const CSSSyntaxDefinition&,
                                 SecureContextMode) const;

  CSSVariableData(const CSSVariableData&) = delete;
  CSSVariableData& operator=(const CSSVariableData&) = delete;
  CSSVariableData(CSSVariableData&&) = delete;
  CSSVariableData& operator=(const CSSVariableData&&) = delete;

  // ORs the given flags with those of the given token.
  static void ExtractFeatures(const CSSParserToken& token,
                              bool& has_font_units,
                              bool& has_root_font_units,
                              bool& has_line_height_units);

  // The maximum number of bytes for a CSS variable (including text
  // that comes from var() substitution). This matches Firefox.
  //
  // If you change this, length_ below may need updates.
  //
  // https://drafts.csswg.org/css-variables/#long-variables
  static const size_t kMaxVariableBytes = 2097152;

 private:
  CSSVariableData()
      : length_(0),
        is_animation_tainted_(false),
        needs_variable_resolution_(false),
        is_8bit_(true),
        has_font_units_(false),
        has_root_font_units_(false),
        has_line_height_units_(false),
        unused_(0) {}

  CSSVariableData(StringView,
                  bool is_animation_tainted,
                  bool needs_variable_resolution,
                  bool has_font_units,
                  bool has_root_font_units,
                  bool has_line_height_units);

  // 32 bits refcount before this.

  // We'd like to use bool for the booleans, but this causes the struct to
  // balloon in size on Windows:
  // https://randomascii.wordpress.com/2010/06/06/bit-field-packing-with-visual-c/

  // Enough for storing up to 2MB (and then some), cf. kMaxSubstitutionBytes.
  // The remaining 4 bits are kept in reserve for future use.
  const unsigned length_ : 22;
  const unsigned is_animation_tainted_ : 1;       // bool.
  const unsigned needs_variable_resolution_ : 1;  // bool.
  const unsigned is_8bit_ : 1;                    // bool.
  unsigned has_font_units_ : 1;                   // bool.
  unsigned has_root_font_units_ : 1;              // bool.
  unsigned has_line_height_units_ : 1;            // bool.
  const unsigned unused_ : 4;

  // The actual character data is stored after this.
};

#if !DCHECK_IS_ON()
static_assert(sizeof(CSSVariableData) <= 8,
              "CSSVariableData must not grow without thinking");
#endif

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VARIABLE_DATA_H_
