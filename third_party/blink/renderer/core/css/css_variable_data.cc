// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_variable_data.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/html/parser/input_stream_preprocessor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

static bool IsFontUnitToken(CSSParserToken token) {
  if (token.GetType() != kDimensionToken) {
    return false;
  }
  switch (token.GetUnitType()) {
    case CSSPrimitiveValue::UnitType::kEms:
    case CSSPrimitiveValue::UnitType::kChs:
    case CSSPrimitiveValue::UnitType::kExs:
    case CSSPrimitiveValue::UnitType::kIcs:
    case CSSPrimitiveValue::UnitType::kCaps:
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
    case CSSPrimitiveValue::UnitType::kRcaps:
      return true;
    default:
      return false;
  }
}

static bool IsLineHeightUnitToken(CSSParserToken token) {
  return token.GetType() == kDimensionToken &&
         token.GetUnitType() == CSSPrimitiveValue::UnitType::kLhs;
}

VariableDataFeatures CSSVariableData::ExtractFeatures(
    const CSSParserToken& token) {
  VariableDataFeatures features =
      static_cast<VariableDataFeatures>(VariableDataFeature::kNone);
  if (IsFontUnitToken(token)) {
    features |=
        static_cast<VariableDataFeatures>(VariableDataFeature::kHasFontUnits);
  }
  if (IsRootFontUnitToken(token)) {
    features |= static_cast<VariableDataFeatures>(
        VariableDataFeature::kHasRootFontUnits);
  }
  if (IsLineHeightUnitToken(token)) {
    features |= static_cast<VariableDataFeatures>(
        VariableDataFeature::kHasLineHeightUnits);
  }
  if (css_parsing_utils::IsDashedFunctionName(token)) {
    features |= static_cast<VariableDataFeatures>(
        VariableDataFeature::kHasDashedFunctions);
  }
  return features;
}

CSSVariableData* CSSVariableData::Create(const String& original_text,
                                         bool is_animation_tainted,
                                         bool is_attr_tainted,
                                         HasReferences has_references) {
  VariableDataFeatures features =
      static_cast<VariableDataFeatures>(VariableDataFeature::kNone);
  if (has_references) {
    features =
        static_cast<VariableDataFeatures>(VariableDataFeature::kHasReferences);
  }
  CSSParserTokenStream stream(original_text);
  while (!stream.AtEnd()) {
    features |= ExtractFeatures(stream.ConsumeRaw());
  }
  return Create(original_text, is_animation_tainted, is_attr_tainted, features);
}

String CSSVariableData::Serialize() const {
  if (length_ > 0 && UNSAFE_TODO(OriginalText()[length_ - 1]) == '\\') {
    // https://drafts.csswg.org/css-syntax/#consume-escaped-code-point
    // '\' followed by EOF is consumed as U+FFFD.
    // https://drafts.csswg.org/css-syntax/#consume-string-token
    // '\' followed by EOF in a string token is ignored.
    //
    // The tokenizer handles both of these cases when returning tokens, but
    // since we're working with the original string, we need to deal with them
    // ourselves.
    StringBuilder serialized_text;
    serialized_text.Append(OriginalText());
    serialized_text.Resize(serialized_text.length() - 1);

    CSSParserTokenStream stream(OriginalText());
    CSSParserTokenType last_token_type = kEOFToken;
    for (;;) {
      CSSParserTokenType token_type = stream.ConsumeRaw().GetType();
      if (token_type == kEOFToken) {
        break;
      }
      last_token_type = token_type;
    }

    if (last_token_type != kStringToken) {
      serialized_text.Append(kReplacementCharacter);
    }

    // Certain token types implicitly include terminators when serialized.
    // https://drafts.csswg.org/cssom/#common-serializing-idioms
    if (last_token_type == kStringToken) {
      serialized_text.Append('"');
    }
    if (last_token_type == kUrlToken) {
      serialized_text.Append(')');
    }

    return serialized_text.ReleaseString();
  }

  return OriginalText().ToString();
}

bool CSSVariableData::EqualsIgnoringAttrTainting(
    const CSSVariableData& other) const {
  return OriginalText() == other.OriginalText();
}

bool CSSVariableData::operator==(const CSSVariableData& other) const {
  return OriginalText() == other.OriginalText() &&
         IsAttrTainted() == other.IsAttrTainted();
}

CSSVariableData::CSSVariableData(PassKey,
                                 StringView original_text,
                                 bool is_animation_tainted,
                                 bool is_attr_tainted,
                                 VariableDataFeatures features)
    : length_(original_text.length()),
      features_(features),
      is_animation_tainted_(is_animation_tainted),
      is_attr_tainted_(is_attr_tainted),
      is_8bit_(original_text.Is8Bit()) {
  // SAFETY: This constructor is only reachable from CSSVariableData::Create()
  // (because it requires a PassKey), which allocates enough memory in
  // AdditionalBytes to hold the string.
  if (is_8bit_) {
    std::ranges::copy(original_text.Span8(),
                      UNSAFE_BUFFERS(reinterpret_cast<LChar*>(this + 1)));
  } else {
    std::ranges::copy(original_text.Span16(),
                      UNSAFE_BUFFERS(reinterpret_cast<UChar*>(this + 1)));
  }
}

const CSSValue* CSSVariableData::ParseForSyntax(
    const CSSSyntaxDefinition& syntax,
    SecureContextMode secure_context_mode,
    CSSParserLocalContext& local_context) const {
  DCHECK(!NeedsVariableResolution());
  // TODO(timloh): This probably needs a proper parser context for
  // relative URL resolution.
  return syntax.Parse(OriginalText(),
                      *StrictCSSParserContext(secure_context_mode),
                      local_context, is_animation_tainted_, is_attr_tainted_);
}

}  // namespace blink
