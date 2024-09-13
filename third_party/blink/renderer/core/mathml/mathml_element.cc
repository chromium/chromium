// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_element.h"

#include "third_party/blink/renderer/bindings/core/v8/js_event_handler_for_content_attribute.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"

namespace blink {

MathMLElement::MathMLElement(const QualifiedName& tagName,
                             Document& document,
                             ConstructionType constructionType)
    : Element(tagName, &document, constructionType) {}

MathMLElement::~MathMLElement() {}

static inline bool IsValidDirAttribute(const AtomicString& value) {
  return EqualIgnoringASCIICase(value, "ltr") ||
         EqualIgnoringASCIICase(value, "rtl");
}

// Keywords from CSS font-size are skipped.
static inline bool IsDisallowedMathSizeAttribute(const AtomicString& value) {
  return EqualIgnoringASCIICase(value, "medium") ||
         value.EndsWith("large", kTextCaseASCIIInsensitive) ||
         value.EndsWith("small", kTextCaseASCIIInsensitive) ||
         EqualIgnoringASCIICase(value, "smaller") ||
         EqualIgnoringASCIICase(value, "larger") ||
         EqualIgnoringASCIICase(value, "math");
}

bool MathMLElement::IsPresentationAttribute(const QualifiedName& name) const {
  if (name == html_names::kDirAttr || name == mathml_names::kMathsizeAttr ||
      name == mathml_names::kMathcolorAttr ||
      name == mathml_names::kMathbackgroundAttr ||
      name == mathml_names::kScriptlevelAttr ||
      name == mathml_names::kDisplaystyleAttr) {
    return true;
  }
  return Element::IsPresentationAttribute(name);
}

namespace {

bool ParseScriptLevel(const AtomicString& attributeValue,
                      unsigned& scriptLevel,
                      bool& add) {
  String value = attributeValue;
  if (value.StartsWith("+") || value.StartsWith("-")) {
    add = true;
    value = value.Right(1);
  }

  return WTF::VisitCharacters(value, [&](auto chars) {
    WTF::NumberParsingResult result;
    constexpr auto kOptions =
        WTF::NumberParsingOptions().SetAcceptMinusZeroForUnsigned();
    scriptLevel = CharactersToUInt(chars, kOptions, &result);
    return result == WTF::NumberParsingResult::kSuccess;
  });
}

}  // namespace

void MathMLElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kDirAttr) {
    if (IsValidDirAttribute(value)) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kDirection,
                                              value);
    }
  } else if (name == mathml_names::kMathsizeAttr) {
    if (!IsDisallowedMathSizeAttribute(value)) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kFontSize,
                                              value);
    }
  } else if (name == mathml_names::kMathbackgroundAttr) {
    AddPropertyToPresentationAttributeStyle(
        style, CSSPropertyID::kBackgroundColor, value);
  } else if (name == mathml_names::kMathcolorAttr) {
    AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kColor,
                                            value);
  } else if (name == mathml_names::kScriptlevelAttr) {
    unsigned scriptLevel = 0;
    bool add = false;
    if (ParseScriptLevel(value, scriptLevel, add)) {
      if (add) {
        AddPropertyToPresentationAttributeStyle(
            style, CSSPropertyID::kMathDepth, "add(" + value + ")");
      } else {
        AddPropertyToPresentationAttributeStyle(
            style, CSSPropertyID::kMathDepth, scriptLevel,
            CSSPrimitiveValue::UnitType::kNumber);
      }
    }
  } else if (name == mathml_names::kDisplaystyleAttr) {
    if (EqualIgnoringASCIICase(value, "false")) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kMathStyle,
                                              CSSValueID::kCompact);
    } else if (EqualIgnoringASCIICase(value, "true")) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kMathStyle,
                                              CSSValueID::kNormal);
    }
  } else {
    Element::CollectStyleForPresentationAttribute(name, value, style);
  }
}

void MathMLElement::ParseAttribute(const AttributeModificationParams& param) {
  const AtomicString& event_name =
      HTMLElement::EventNameForAttributeName(param.name);
  if (!event_name.IsNull()) {
    SetAttributeEventListener(
        event_name, JSEventHandlerForContentAttribute::Create(
                        GetExecutionContext(), param.name, param.new_value));
    return;
  }

  Element::ParseAttribute(param);
}

std::optional<bool> MathMLElement::BooleanAttribute(
    const QualifiedName& name) const {
  const AtomicString& value = FastGetAttribute(name);
  if (EqualIgnoringASCIICase(value, "true"))
    return true;
  if (EqualIgnoringASCIICase(value, "false"))
    return false;
  return std::nullopt;
}

const CSSPrimitiveValue* MathMLElement::ParseMathLength(
    const QualifiedName& attr_name,
    AllowPercentages allow_percentages,
    CSSPrimitiveValue::ValueRange value_range) {
  if (!FastHasAttribute(attr_name))
    return nullptr;
  auto value = FastGetAttribute(attr_name);
  const CSSPrimitiveValue* parsed_value = CSSParser::ParseLengthPercentage(
      value,
      StrictCSSParserContext(GetExecutionContext()->GetSecureContextMode()),
      value_range);
  if (!parsed_value || parsed_value->IsCalculated() ||
      (parsed_value->IsPercentage() &&
       allow_percentages == AllowPercentages::kNo)) {
    return nullptr;
  }
  return parsed_value;
}

std::optional<Length> MathMLElement::AddMathLengthToComputedStyle(
    const CSSToLengthConversionData& conversion_data,
    const QualifiedName& attr_name,
    AllowPercentages allow_percentages,
    CSSPrimitiveValue::ValueRange value_range) {
  if (const CSSPrimitiveValue* parsed_value =
          ParseMathLength(attr_name, allow_percentages, value_range)) {
    return parsed_value->ConvertToLength(conversion_data);
  }
  return std::nullopt;
}

}  // namespace blink
