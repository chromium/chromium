// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_variable_data.h"

#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

template <typename CharacterType>
static void UpdateTokens(const CSSParserTokenRange& range,
                         const String& backing_string,
                         Vector<CSSParserToken>& result) {
  const CharacterType* current_offset =
      backing_string.GetCharacters<CharacterType>();
  for (const CSSParserToken& token : range) {
    if (token.HasStringBacking()) {
      unsigned length = token.Value().length();
      StringView string(current_offset, length);
      result.push_back(token.CopyWithUpdatedString(string));
      current_offset += length;
    } else {
      result.push_back(token);
    }
  }
  DCHECK(current_offset == backing_string.GetCharacters<CharacterType>() +
                               backing_string.length());
}

static bool IsFontUnitToken(CSSParserToken token) {
  if (token.GetType() != kDimensionToken)
    return false;
  switch (token.GetUnitType()) {
    case CSSPrimitiveValue::UnitType::kEms:
    case CSSPrimitiveValue::UnitType::kChs:
    case CSSPrimitiveValue::UnitType::kExs:
      return true;
    default:
      return false;
  }
}

static bool IsRootFontUnitToken(CSSParserToken token) {
  return token.GetType() == kDimensionToken &&
         token.GetUnitType() == CSSPrimitiveValue::UnitType::kRems;
}

String CSSVariableData::Serialize() const {
  if (original_text_) {
    if (original_text_.EndsWith('\\')) {
      // https://drafts.csswg.org/css-syntax/#consume-escaped-code-point
      // '\' followed by EOF is consumed as U+FFFD.
      // https://drafts.csswg.org/css-syntax/#consume-string-token
      // '\' followed by EOF in a string token is ignored.
      //
      // The tokenizer handles both of these cases when returning tokens, but
      // since we're working with the original string, we need to deal with them
      // ourselves.
      StringBuilder serialized_text;
      serialized_text.Append(original_text_);
      serialized_text.Resize(serialized_text.length() - 1);
      DCHECK(!tokens_.IsEmpty());
      const CSSParserToken& last = tokens_.back();
      if (last.GetType() != kStringToken)
        serialized_text.Append(kReplacementCharacter);

      // Certain token types implicitly include terminators when serialized.
      // https://drafts.csswg.org/cssom/#common-serializing-idioms
      if (last.GetType() == kStringToken)
        serialized_text.Append('"');
      if (last.GetType() == kUrlToken)
        serialized_text.Append(')');

      return serialized_text.ToString();
    }

    return original_text_;
  }
  return TokenRange().Serialize();
}

bool CSSVariableData::operator==(const CSSVariableData& other) const {
  return Tokens() == other.Tokens();
}

void CSSVariableData::ConsumeAndUpdateTokens(const CSSParserTokenRange& range) {
  DCHECK_EQ(tokens_.size(), 0u);
  DCHECK_EQ(backing_strings_.size(), 0u);
  StringBuilder string_builder;
  CSSParserTokenRange local_range = range;

  while (!local_range.AtEnd()) {
    CSSParserToken token = local_range.Consume();
    if (token.HasStringBacking())
      string_builder.Append(token.Value());
    has_font_units_ |= IsFontUnitToken(token);
    has_root_font_units_ |= IsRootFontUnitToken(token);
  }
  String backing_string = string_builder.ToString();
  backing_strings_.push_back(backing_string);
  if (backing_string.Is8Bit())
    UpdateTokens<LChar>(range, backing_string, tokens_);
  else
    UpdateTokens<UChar>(range, backing_string, tokens_);
}

CSSVariableData::CSSVariableData(const CSSTokenizedValue& tokenized_value,
                                 bool is_animation_tainted,
                                 bool needs_variable_resolution,
                                 const KURL& base_url,
                                 const WTF::TextEncoding& charset)
    : original_text_(tokenized_value.text.ToString()),
      is_animation_tainted_(is_animation_tainted),
      needs_variable_resolution_(needs_variable_resolution),
      base_url_(base_url.IsValid() ? base_url.GetString() : String()),
      charset_(charset) {
  DCHECK(!tokenized_value.range.AtEnd());
  ConsumeAndUpdateTokens(tokenized_value.range);
}

const CSSValue* CSSVariableData::ParseForSyntax(
    const CSSSyntaxDefinition& syntax,
    SecureContextMode secure_context_mode) const {
  DCHECK(!NeedsVariableResolution());
  // TODO(timloh): This probably needs a proper parser context for
  // relative URL resolution.
  return syntax.Parse(TokenRange(),
                      *StrictCSSParserContext(secure_context_mode),
                      is_animation_tainted_);
}

}  // namespace blink
