// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mathml/mathml_token_element.h"

#include "third_party/blink/renderer/core/dom/character_data.h"
#include "third_party/blink/renderer/core/layout/mathml/layout_mathml_block_flow.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

MathMLTokenElement::MathMLTokenElement(const QualifiedName& tagName,
                                       Document& document)
    : MathMLElement(tagName, document) {
  token_content_ = std::nullopt;
}

namespace {

UChar32 TokenCodePoint(const String& text_content) {
  DCHECK(!text_content.Is8Bit());
  auto content_length = text_content.length();
  // Reject malformed UTF-16 and operator strings consisting of more than one
  // codepoint.
  if ((content_length > 2) || (content_length == 0) ||
      (content_length == 1 && !U16_IS_SINGLE(text_content[0])) ||
      (content_length == 2 && !U16_IS_LEAD(text_content[0])))
    return kNonCharacter;

  UChar32 character;
  unsigned offset = 0;
  U16_NEXT(text_content, offset, content_length, character);
  return character;
}

}  // namespace

bool MathMLTokenElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (Node::HasTagName(mathml_names::kMiTag) &&
      name == mathml_names::kMathvariantAttr) {
    return true;
  }
  return MathMLElement::IsPresentationAttribute(name);
}

void MathMLTokenElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == mathml_names::kMathvariantAttr &&
      EqualIgnoringASCIICase(value, "normal")) {
    AddPropertyToPresentationAttributeStyle(
        style, CSSPropertyID::kTextTransform, CSSValueID::kNone);
  } else {
    MathMLElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

MathMLTokenElement::TokenContent MathMLTokenElement::ParseTokenContent() {
  MathMLTokenElement::TokenContent token_content;
  // Build the text content of the token element. If it contains something other
  // than character data, exit early since no special handling is required.
  StringBuilder text_content;
  for (auto* child = firstChild(); child; child = child->nextSibling()) {
    auto* character_data = DynamicTo<CharacterData>(child);
    if (!character_data)
      return token_content;
    if (child->getNodeType() == kTextNode)
      text_content.Append(character_data->data());
  }
  // Parse the token content.
  token_content.characters = text_content.ToString();
  token_content.characters.Ensure16Bit();
  token_content.code_point = TokenCodePoint(token_content.characters);
  return token_content;
}

const MathMLTokenElement::TokenContent& MathMLTokenElement::GetTokenContent() {
  if (!token_content_)
    token_content_ = ParseTokenContent();
  return token_content_.value();
}

void MathMLTokenElement::ChildrenChanged(
    const ChildrenChange& children_change) {
  token_content_ = std::nullopt;
  MathMLElement::ChildrenChanged(children_change);
}

LayoutObject* MathMLTokenElement::CreateLayoutObject(
    const ComputedStyle& style) {
  if (!style.IsDisplayMathType()) {
    return MathMLElement::CreateLayoutObject(style);
  }
  return MakeGarbageCollected<LayoutMathMLBlockFlow>(this);
}

}  // namespace blink
