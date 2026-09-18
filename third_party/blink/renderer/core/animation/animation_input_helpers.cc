// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_input_helpers.h"

#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/media_values.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/resolver/css_to_style_map.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

static String CSSPropertyToKeyframeAttribute(const CSSProperty& property) {
  DCHECK_NE(property.PropertyID(), CSSPropertyID::kInvalid);
  DCHECK_NE(property.PropertyID(), CSSPropertyID::kVariable);

  switch (property.PropertyID()) {
    case CSSPropertyID::kFloat:
      return "cssFloat";
    case CSSPropertyID::kOffset:
      return "cssOffset";
    default:
      return property.GetJSPropertyName();
  }
}

// Converts a camelCase JS keyframe property name to a hyphenated CSS property
// name, e.g. "backgroundColor" -> "background-color", appending the result to
// `builder`. Leaves `builder` empty if `characters` contains a hyphen
// (disallowed for keyframe attributes).
template <typename CharType>
static void ConvertKeyframePropertyNameToCSSPropertyName(
    base::span<const CharType> characters,
    StringBuilder& builder) {
  for (size_t i = 0; i < characters.size(); ++i) {
    if (characters[i] == '-') {
      builder.Clear();
      return;
    }
    if (IsAsciiUpper(characters[i])) {
      builder.Append('-');
    }
    builder.Append(characters[i]);
  }
}

CSSPropertyID AnimationInputHelpers::KeyframeAttributeToCSSProperty(
    const String& property,
    const Document& document) {
  if (CSSVariableParser::IsValidVariableName(property))
    return CSSPropertyID::kVariable;

  // Disallow prefixed properties.
  if (property[0] == '-')
    return CSSPropertyID::kInvalid;
  if (IsAsciiUpper(property[0])) {
    return CSSPropertyID::kInvalid;
  }
  if (property == "cssFloat")
    return CSSPropertyID::kFloat;
  if (property == "cssOffset")
    return CSSPropertyID::kOffset;
  if (property.empty()) {
    return CSSPropertyID::kInvalid;
  }

  StringBuilder builder;
  VisitCharacters(property, [&builder](auto chars) {
    ConvertKeyframePropertyNameToCSSPropertyName(chars, builder);
  });
  if (builder.empty()) {
    return CSSPropertyID::kInvalid;
  }
  return CssPropertyID(document.GetExecutionContext(), StringView(builder));
}

scoped_refptr<TimingFunction> AnimationInputHelpers::ParseTimingFunction(
    const String& string,
    Document* document,
    ExceptionState& exception_state) {
  if (string.empty()) {
    exception_state.ThrowTypeError("Easing may not be the empty string");
    return nullptr;
  }

  // Fallback to an insecure parsing mode if we weren't provided with a
  // document.
  SecureContextMode secure_context_mode =
      document && document->GetExecutionContext()
          ? document->GetExecutionContext()->GetSecureContextMode()
          : SecureContextMode::kInsecureContext;
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kTransitionTimingFunction, string,
      StrictCSSParserContext(secure_context_mode));
  const auto* value_list = DynamicTo<CSSValueList>(value);
  if (!value_list) {
    DCHECK(!value || value->IsCSSWideKeyword());
    exception_state.ThrowTypeError(
        StrCat({"'", string, "' is not a valid value for easing"}));
    return nullptr;
  }
  if (value_list->length() > 1) {
    exception_state.ThrowTypeError("Easing may not be set to a list of values");
    return nullptr;
  }
  // TODO(sesse): Are there situations where we're being called, but where
  // we should use a length resolver related to a specific element's computed
  // style?
  MediaValues* media_values = MediaValues::CreateDynamicIfFrameExists(
      document ? document->GetFrame() : nullptr);
  return CSSToStyleMap::MapAnimationTimingFunction(*media_values,
                                                   value_list->Item(0));
}

String AnimationInputHelpers::PropertyHandleToKeyframeAttribute(
    PropertyHandle property) {
  if (property.IsCSSCustomProperty()) {
    return property.CustomPropertyName();
  }
  return CSSPropertyToKeyframeAttribute(property.GetCSSProperty());
}

}  // namespace blink
