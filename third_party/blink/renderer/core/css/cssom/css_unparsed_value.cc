// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_unparsed_value.h"

#include "css_style_value.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_variable_reference_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

String FindVariableName(CSSParserTokenStream& stream) {
  stream.ConsumeWhitespace();
  if (stream.Peek().GetType() == CSSParserTokenType::kIdentToken) {
    return stream.Consume().Value().ToString();
  } else {
    return {};
  }
}

V8CSSUnparsedSegment* VariableReferenceValue(
    const StringView& variable_name,
    const HeapVector<Member<V8CSSUnparsedSegment>>& segments) {
  CSSUnparsedValue* unparsed_value;
  if (segments.size() == 0) {
    unparsed_value = nullptr;
  } else {
    unparsed_value = CSSUnparsedValue::Create(segments);
  }

  CSSStyleVariableReferenceValue* variable_reference =
      CSSStyleVariableReferenceValue::Create(variable_name.ToString(),
                                             unparsed_value);
  if (!variable_reference) {
    // TODO(sesse): Plumb the ExceptionState here so that we can use
    // the Create() variant that properly throws an exception.
    return nullptr;
  }
  return MakeGarbageCollected<V8CSSUnparsedSegment>(variable_reference);
}

HeapVector<Member<V8CSSUnparsedSegment>> ParserTokenStreamToTokens(
    CSSParserTokenStream& stream) {
  int nesting_level = 0;
  HeapVector<Member<V8CSSUnparsedSegment>> segments;
  StringBuilder builder;
  while (stream.Peek().GetType() != kEOFToken) {
    if (stream.Peek().FunctionId() == CSSValueID::kVar ||
        stream.Peek().FunctionId() == CSSValueID::kEnv) {
      if (!builder.empty()) {
        segments.push_back(MakeGarbageCollected<V8CSSUnparsedSegment>(
            builder.ReleaseString()));
      }

      CSSParserTokenStream::BlockGuard guard(stream);
      String variable_name = FindVariableName(stream);
      stream.ConsumeWhitespace();
      if (stream.Peek().GetType() == CSSParserTokenType::kCommaToken) {
        stream.Consume();
      }
      V8CSSUnparsedSegment* ref = VariableReferenceValue(
          variable_name, ParserTokenStreamToTokens(stream));
      if (!ref) {
        break;
      }
      segments.push_back(ref);
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
    segments.push_back(
        MakeGarbageCollected<V8CSSUnparsedSegment>(builder.ReleaseString()));
  }
  return segments;
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
  if (index < segments_.size()) {
    return segments_[index].Get();
  }
  return nullptr;
}

IndexedPropertySetterResult CSSUnparsedValue::AnonymousIndexedSetter(
    uint32_t index,
    V8CSSUnparsedSegment* segment,
    ExceptionState& exception_state) {
  if (index < segments_.size()) {
    segments_[index] = segment;
    return IndexedPropertySetterResult::kIntercepted;
  }

  if (index == segments_.size()) {
    segments_.push_back(segment);
    return IndexedPropertySetterResult::kIntercepted;
  }

  exception_state.ThrowRangeError(
      ExceptionMessages::IndexOutsideRange<unsigned>(
          "index", index, 0, ExceptionMessages::kInclusiveBound,
          segments_.size(), ExceptionMessages::kInclusiveBound));
  return IndexedPropertySetterResult::kIntercepted;
}

bool CSSUnparsedValue::IsValidDeclarationValue() const {
  return IsValidDeclarationValue(ToStringInternal());
}

const CSSValue* CSSUnparsedValue::ToCSSValue() const {
  String unparsed_string = ToStringInternal();

  if (unparsed_string.IsNull()) {
    return MakeGarbageCollected<CSSUnparsedDeclarationValue>(
        MakeGarbageCollected<CSSVariableData>());
  }

  CHECK(IsValidDeclarationValue(unparsed_string));
  // The call to IsValidDeclarationValue() above also creates a CSSVariableData
  // to carry out its check. It would be nice to use that here, but WPTs
  // expect leading whitespace to be preserved, even though it's not possible
  // to create such declaration values normally.
  CSSVariableData* variable_data =
      CSSVariableData::Create(unparsed_string,
                              /*is_animation_tainted=*/false,
                              /*is_attr_tainted=*/false,
                              /*needs_variable_resolution=*/false);

  // TODO(crbug.com/985028): We should probably propagate the CSSParserContext
  // to here.
  return MakeGarbageCollected<CSSUnparsedDeclarationValue>(variable_data);
}

bool CSSUnparsedValue::IsValidDeclarationValue(const String& string) {
  CSSParserTokenStream stream(string);
  bool important_unused;
  // This checks that the value does not violate the "argument grammar" [1]
  // of any substitution functions, and that it is a valid <declaration-value>
  // otherwise.
  //
  // [1] https://drafts.csswg.org/css-values-5/#argument-grammar
  //
  // TODO(andruud): 'restricted_value' depends on the destination property.
  return CSSVariableParser::ConsumeUnparsedDeclaration(
      stream,
      /*allow_important_annotation=*/false,
      /*is_animation_tainted=*/false,
      /*must_contain_variable_reference=*/false,
      /*restricted_value=*/false,
      /*comma_ends_declaration=*/false, important_unused,
      *StrictCSSParserContext(SecureContextMode::kInsecureContext));
}

String CSSUnparsedValue::ToStringInternal() const {
  String serialized = SerializeSegments();

  // The serialization above defensively inserted /**/ between segments
  // to make sure that e.g. ['foo', 'bar'] does not collapse into 'foobar'.
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
  CSSParserTokenStream stream(serialized);
  if (stream.AtEnd()) {
    return g_null_atom;
  }
  CSSParserToken token = stream.ConsumeRaw();
  token.Serialize(builder);
  while (!stream.Peek().IsEOF()) {
    if (NeedsInsertedComment(token, stream.Peek())) {
      builder.Append("/**/");
    }
    token = stream.ConsumeRaw();
    token.Serialize(builder);
  }
  return builder.ReleaseString();
}

String CSSUnparsedValue::SerializeSegments() const {
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
  for (unsigned i = 0; i < segments_.size(); i++) {
    if (i) {
      builder.Append("/**/");
    }
    switch (segments_[i]->GetContentType()) {
      case V8CSSUnparsedSegment::ContentType::kCSSVariableReferenceValue: {
        const auto* reference_value =
            segments_[i]->GetAsCSSVariableReferenceValue();
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
        builder.Append(segments_[i]->GetAsString());
        break;
    }
  }
  values_on_stack.erase(this);
  return true;
}

}  // namespace blink
