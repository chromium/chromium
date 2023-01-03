// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_variable_data.h"

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

template <typename CharacterType>
static void UpdateTokens(const CSSParserTokenRange& range,
                         const String& backing_string,
                         CSSParserToken* result) {
  const CharacterType* current_offset =
      backing_string.GetCharacters<CharacterType>();
  for (const CSSParserToken& token : range) {
    if (token.HasStringBacking()) {
      unsigned length = token.Value().length();
      StringView string(current_offset, length);
      new (result++) CSSParserToken(token.CopyWithUpdatedString(string));
      current_offset += length;
    } else {
      new (result++) CSSParserToken(token);
    }
  }
  DCHECK(current_offset == backing_string.GetCharacters<CharacterType>() +
                               backing_string.length());
}

static bool IsFontUnitToken(CSSParserToken token) {
  if (token.GetType() != kDimensionToken) {
    return false;
  }
  switch (token.GetUnitType()) {
    case CSSPrimitiveValue::UnitType::kEms:
    case CSSPrimitiveValue::UnitType::kChs:
    case CSSPrimitiveValue::UnitType::kExs:
    case CSSPrimitiveValue::UnitType::kIcs:
      return true;
    default:
      return false;
  }
}

static bool IsRootFontUnitToken(CSSParserToken token) {
  if (token.GetType() != kDimensionToken) {
    return false;
  }
  switch (token.GetUnitType()) {
    case CSSPrimitiveValue::UnitType::kRems:
    case CSSPrimitiveValue::UnitType::kRexs:
    case CSSPrimitiveValue::UnitType::kRchs:
    case CSSPrimitiveValue::UnitType::kRics:
    case CSSPrimitiveValue::UnitType::kRlhs:
      return true;
    default:
      return false;
  }
}

static bool IsLineHeightUnitToken(CSSParserToken token) {
  return token.GetType() == kDimensionToken &&
         token.GetUnitType() == CSSPrimitiveValue::UnitType::kLhs;
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
      DCHECK_NE(0u, num_tokens_);
      const CSSParserToken& last = TokenInternalPtr()[num_tokens_ - 1];
      if (last.GetType() != kStringToken) {
        serialized_text.Append(kReplacementCharacter);
      }

      // Certain token types implicitly include terminators when serialized.
      // https://drafts.csswg.org/cssom/#common-serializing-idioms
      if (last.GetType() == kStringToken) {
        serialized_text.Append('"');
      }
      if (last.GetType() == kUrlToken) {
        serialized_text.Append(')');
      }

      return serialized_text.ReleaseString();
    }

    return original_text_;
  }
  return TokenRange().Serialize();
}

bool CSSVariableData::operator==(const CSSVariableData& other) const {
  return base::ranges::equal(Tokens(), other.Tokens());
}

void CSSVariableData::ConsumeAndUpdateTokens(const CSSParserTokenRange& range) {
  DCHECK_EQ(num_tokens_, 0u);
  DCHECK(backing_string_.empty());
  StringBuilder string_builder;
  CSSParserTokenRange local_range = range;

  while (!local_range.AtEnd()) {
    CSSParserToken token = local_range.Consume();
    if (token.HasStringBacking()) {
      string_builder.Append(token.Value());
    }
    has_font_units_ |= IsFontUnitToken(token);
    has_root_font_units_ |= IsRootFontUnitToken(token);
    has_line_height_units_ |= IsLineHeightUnitToken(token);
    ++num_tokens_;
  }
  backing_string_ = string_builder.ToAtomicString();
  if (backing_string_.Is8Bit()) {
    UpdateTokens<LChar>(range, backing_string_, TokenInternalPtr());
  } else {
    UpdateTokens<UChar>(range, backing_string_, TokenInternalPtr());
  }
}

#if EXPENSIVE_DCHECKS_ARE_ON()

namespace {

template <typename CharacterType>
bool IsSubspan(base::span<const CharacterType> inner,
               base::span<const CharacterType> outer) {
  // Note that base::span uses CheckedContiguousIterator, which restricts
  // which comparisons are allowed. Therefore we must avoid begin()/end() here.
  return inner.data() >= outer.data() &&
         (inner.data() + inner.size()) <= (outer.data() + outer.size());
}

bool TokenValueIsBacked(const CSSParserToken& token,
                        const String& backing_string) {
  StringView value = token.Value();
  if (value.Is8Bit() != backing_string.Is8Bit()) {
    return false;
  }
  return value.Is8Bit() ? IsSubspan(value.Span8(), backing_string.Span8())
                        : IsSubspan(value.Span16(), backing_string.Span16());
}

}  // namespace

void CSSVariableData::VerifyStringBacking() const {
  for (const CSSParserToken& token : Tokens()) {
    DCHECK(!token.HasStringBacking() ||
           TokenValueIsBacked(token, backing_string_))
        << "Token value is not backed: " << token.Value().ToString();
  }
}

#endif  // EXPENSIVE_DCHECKS_ARE_ON()

CSSVariableData::CSSVariableData(const CSSTokenizedValue& tokenized_value,
                                 bool is_animation_tainted,
                                 bool needs_variable_resolution)
    : original_text_(tokenized_value.text.ToString()),
      is_animation_tainted_(is_animation_tainted),
      needs_variable_resolution_(needs_variable_resolution) {
  ConsumeAndUpdateTokens(tokenized_value.range);
#if EXPENSIVE_DCHECKS_ARE_ON()
  VerifyStringBacking();
#endif  // EXPENSIVE_DCHECKS_ARE_ON()
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
