// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"

namespace blink {

namespace {

bool IsValidVariableReference(CSSParserTokenRange);
bool IsValidEnvVariableReference(CSSParserTokenRange);

// Checks if a token sequence is a valid <declaration-value> [1],
// with the additional restriction that any var()/env() functions (if present)
// must follow their respective grammars as well.
//
// If this function returns true, then it outputs some additional details about
// the token sequence that can be used to determine if it's valid in a given
// situation, e.g. if "var()" is present (has_references=true), then the
// sequence is valid for any property [2].
//
// Braces (i.e. {}) are considered to be "positioned" when they appear
// top-level with non-whitespace tokens to the left or the right.
//
// For example:
//
//   foo {}    =>  Positioned
//   {} foo    =>  Positioned
//   { foo }   =>  Not positioned (the {} covers the whole value).
//   foo [{}]  =>  Not positioned (the {} appears within another block).
//
// Token sequences with "positioned" braces are not valid in standard
// properties, even if var()/env() is present in the value [3].
//
// [1] https://drafts.csswg.org/css-syntax-3/#typedef-declaration-value
// [2] https://drafts.csswg.org/css-variables/#using-variables
// [3] https://github.com/w3c/csswg-drafts/issues/9317
bool IsValidRestrictedDeclarationValue(CSSParserTokenRange range,
                                       bool& has_references,
                                       bool& has_positioned_braces) {
  size_t block_stack_size = 0;

  // https://drafts.csswg.org/css-syntax/#component-value
  size_t top_level_component_values = 0;
  bool has_top_level_brace = false;

  while (!range.AtEnd()) {
    if (RuntimeEnabledFeatures::CSSNestingIdentEnabled()) {
      if (block_stack_size == 0 && range.Peek().GetType() != kWhitespaceToken) {
        ++top_level_component_values;
        if (range.Peek().GetType() == kLeftBraceToken) {
          has_top_level_brace = true;
        }
      }
    }

    // First check if this is a valid variable reference, then handle the next
    // token accordingly.
    if (range.Peek().GetBlockType() == CSSParserToken::kBlockStart) {
      const CSSParserToken& token = range.Peek();

      // A block may have both var and env references. They can also be nested
      // and used as fallbacks.
      switch (token.FunctionId()) {
        case CSSValueID::kVar:
          if (!IsValidVariableReference(range.ConsumeBlock())) {
            return false;  // Invalid reference.
          }
          has_references = true;
          continue;
        case CSSValueID::kEnv:
          if (!IsValidEnvVariableReference(range.ConsumeBlock())) {
            return false;  // Invalid reference.
          }
          has_references = true;
          continue;
        default:
          break;
      }
    }

    const CSSParserToken& token = range.Consume();
    if (token.GetBlockType() == CSSParserToken::kBlockStart) {
      ++block_stack_size;
    } else if (token.GetBlockType() == CSSParserToken::kBlockEnd) {
      --block_stack_size;
    } else {
      switch (token.GetType()) {
        case kDelimiterToken: {
          if (token.Delimiter() == '!' && block_stack_size == 0) {
            return false;
          }
          break;
        }
        case kRightParenthesisToken:
        case kRightBraceToken:
        case kRightBracketToken:
        case kBadStringToken:
        case kBadUrlToken:
          return false;
        case kSemicolonToken:
          if (block_stack_size == 0) {
            return false;
          }
          break;
        default:
          break;
      }
    }
  }

  if (RuntimeEnabledFeatures::CSSNestingIdentEnabled()) {
    has_positioned_braces =
        has_top_level_brace && (top_level_component_values > 1);
  }

  return true;
}

bool IsValidVariableReference(CSSParserTokenRange range) {
  range.ConsumeWhitespace();
  if (!CSSVariableParser::IsValidVariableName(
          range.ConsumeIncludingWhitespace())) {
    return false;
  }
  if (range.AtEnd()) {
    return true;
  }

  if (range.Consume().GetType() != kCommaToken) {
    return false;
  }

  bool has_references = false;
  bool has_positioned_braces = false;
  return IsValidRestrictedDeclarationValue(range, has_references,
                                           has_positioned_braces);
}

bool IsValidEnvVariableReference(CSSParserTokenRange range) {
  range.ConsumeWhitespace();
  auto token = range.ConsumeIncludingWhitespace();
  if (token.GetType() != CSSParserTokenType::kIdentToken) {
    return false;
  }
  if (range.AtEnd()) {
    return true;
  }

  if (RuntimeEnabledFeatures::ViewportSegmentsEnabled()) {
    // Consume any number of integer values that indicate the indices for a
    // multi-dimensional variable.
    token = range.ConsumeIncludingWhitespace();
    while (token.GetType() == kNumberToken) {
      if (token.GetNumericValueType() != kIntegerValueType) {
        return false;
      }
      if (token.NumericValue() < 0.) {
        return false;
      }
      token = range.ConsumeIncludingWhitespace();
    }

    // If that's all we had (either ident then integers or just the ident) then
    // the env() is valid.
    if (token.GetType() == kEOFToken) {
      return true;
    }
  } else {
    token = range.Consume();
  }

  // Otherwise we need a comma followed by an optional fallback value.
  if (token.GetType() != kCommaToken) {
    return false;
  }

  bool has_references = false;
  bool has_positioned_braces = false;
  return IsValidRestrictedDeclarationValue(range, has_references,
                                           has_positioned_braces);
}

bool IsValidVariable(CSSParserTokenRange range,
                     bool& has_references,
                     bool& has_positioned_braces) {
  has_references = false;
  has_positioned_braces = false;
  return IsValidRestrictedDeclarationValue(range, has_references,
                                           has_positioned_braces);
}

CSSValue* ParseCSSWideValue(CSSParserTokenRange range) {
  range.ConsumeWhitespace();
  CSSValue* value = css_parsing_utils::ConsumeCSSWideKeyword(range);
  return range.AtEnd() ? value : nullptr;
}

}  // namespace

bool CSSVariableParser::IsValidVariableName(const CSSParserToken& token) {
  if (token.GetType() != kIdentToken) {
    return false;
  }

  StringView value = token.Value();
  return value.length() >= 3 && value[0] == '-' && value[1] == '-';
}

bool CSSVariableParser::IsValidVariableName(const String& string) {
  return string.length() >= 3 && string[0] == '-' && string[1] == '-';
}

bool CSSVariableParser::ContainsValidVariableReferences(
    CSSParserTokenRange range) {
  bool has_references;
  bool has_positioned_braces;
  return IsValidVariable(range, has_references, has_positioned_braces) &&
         has_references && !has_positioned_braces;
}

CSSValue* CSSVariableParser::ParseDeclarationIncludingCSSWide(
    const CSSTokenizedValue& tokenized_value,
    bool is_animation_tainted,
    const CSSParserContext& context) {
  if (CSSValue* css_wide = ParseCSSWideValue(tokenized_value.range)) {
    return css_wide;
  }
  return ParseDeclarationValue(tokenized_value, is_animation_tainted, context);
}

CSSCustomPropertyDeclaration* CSSVariableParser::ParseDeclarationValue(
    const CSSTokenizedValue& tokenized_value,
    bool is_animation_tainted,
    const CSSParserContext& context) {
  bool has_references;
  bool has_positioned_braces_ignored;
  // Note that positioned braces are allowed in custom property declarations.
  if (!IsValidVariable(tokenized_value.range, has_references,
                       has_positioned_braces_ignored)) {
    return nullptr;
  }
  if (tokenized_value.text.length() > CSSVariableData::kMaxVariableBytes) {
    return nullptr;
  }

  StringView text = StripTrailingWhitespaceAndComments(tokenized_value.text);
  return MakeGarbageCollected<CSSCustomPropertyDeclaration>(
      CSSVariableData::Create(CSSTokenizedValue{tokenized_value.range, text},
                              is_animation_tainted, has_references),
      &context);
}

CSSVariableReferenceValue* CSSVariableParser::ParseVariableReferenceValue(
    CSSTokenizedValue value,
    const CSSParserContext& context,
    bool is_animation_tainted) {
  bool has_references;
  bool has_positioned_braces;
  if (!IsValidVariable(value.range, has_references, has_positioned_braces)) {
    return nullptr;
  }
  if (has_positioned_braces) {
    return nullptr;
  }
  if (ParseCSSWideValue(value.range)) {
    return nullptr;
  }
  return MakeGarbageCollected<CSSVariableReferenceValue>(
      CSSVariableData::Create(value, is_animation_tainted, has_references),
      context);
}

StringView CSSVariableParser::StripTrailingWhitespaceAndComments(
    StringView text) {
  // Comments may (unfortunately!) be unfinished, so we can't rely on
  // looking for */; if there's /* anywhere, we'll need to scan through
  // the string from the start. We do a very quick heuristic first
  // to get rid of the most common cases.
  //
  // TODO(sesse): In the cases where we've tokenized the string before
  // (i.e. not CSSOM, where we just get a string), we know we can't
  // have unfinished comments, so consider piping that knowledge all
  // the way through here.
  if (text.Is8Bit() &&
      memchr(text.Characters8(), '/', text.length()) == nullptr) {
    // No comments, so we can strip whitespace only.
    while (!text.empty() && IsHTMLSpace(text[text.length() - 1])) {
      text = StringView(text, 0, text.length() - 1);
    }
    return text;
  }

  wtf_size_t string_len = 0;
  bool in_comment = false;
  for (wtf_size_t i = 0; i < text.length(); ++i) {
    if (in_comment) {
      // See if we can end this comment.
      if (text[i] == '*' && i + 1 < text.length() && text[i + 1] == '/') {
        ++i;
        in_comment = false;
      }
    } else {
      // See if we must start a comment.
      if (text[i] == '/' && i + 1 < text.length() && text[i + 1] == '*') {
        ++i;
        in_comment = true;
      } else if (!IsHTMLSpace(text[i])) {
        // A non-space outside a comment, so the string
        // must go at least to here.
        string_len = i + 1;
      }
    }
  }

  StringView ret = StringView(text, 0, string_len);

  // Leading whitespace should already have been stripped.
  // (This test needs to be after we stripped trailing spaces,
  // or we could look at trailing space believing it was leading.)
  DCHECK(ret.empty() || !IsHTMLSpace(ret[0]));

  return ret;
}

}  // namespace blink
