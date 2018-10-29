// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/content.h"

#include "third_party/blink/renderer/core/css/css_counter_value.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace {

CSSValue* ConsumeAttr(CSSParserTokenRange args,
                      const CSSParserContext& context) {
  if (args.Peek().GetType() != kIdentToken)
    return nullptr;

  AtomicString attr_name =
      args.ConsumeIncludingWhitespace().Value().ToAtomicString();
  if (!args.AtEnd())
    return nullptr;

  if (context.IsHTMLDocument())
    attr_name = attr_name.LowerASCII();

  CSSFunctionValue* attr_value = CSSFunctionValue::Create(CSSValueAttr);
  attr_value->Append(*CSSCustomIdentValue::Create(attr_name));
  return attr_value;
}

CSSValue* ConsumeCounterContent(CSSParserTokenRange args, bool counters) {
  CSSCustomIdentValue* identifier =
      CSSPropertyParserHelpers::ConsumeCustomIdent(args);
  if (!identifier)
    return nullptr;

  CSSStringValue* separator = nullptr;
  if (!counters) {
    separator = CSSStringValue::Create(String());
  } else {
    if (!CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args) ||
        args.Peek().GetType() != kStringToken)
      return nullptr;
    separator = CSSStringValue::Create(
        args.ConsumeIncludingWhitespace().Value().ToString());
  }

  CSSIdentifierValue* list_style = nullptr;
  if (CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(args)) {
    CSSValueID id = args.Peek().Id();
    if ((id != CSSValueNone &&
         (id < CSSValueDisc || id > CSSValueKatakanaIroha)))
      return nullptr;
    list_style = CSSPropertyParserHelpers::ConsumeIdent(args);
  } else {
    list_style = CSSIdentifierValue::Create(CSSValueDecimal);
  }

  if (!args.AtEnd())
    return nullptr;
  return cssvalue::CSSCounterValue::Create(identifier, list_style, separator);
}

}  // namespace
namespace CSSLonghand {

const CSSValue* Content::ParseSingleValue(CSSParserTokenRange& range,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  if (CSSPropertyParserHelpers::IdentMatches<CSSValueNone, CSSValueNormal>(
          range.Peek().Id()))
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  CSSValueList* values = CSSValueList::CreateSpaceSeparated();

  do {
    CSSValue* parsed_value =
        CSSPropertyParserHelpers::ConsumeImage(range, &context);
    if (!parsed_value) {
      parsed_value = CSSPropertyParserHelpers::ConsumeIdent<
          CSSValueOpenQuote, CSSValueCloseQuote, CSSValueNoOpenQuote,
          CSSValueNoCloseQuote>(range);
    }
    if (!parsed_value)
      parsed_value = CSSPropertyParserHelpers::ConsumeString(range);
    if (!parsed_value) {
      if (range.Peek().FunctionId() == CSSValueAttr) {
        parsed_value = ConsumeAttr(
            CSSPropertyParserHelpers::ConsumeFunction(range), context);
      } else if (range.Peek().FunctionId() == CSSValueCounter) {
        parsed_value = ConsumeCounterContent(
            CSSPropertyParserHelpers::ConsumeFunction(range), false);
      } else if (range.Peek().FunctionId() == CSSValueCounters) {
        parsed_value = ConsumeCounterContent(
            CSSPropertyParserHelpers::ConsumeFunction(range), true);
      }
      if (!parsed_value)
        return nullptr;
    }
    values->Append(*parsed_value);
  } while (!range.AtEnd());

  return values;
}

const CSSValue* Content::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForContentData(style);
}

void Content::ApplyInitial(StyleResolverState& state) const {
  state.Style()->SetContent(nullptr);
}

void Content::ApplyInherit(StyleResolverState& state) const {
  // FIXME: In CSS3, it will be possible to inherit content. In CSS2 it is not.
  // This note is a reminder that eventually "inherit" needs to be supported.
}

void Content::ApplyValue(StyleResolverState& state,
                         const CSSValue& value) const {
  if (value.IsIdentifierValue()) {
    DCHECK(ToCSSIdentifierValue(value).GetValueID() == CSSValueNormal ||
           ToCSSIdentifierValue(value).GetValueID() == CSSValueNone);
    state.Style()->SetContent(nullptr);
    return;
  }

  ContentData* first_content = nullptr;
  ContentData* prev_content = nullptr;
  for (auto& item : ToCSSValueList(value)) {
    ContentData* next_content = nullptr;
    if (item->IsImageGeneratorValue() || item->IsImageSetValue() ||
        item->IsImageValue()) {
      next_content =
          ContentData::Create(state.GetStyleImage(CSSPropertyContent, *item));
    } else if (item->IsCounterValue()) {
      const cssvalue::CSSCounterValue* counter_value =
          cssvalue::ToCSSCounterValue(item.Get());
      const auto list_style_type =
          CssValueIDToPlatformEnum<EListStyleType>(counter_value->ListStyle());
      std::unique_ptr<CounterContent> counter =
          std::make_unique<CounterContent>(
              AtomicString(counter_value->Identifier()), list_style_type,
              AtomicString(counter_value->Separator()));
      next_content = ContentData::Create(std::move(counter));
    } else if (item->IsIdentifierValue()) {
      QuoteType quote_type;
      switch (ToCSSIdentifierValue(*item).GetValueID()) {
        default:
          NOTREACHED();
          FALLTHROUGH;
        case CSSValueOpenQuote:
          quote_type = QuoteType::kOpen;
          break;
        case CSSValueCloseQuote:
          quote_type = QuoteType::kClose;
          break;
        case CSSValueNoOpenQuote:
          quote_type = QuoteType::kNoOpen;
          break;
        case CSSValueNoCloseQuote:
          quote_type = QuoteType::kNoClose;
          break;
      }
      next_content = ContentData::Create(quote_type);
    } else {
      String string;
      if (item->IsFunctionValue()) {
        const CSSFunctionValue* function_value = ToCSSFunctionValue(item.Get());
        DCHECK_EQ(function_value->FunctionType(), CSSValueAttr);
        state.Style()->SetHasAttrContent();
        // TODO: Can a namespace be specified for an attr(foo)?
        QualifiedName attr(
            g_null_atom, ToCSSCustomIdentValue(function_value->Item(0)).Value(),
            g_null_atom);
        const AtomicString& value = state.GetElement()->getAttribute(attr);
        string = value.IsNull() ? g_empty_string : value.GetString();
      } else {
        string = ToCSSStringValue(*item).Value();
      }
      if (prev_content && prev_content->IsText()) {
        TextContentData* text_content = ToTextContentData(prev_content);
        text_content->SetText(text_content->GetText() + string);
        continue;
      }
      next_content = ContentData::Create(string);
    }

    if (!first_content)
      first_content = next_content;
    else
      prev_content->SetNext(next_content);

    prev_content = next_content;
  }
  DCHECK(first_content);
  state.Style()->SetContent(first_content);
}

}  // namespace CSSLonghand
}  // namespace blink
