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
  static scoped_refptr<CSSVariableData> Create(
      const CSSTokenizedValue& tokenized_value,
      bool is_animation_tainted,
      bool needs_variable_resolution) {
    void* buf =
        AllocateSpaceIncludingCSSParserTokens(tokenized_value.range.size());
    return base::AdoptRef(new (buf) CSSVariableData(
        tokenized_value, is_animation_tainted, needs_variable_resolution));
  }

  CSSParserTokenRange TokenRange() const {
    return CSSParserTokenRange{
        base::span<CSSParserToken>(TokenInternalPtr(), num_tokens_)};
  }

  base::span<CSSParserToken> Tokens() const {
    return {TokenInternalPtr(), num_tokens_};
  }

  const AtomicString& BackingString() const { return backing_string_; }

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

 private:
  CSSVariableData() {}

  CSSVariableData(const CSSTokenizedValue&,
                  bool is_animation_tainted,
                  bool needs_variable_resolution);

  void ConsumeAndUpdateTokens(const CSSParserTokenRange&);
#if EXPENSIVE_DCHECKS_ARE_ON()
  void VerifyStringBacking() const;
#endif  // EXPENSIVE_DCHECKS_ARE_ON()

  static void* AllocateSpaceIncludingCSSParserTokens(size_t num_tokens) {
    const size_t bytes_needed =
        sizeof(CSSVariableData) + num_tokens * sizeof(CSSParserToken);
    return WTF::Partitions::FastMalloc(
        bytes_needed, WTF::GetStringWithTypeName<CSSVariableData>());
  }

  CSSParserToken* TokenInternalPtr() const {
    return const_cast<CSSParserToken*>(
        reinterpret_cast<const CSSParserToken*>(this + 1));
  }

  // tokens_ may have raw pointers to string data, we store the String object
  // owning that data in backing_string_ to keep it alive alongside the
  // tokens_. (AtomicString makes sure it is deduplicated.)
  AtomicString backing_string_;
  String original_text_;
  wtf_size_t num_tokens_ = 0;
  const bool is_animation_tainted_ = false;
  const bool needs_variable_resolution_ = false;
  bool has_font_units_ = false;
  bool has_root_font_units_ = false;
  bool has_line_height_units_ = false;

  // The CSSParserTokens are stored after this.
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VARIABLE_DATA_H_
