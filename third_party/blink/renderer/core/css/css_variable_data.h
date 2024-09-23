// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VARIABLE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VARIABLE_DATA_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CSSSyntaxDefinition;
enum class SecureContextMode;

class CORE_EXPORT CSSVariableData : public GarbageCollected<CSSVariableData> {
 public:
  CSSVariableData()
      : length_(0),
        is_animation_tainted_(false),
        needs_variable_resolution_(false),
        is_8bit_(true),
        has_font_units_(false),
        has_root_font_units_(false),
        has_line_height_units_(false),
        unused_(0) {}

  using PassKey = base::PassKey<CSSVariableData>;
  CSSVariableData(PassKey,
                  StringView,
                  bool is_animation_tainted,
                  bool needs_variable_resolution,
                  bool has_font_units,
                  bool has_root_font_units,
                  bool has_line_height_units);

  // This is the fastest (non-trivial) constructor if you've got the has_* data
  // already, e.g. because you extracted them while tokenizing (see
  // ExtractFeatures()) or got them from another CSSVariableData instance during
  // substitution.
  static CSSVariableData* Create(StringView original_text,
                                 bool is_animation_tainted,
                                 bool needs_variable_resolution,
                                 bool has_font_units,
                                 bool has_root_font_units,
                                 bool has_line_height_units) {
    if (original_text.length() > kMaxVariableBytes) {
      // This should have been blocked off during variable substitution.
      NOTREACHED_IN_MIGRATION();
      return nullptr;
    }

    return MakeGarbageCollected<CSSVariableData>(
        AdditionalBytes(original_text.Is8Bit() ? original_text.length()
                                               : 2 * original_text.length()),
        PassKey(), original_text, is_animation_tainted,
        needs_variable_resolution, has_font_units, has_root_font_units,
        has_line_height_units);
  }

  // This tokenizes the string to determine the has_* data.
  // (The tokens are not used apart from that; only the original string is
  // stored.)
  static CSSVariableData* Create(const String& original_text,
                                 bool is_animation_tainted,
                                 bool needs_variable_resolution);

  void Trace(Visitor*) const {}

  StringView OriginalText() const {
    if (is_8bit_) {
      return StringView(reinterpret_cast<const LChar*>(this + 1), length_);
    } else {
      return StringView(reinterpret_cast<const UChar*>(this + 1), length_);
    }
  }

  String Serialize() const;

  bool operator==(const CSSVariableData& other) const;
  bool EqualsIgnoringTaint(const CSSVariableData& other) const;

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
static_assert(sizeof(CSSVariableData) <= 4,
              "CSSVariableData must not grow without thinking");
#endif

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VARIABLE_DATA_H_
