// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_unparsed_value.h"

#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

String FindVariableName(CSSParserTokenStream& stream) {
  stream.ConsumeWhitespace();
  return stream.Consume().Value().ToString();
}

V8CSSUnparsedSegment* VariableReferenceValue(
    const StringView& variable_name,
    const HeapVector<Member<V8CSSUnparsedSegment>>& tokens) {
  CSSUnparsedValue* unparsed_value;
  if (tokens.size() == 0) {
    unparsed_value = nullptr;
  } else {
    unparsed_value = CSSUnparsedValue::Create(tokens);
  }

  CSSStyleVariableReferenceValue* variable_reference =
      CSSStyleVariableReferenceValue::Create(variable_name.ToString(),
                                             unparsed_value);
  return MakeGarbageCollected<V8CSSUnparsedSegment>(variable_reference);
}

HeapVector<Member<V8CSSUnparsedSegment>> ParserTokenStreamToTokens(
    CSSParserTokenStream& stream) {
  int nesting_level = 0;
  HeapVector<Member<V8CSSUnparsedSegment>> tokens;
  StringBuilder builder;
  while (stream.Peek().GetType() != kEOFToken) {
    if (stream.Peek().FunctionId() == CSSValueID::kVar ||
        stream.Peek().FunctionId() == CSSValueID::kEnv) {
      if (!builder.empty()) {
        tokens.push_back(MakeGarbageCollected<V8CSSUnparsedSegment>(
            builder.ReleaseString()));
      }

      CSSParserTokenStream::BlockGuard guard(stream);
      String variable_name = FindVariableName(stream);
      stream.ConsumeWhitespace();
      if (stream.Peek().GetType() == CSSParserTokenType::kCommaToken) {
        stream.Consume();
      }
      tokens.push_back(VariableReferenceValue(
          variable_name, ParserTokenStreamToTokens(stream)));
    } else {
      if (stream.Peek().GetBlockType() == CSSParserToken::kBlockStart) {
        ++nesting_level;
      } else if (stream.Peek().GetBlockType() == CSSParserToken::kBlockEnd) {
        --nesting_level;
        if (nesting_level < 0) {
          // Don't include the end right-paren.
          break;
        }
      }
      stream.ConsumeRaw().Serialize(builder);
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
    const CSSUnparsedDeclarationValue& value) {
  DCHECK(value.VariableDataValue());
  return FromCSSVariableData(*value.VariableDataValue());
}

CSSUnparsedValue* CSSUnparsedValue::FromCSSVariableData(
    const CSSVariableData& value) {
  CSSParserTokenStream stream(value.OriginalText());
  return CSSUnparsedValue::Create(ParserTokenStreamToTokens(stream));
}

V8CSSUnparsedSegment* CSSUnparsedValue::AnonymousIndexedGetter(
    uint32_t index,
    ExceptionState& exception_state) const {
  if (index < tokens_.size()) {
    return tokens_[index].Get();
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
  String unparsed_string = ToUnparsedString();
  CSSParserTokenStream stream(unparsed_string);

  if (stream.AtEnd()) {
    return MakeGarbageCollected<CSSUnparsedDeclarationValue>(
        MakeGarbageCollected<CSSVariableData>());
  }

  // The string we just parsed has /**/ inserted between every token
  // to make sure we get back the correct sequence of tokens.
  // The spec mentions nothing of the sort:
  // https://drafts.css-houdini.org/css-typed-om-1/#unparsedvalue-serialization
  //
  // However, inserting /**/ is required in some places, or round-tripping
  // of properties would not work. This is acknowledged as a mistake in the
  // spec:
  // https://github.com/w3c/css-houdini-drafts/issues/1021
  //
  // Thus, we insert empty comments but only when needed to avoid changing
  // the meaning. If this CSSUnparsedValue came from serializing a string,
  // the original contents of any comments will be lost, but Typed OM does
  // not have anywhere to store that kind of data, so it is expected.
  StringBuilder builder;
  CSSParserToken token = stream.ConsumeRaw();
  token.Serialize(builder);
  while (!stream.Peek().IsEOF()) {
    if (NeedsInsertedComment(token, stream.Peek())) {
      builder.Append("/**/");
    }
    token = stream.ConsumeRaw();
    token.Serialize(builder);
  }
  String original_text = builder.ReleaseString();

  // TODO(crbug.com/985028): We should probably propagate the CSSParserContext
  // to here.
  return MakeGarbageCollected<CSSUnparsedDeclarationValue>(
      CSSVariableData::Create(original_text, false /* is_animation_tainted */,
                              false /* needs_variable_resolution */));
}

String CSSUnparsedValue::ToUnparsedString() const {
  StringBuilder builder;
  HeapHashSet<Member<const CSSUnparsedValue>> values_on_stack;
  if (AppendUnparsedString(builder, values_on_stack)) {
    return builder.ReleaseString();
  }
  return g_empty_atom;
}

bool CSSUnparsedValue::AppendUnparsedString(
    StringBuilder& builder,
    HeapHashSet<Member<const CSSUnparsedValue>>& values_on_stack) const {
  if (values_on_stack.Contains(this)) {
    return false;  // Cycle.
  }
  values_on_stack.insert(this);
  for (unsigned i = 0; i < tokens_.size(); i++) {
    if (i) {
      builder.Append("/**/");
    }
    switch (tokens_[i]->GetContentType()) {
      case V8CSSUnparsedSegment::ContentType::kCSSVariableReferenceValue: {
        const auto* reference_value =
            tokens_[i]->GetAsCSSVariableReferenceValue();
        builder.Append("var(");
        builder.Append(reference_value->variable());
        if (reference_value->fallback()) {
          builder.Append(",");
          if (!reference_value->fallback()->AppendUnparsedString(
                  builder, values_on_stack)) {
            return false;  // Cycle.
          }
        }
        builder.Append(")");
        break;
      }
      case V8CSSUnparsedSegment::ContentType::kString:
        builder.Append(tokens_[i]->GetAsString());
        break;
    }
  }
  values_on_stack.erase(this);
  return true;
}

}  // namespace blink
