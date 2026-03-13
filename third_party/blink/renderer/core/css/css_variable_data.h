// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VARIABLE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VARIABLE_DATA_H_

#include "base/types/pass_key.h"
#include "base/types/strong_alias.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CSSSyntaxDefinition;
class CSSParserLocalContext;
enum class SecureContextMode;

enum class VariableDataFeature : uint8_t {
  kNone = 0,
  kHasFontUnits = 1 << 0,
  kHasRootFontUnits = 1 << 1,
  kHasLineHeightUnits = 1 << 2,
  kHasDashedFunctions = 1 << 3,
  kHasReferences = 1 << 4,
};
static constexpr size_t kVariableDataFeatureBits = 5;
using VariableDataFeatures = unsigned;

class CORE_EXPORT CSSVariableData : public GarbageCollected<CSSVariableData> {
 public:
  CSSVariableData()
      : length_(0),
        features_(
            static_cast<VariableDataFeatures>(VariableDataFeature::kNone)),
        is_animation_tainted_(false),
        is_attr_tainted_(false),
        is_8bit_(true) {}

  using PassKey = base::PassKey<CSSVariableData>;
  CSSVariableData(PassKey,
                  StringView,
                  bool is_animation_tainted,
                  bool is_attr_tainted,
                  VariableDataFeatures features);

  // This is the fastest (non-trivial) constructor if you've got the has_* data
  // already, e.g. because you extracted them while tokenizing (see
  // ExtractFeatures()) or got them from another CSSVariableData instance during
  // substitution.
  static CSSVariableData* Create(StringView original_text,
                                 bool is_animation_tainted,
                                 bool is_attr_tainted,
                                 VariableDataFeatures features) {
    if (original_text.length() > kMaxVariableBytes) {
      // This should have been blocked off during variable substitution.
      NOTREACHED();
    }

    return MakeGarbageCollected<CSSVariableData>(
        AdditionalBytes(original_text.Is8Bit() ? original_text.length()
                                               : 2 * original_text.length()),
        PassKey(), original_text, is_animation_tainted, is_attr_tainted,
        features);
  }

  // This tokenizes the string to determine the has_* data.
  // (The tokens are not used apart from that; only the original string is
  // stored.)
  using HasReferences = base::StrongAlias<class HasReferencesTag, bool>;
  static CSSVariableData* Create(const String& original_text,
                                 bool is_animation_tainted,
                                 bool is_attr_tainted,
                                 HasReferences has_references);

  void Trace(Visitor*) const {}

  StringView OriginalText() const {
    // SAFETY: See AdditionalBytes() in Create().
    if (is_8bit_) {
      return StringView(UNSAFE_BUFFERS(
          base::span(reinterpret_cast<const LChar*>(this + 1), length_)));
    } else {
      return StringView(UNSAFE_BUFFERS(
          base::span(reinterpret_cast<const UChar*>(this + 1), length_)));
    }
  }

  uint64_t Hash() const {
    return StringHasher::HashMemory(OriginalText().RawByteSpan());
  }

  String Serialize() const;

  bool EqualsIgnoringAttrTainting(const CSSVariableData& other) const;

  bool operator==(const CSSVariableData& other) const;

  bool IsAnimationTainted() const { return is_animation_tainted_; }

  bool IsAttrTainted() const { return is_attr_tainted_; }

  bool NeedsVariableResolution() const {
    return features_ & static_cast<VariableDataFeatures>(
                           VariableDataFeature::kHasReferences);
  }

  // True if the CSSVariableData has tokens with units that are relative to the
  // font-size of the current element, e.g. 'em'.
  bool HasFontUnits() const {
    return features_ & static_cast<VariableDataFeatures>(
                           VariableDataFeature::kHasFontUnits);
  }

  // True if the CSSVariableData has tokens with units that are relative to the
  // font-size of the root element, e.g. 'rem'.
  bool HasRootFontUnits() const {
    return features_ & static_cast<VariableDataFeatures>(
                           VariableDataFeature::kHasRootFontUnits);
  }

  // True if the CSSVariableData has tokens with 'lh' units which are relative
  // to line-height property.
  bool HasLineHeightUnits() const {
    return features_ & static_cast<VariableDataFeatures>(
                           VariableDataFeature::kHasLineHeightUnits);
  }

  // https://drafts.csswg.org/css-mixins-1/#typedef-dashed-function
  bool HasDashedFunctions() const {
    return features_ & static_cast<VariableDataFeatures>(
                           VariableDataFeature::kHasDashedFunctions);
  }

  VariableDataFeatures GetVariableDataFeatures() const { return features_; }

  const CSSValue* ParseForSyntax(const CSSSyntaxDefinition&,
                                 SecureContextMode,
                                 CSSParserLocalContext&) const;

  CSSVariableData(const CSSVariableData&) = delete;
  CSSVariableData& operator=(const CSSVariableData&) = delete;
  CSSVariableData(CSSVariableData&&) = delete;
  CSSVariableData& operator=(const CSSVariableData&&) = delete;

  // ORs the given flags with those of the given token.
  static VariableDataFeatures ExtractFeatures(const CSSParserToken& token);

  // The maximum number of bytes for a CSS variable (including text
  // that comes from var() substitution). This matches Firefox.
  //
  // If you change this, length_ below may need updates.
  //
  // https://drafts.csswg.org/css-values-5/#long-substitution
  static const size_t kMaxVariableBytes = 2097152;

 private:
  // We'd like to use bool for the booleans, but this causes the struct to
  // balloon in size on Windows:
  // https://randomascii.wordpress.com/2010/06/06/bit-field-packing-with-visual-c/

  // Enough for storing up to 2MB (and then some), cf. kMaxSubstitutionBytes.
  // The remaining 2 bits are kept in reserve for future use.
  const unsigned length_ : 22;
  VariableDataFeatures features_ : kVariableDataFeatureBits;
  const unsigned is_animation_tainted_ : 1;       // bool.
  const unsigned is_attr_tainted_ : 1;            // bool.
  const unsigned is_8bit_ : 1;                    // bool.
  unsigned /* unused_ */ : 2;

  // The actual character data is stored after this.
};

#if !DCHECK_IS_ON()
static_assert(sizeof(CSSVariableData) <= 4,
              "CSSVariableData must not grow without thinking");
#endif

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VARIABLE_DATA_H_
