/*
 * Copyright (c) 2012 Motorola Mobility, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY MOTOROLA MOBILITY, INC. AND ITS CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MOTOROLA MOBILITY, INC. OR ITS
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/forms/radio_node_list.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_rare_data.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"

namespace blink {

using mojom::blink::FormControlType;

RadioNodeList::RadioNodeList(ContainerNode& owner_node,
                             CollectionType type,
                             const AtomicString& name)
    : LiveNodeList(owner_node,
                   type,
                   kInvalidateForFormControls,
                   IsA<HTMLFormElement>(owner_node)
                       ? NodeListSearchRoot::kTreeScope
                       : NodeListSearchRoot::kOwnerNode),
      name_(name) {
  DCHECK(type == kRadioNodeListType || type == kRadioImgNodeListType);
}

RadioNodeList::~RadioNodeList() = default;

static inline HTMLInputElement* ToRadioButtonInputElement(Element& element) {
  auto* input_element = DynamicTo<HTMLInputElement>(&element);
  if (!input_element)
    return nullptr;
  if (input_element->FormControlType() != FormControlType::kInputRadio ||
      input_element->Value().empty()) {
    return nullptr;
  }
  return input_element;
}

String RadioNodeList::value() const {
  if (ShouldOnlyMatchImgElements())
    return String();
  unsigned length = this->length();
  for (unsigned i = 0; i < length; ++i) {
    const HTMLInputElement* input_element = ToRadioButtonInputElement(*item(i));
    if (!input_element || !input_element->Checked())
      continue;
    return input_element->Value();
  }
  return String();
}

void RadioNodeList::setValue(const String& value) {
  if (ShouldOnlyMatchImgElements())
    return;
  unsigned length = this->length();
  for (unsigned i = 0; i < length; ++i) {
    HTMLInputElement* input_element = ToRadioButtonInputElement(*item(i));
    if (!input_element || input_element->Value() != value)
      continue;
    input_element->SetChecked(true);
    return;
  }
}

bool RadioNodeList::MatchesByIdOrName(const Element& test_element) const {
  return test_element.GetIdAttribute() == name_ ||
         test_element.GetNameAttribute() == name_;
}

bool RadioNodeList::ElementMatches(const Element& element) const {
  if (ShouldOnlyMatchImgElements()) {
    auto* html_image_element = DynamicTo<HTMLImageElement>(element);
    if (!html_image_element)
      return false;

    if (html_image_element->formOwner() != ownerNode())
      return false;

    return MatchesByIdOrName(element);
  }
  auto* html_element = DynamicTo<HTMLElement>(element);
  bool is_form_associated =
      html_element && html_element->IsFormAssociatedCustomElement();
  if (!IsA<HTMLObjectElement>(element) && !element.IsFormControlElement() &&
      !is_form_associated) {
    return false;
  }

  auto* html_input_element = DynamicTo<HTMLInputElement>(&element);
  if (html_input_element &&
      html_input_element->FormControlType() == FormControlType::kInputImage) {
    return false;
  }

  if (IsA<HTMLFormElement>(ownerNode())) {
    auto* form_element = html_element->formOwner();
    if (!form_element || form_element != ownerNode())
      return false;
  }

  return MatchesByIdOrName(element);
}

}  // namespace blink
