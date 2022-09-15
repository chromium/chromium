// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VARIABLE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VARIABLE_DATA_H_

#include <memory>

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
      bool needs_variable_resolution,
      const KURL& base_url,
      const WTF::TextEncoding& charset) {
    void* buf =
        AllocateSpaceIncludingCSSParserTokens(tokenized_value.range.size());
    return base::AdoptRef(new (buf) CSSVariableData(
        tokenized_value, is_animation_tainted, needs_variable_resolution,
        base_url, charset));
  }

  static scoped_refptr<CSSVariableData> CreateResolved(
      Vector<CSSParserToken> resolved_tokens,
      Vector<String> backing_strings,
      bool is_animation_tainted,
      bool has_font_units,
      bool has_root_font_units,
      const String& base_url,
      const WTF::TextEncoding& charset) {
    void* buf = AllocateSpaceIncludingCSSParserTokens(resolved_tokens.size());
    return base::AdoptRef(new (buf) CSSVariableData(
        std::move(resolved_tokens), std::move(backing_strings),
        is_animation_tainted, has_font_units, has_root_font_units, base_url,
        charset));
  }

  CSSParserTokenRange TokenRange() const {
    return CSSParserTokenRange{
        base::span<CSSParserToken>(TokenInternalPtr(), num_tokens_)};
  }

  base::span<CSSParserToken> Tokens() const {
    return {TokenInternalPtr(), num_tokens_};
  }

  // Appends all backing strings to the given vector.
  void AppendBackingStrings(Vector<String>& output) const;

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

  const String& BaseURL() const { return base_url_; }

  const WTF::TextEncoding& Charset() const { return charset_; }

  const CSSValue* ParseForSyntax(const CSSSyntaxDefinition&,
                                 SecureContextMode) const;

  ~CSSVariableData() {
    if (num_backing_strings_ == 1) {
      backing_string_.~String();
    } else {
      backing_strings_.~unique_ptr<String[]>();
    }
  }

  CSSVariableData(const CSSVariableData&) = delete;
  CSSVariableData& operator=(const CSSVariableData&) = delete;
  CSSVariableData(CSSVariableData&&) = delete;
  CSSVariableData& operator=(const CSSVariableData&&) = delete;

 private:
  CSSVariableData() {}

  CSSVariableData(const CSSTokenizedValue&,
                  bool is_animation_tainted,
                  bool needs_variable_resolution,
                  const KURL& base_url,
                  const WTF::TextEncoding& charset);

  CSSVariableData(Vector<CSSParserToken> resolved_tokens,
                  Vector<String> backing_strings,
                  bool is_animation_tainted,
                  bool has_font_units,
                  bool has_root_font_units,
                  const String& base_url,
                  const WTF::TextEncoding& charset)
      : num_tokens_(resolved_tokens.size()),
        is_animation_tainted_(is_animation_tainted),
        has_font_units_(has_font_units),
        has_root_font_units_(has_root_font_units),
        base_url_(base_url),
        charset_(charset) {
    if (backing_strings.size() == 1) {
      backing_string_ = std::move(backing_strings[0]);
    } else if (backing_strings.size() > 1) {
      backing_strings_ = std::make_unique<String[]>(backing_strings.size());
      for (wtf_size_t i = 0; i < backing_strings.size(); ++i) {
        backing_strings_[i] = std::move(backing_strings[i]);
      }
    }
    num_backing_strings_ = backing_strings.size();

    std::uninitialized_move(resolved_tokens.begin(), resolved_tokens.end(),
                            TokenInternalPtr());
#if EXPENSIVE_DCHECKS_ARE_ON()
    VerifyStringBacking();
#endif  // EXPENSIVE_DCHECKS_ARE_ON()
  }

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

  // tokens_ may have raw pointers to string data, we store the String objects
  // owning that data in backing_strings_ to keep it alive alongside the
  // tokens_.
  union {
    String backing_string_;  // If num_backing_strings_ == 1.
    std::unique_ptr<String[]> backing_strings_{nullptr};  // Otherwise.
  };
  String original_text_;
  wtf_size_t num_tokens_ = 0;
  wtf_size_t num_backing_strings_ = 0;
  const bool is_animation_tainted_ = false;
  const bool needs_variable_resolution_ = false;
  bool has_font_units_ = false;
  bool has_root_font_units_ = false;
  String base_url_;
  WTF::TextEncoding charset_;

  // The CSSParserTokens are stored after this.
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VARIABLE_DATA_H_
