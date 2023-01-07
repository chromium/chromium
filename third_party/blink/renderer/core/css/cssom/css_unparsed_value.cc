// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_unparsed_value.h"

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/css_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

StringView FindVariableName(CSSParserTokenRange& range) {
  range.ConsumeWhitespace();
  return range.Consume().Value();
}

V8CSSUnparsedSegment*
VariableReferenceValue(const StringView& variable_name,
                       const HeapVector<Member<V8CSSUnparsedSegment>>& tokens
) {
  CSSUnparsedValue* unparsed_value;
  if (tokens.size() == 0)
    unparsed_value = nullptr;
  else
    unparsed_value = CSSUnparsedValue::Create(tokens);

  CSSStyleVariableReferenceValue* variable_reference =
      CSSStyleVariableReferenceValue::Create(variable_name.ToString(),
                                             unparsed_value);
  return MakeGarbageCollected<V8CSSUnparsedSegment>(variable_reference);
}

HeapVector<Member<V8CSSUnparsedSegment>>
ParserTokenRangeToTokens(CSSParserTokenRange range) {
  HeapVector<Member<V8CSSUnparsedSegment>> tokens;
  StringBuilder builder;
  while (!range.AtEnd()) {
    if (range.Peek().FunctionId() == CSSValueID::kVar ||
        range.Peek().FunctionId() == CSSValueID::kEnv) {
      if (!builder.empty()) {
        tokens.push_back(MakeGarbageCollected<V8CSSUnparsedSegment>(
            builder.ReleaseString()));
      }
      CSSParserTokenRange block = range.ConsumeBlock();
      StringView variable_name = FindVariableName(block);
      block.ConsumeWhitespace();
      if (block.Peek().GetType() == CSSParserTokenType::kCommaToken)
        block.Consume();
      tokens.push_back(VariableReferenceValue(variable_name,
                                              ParserTokenRangeToTokens(block)));
    } else {
      range.Consume().Serialize(builder);
    }
  }
  if (!builder.empty()) {
    tokens.push_back(
        MakeGarbageCollected<V8CSSUnparsedSegment>(builder.ReleaseString()));
  }
  return tokens;
}

}  // namespace

CSSUnparsedValue* CSSUnparsedValue::FromCSSValue(
    const CSSVariableReferenceValue& value) {
  DCHECK(value.VariableDataValue());
  return FromCSSVariableData(*value.VariableDataValue());
}

CSSUnparsedValue* CSSUnparsedValue::FromCSSValue(
    const CSSCustomPropertyDeclaration& value) {
  return FromCSSVariableData(value.Value());
}

CSSUnparsedValue* CSSUnparsedValue::FromCSSVariableData(
    const CSSVariableData& value) {
  return CSSUnparsedValue::Create(ParserTokenRangeToTokens(value.TokenRange()));
}

V8CSSUnparsedSegment* CSSUnparsedValue::AnonymousIndexedGetter(
    uint32_t index,
    ExceptionState& exception_state) const {
  if (index < tokens_.size()) {
    return tokens_[index];
  }
  return nullptr;
}

IndexedPropertySetterResult CSSUnparsedValue::AnonymousIndexedSetter(
    uint32_t index,
    V8CSSUnparsedSegment* segment,
    ExceptionState& exception_state) {
  if (index < tokens_.size()) {
    tokens_[index] = segment;
    return IndexedPropertySetterResult::kIntercepted;
  }

  if (index == tokens_.size()) {
    tokens_.push_back(segment);
    return IndexedPropertySetterResult::kIntercepted;
  }

  exception_state.ThrowRangeError(
      ExceptionMessages::IndexOutsideRange<unsigned>(
          "index", index, 0, ExceptionMessages::kInclusiveBound, tokens_.size(),
          ExceptionMessages::kInclusiveBound));
  return IndexedPropertySetterResult::kIntercepted;
}

const CSSValue* CSSUnparsedValue::ToCSSValue() const {
  CSSTokenizer tokenizer(ToString());
  const auto tokens = tokenizer.TokenizeToEOF();
  CSSParserTokenRange range(tokens);

  if (range.AtEnd()) {
    return MakeGarbageCollected<CSSVariableReferenceValue>(
        CSSVariableData::Create());
  }

  // TODO(crbug.com/985028): We should probably propagate the CSSParserContext
  // to here.
  return MakeGarbageCollected<CSSVariableReferenceValue>(
      CSSVariableData::Create({range, StringView()},
                              false /* is_animation_tainted */,
                              false /* needs_variable_resolution */));
}

String CSSUnparsedValue::ToString() const {
  StringBuilder input;

  for (unsigned i = 0; i < tokens_.size(); i++) {
    if (i) {
      input.Append("/**/");
    }
    switch (tokens_[i]->GetContentType()) {
      case V8CSSUnparsedSegment::ContentType::kCSSVariableReferenceValue: {
        const auto* reference_value =
            tokens_[i]->GetAsCSSVariableReferenceValue();
        input.Append("var(");
        input.Append(reference_value->variable());
        if (reference_value->fallback()) {
          input.Append(",");
          input.Append(reference_value->fallback()->ToString());
        }
        input.Append(")");
        break;
      }
      case V8CSSUnparsedSegment::ContentType::kString:
        input.Append(tokens_[i]->GetAsString());
        break;
    }
  }

  return input.ReleaseString();
}

}  // namespace blink
