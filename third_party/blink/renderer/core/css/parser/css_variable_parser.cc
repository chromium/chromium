// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

bool IsValidVariableReference(CSSParserTokenRange);
bool IsValidEnvVariableReference(CSSParserTokenRange);

bool ClassifyBlock(CSSParserTokenRange range, bool& has_references) {
  size_t block_stack_size = 0;

  while (!range.AtEnd()) {
    // First check if this is a valid variable reference, then handle the next
    // token accordingly.
    if (range.Peek().GetBlockType() == CSSParserToken::kBlockStart) {
      const CSSParserToken& token = range.Peek();

      // A block may have both var and env references. They can also be nested
      // and used as fallbacks.
      switch (token.FunctionId()) {
        case CSSValueID::kVar:
          if (!IsValidVariableReference(range.ConsumeBlock()))
            return false;  // Invalid reference.
          has_references = true;
          continue;
        case CSSValueID::kEnv:
          if (!IsValidEnvVariableReference(range.ConsumeBlock()))
            return false;  // Invalid reference.
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
          if (token.Delimiter() == '!' && block_stack_size == 0)
            return false;
          break;
        }
        case kRightParenthesisToken:
        case kRightBraceToken:
        case kRightBracketToken:
        case kBadStringToken:
        case kBadUrlToken:
          return false;
        case kSemicolonToken:
          if (block_stack_size == 0)
            return false;
          break;
        default:
          break;
      }
    }
  }
  return true;
}

bool IsValidVariableReference(CSSParserTokenRange range) {
  range.ConsumeWhitespace();
  if (!CSSVariableParser::IsValidVariableName(
          range.ConsumeIncludingWhitespace()))
    return false;
  if (range.AtEnd())
    return true;

  if (range.Consume().GetType() != kCommaToken)
    return false;
  if (range.AtEnd())
    return false;

  bool has_references = false;
  return ClassifyBlock(range, has_references);
}

bool IsValidEnvVariableReference(CSSParserTokenRange range) {
  range.ConsumeWhitespace();
  if (range.ConsumeIncludingWhitespace().GetType() !=
      CSSParserTokenType::kIdentToken)
    return false;
  if (range.AtEnd())
    return true;

  if (range.Consume().GetType() != kCommaToken)
    return false;
  if (range.AtEnd())
    return false;

  bool has_references = false;
  return ClassifyBlock(range, has_references);
}

CSSValueID ClassifyVariableRange(CSSParserTokenRange range,
                                 bool& has_references) {
  has_references = false;

  range.ConsumeWhitespace();
  if (range.Peek().GetType() == kIdentToken) {
    CSSValueID id = range.ConsumeIncludingWhitespace().Id();
    if (range.AtEnd() &&
        (id == CSSValueID::kInherit || id == CSSValueID::kInitial ||
         id == CSSValueID::kUnset))
      return id;
  }

  if (ClassifyBlock(range, has_references))
    return CSSValueID::kInternalVariableValue;
  return CSSValueID::kInvalid;
}

}  // namespace

bool CSSVariableParser::IsValidVariableName(const CSSParserToken& token) {
  if (token.GetType() != kIdentToken)
    return false;

  StringView value = token.Value();
  return value.length() >= 2 && value[0] == '-' && value[1] == '-';
}

bool CSSVariableParser::IsValidVariableName(const String& string) {
  return string.length() >= 2 && string[0] == '-' && string[1] == '-';
}

bool CSSVariableParser::ContainsValidVariableReferences(
    CSSParserTokenRange range) {
  bool has_references;
  CSSValueID type = ClassifyVariableRange(range, has_references);
  return type == CSSValueID::kInternalVariableValue && has_references;
}

CSSCustomPropertyDeclaration* CSSVariableParser::ParseDeclarationValue(
    const AtomicString& variable_name,
    CSSParserTokenRange range,
    bool is_animation_tainted,
    const CSSParserContext& context) {
  if (range.AtEnd())
    return nullptr;

  bool has_references;
  CSSValueID type = ClassifyVariableRange(range, has_references);

  if (!IsValidCSSValueID(type))
    return nullptr;
  if (type == CSSValueID::kInternalVariableValue) {
    return MakeGarbageCollected<CSSCustomPropertyDeclaration>(
        variable_name,
        CSSVariableData::Create(range, is_animation_tainted, has_references,
                                context.BaseURL(), context.Charset()));
  }
  return MakeGarbageCollected<CSSCustomPropertyDeclaration>(variable_name,
                                                            type);
}

CSSVariableReferenceValue* CSSVariableParser::ParseRegisteredPropertyValue(
    CSSParserTokenRange range,
    const CSSParserContext& context,
    bool require_var_reference,
    bool is_animation_tainted) {
  if (range.AtEnd())
    return nullptr;

  bool has_references;
  CSSValueID type = ClassifyVariableRange(range, has_references);

  if (type != CSSValueID::kInternalVariableValue)
    return nullptr;  // Invalid or a css-wide keyword
  if (require_var_reference && !has_references)
    return nullptr;
  return MakeGarbageCollected<CSSVariableReferenceValue>(
      CSSVariableData::Create(range, is_animation_tainted, has_references,
                              context.BaseURL(), context.Charset()),
      context);
}

}  // namespace blink
