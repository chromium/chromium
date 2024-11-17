// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/shadow/shadow_element_utils.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"

namespace blink {

bool IsSliderContainer(const Element& element) {
  if (!element.IsInUserAgentShadowRoot())
    return false;
  const AtomicString& shadow_pseudo = element.ShadowPseudoId();
  return shadow_pseudo == shadow_element_names::kPseudoMediaSliderContainer ||
         shadow_pseudo == shadow_element_names::kPseudoSliderContainer;
}

bool IsSliderThumb(const Node* node) {
  const auto* element = DynamicTo<Element>(node);
  if (!element || !element->IsInUserAgentShadowRoot())
    return false;
  const AtomicString& shadow_pseudo = element->ShadowPseudoId();
  return shadow_pseudo == shadow_element_names::kPseudoMediaSliderThumb ||
         shadow_pseudo == shadow_element_names::kPseudoSliderThumb;
}

bool IsTextControlContainer(const Node* node) {
  const auto* element = DynamicTo<Element>(node);
  if (!element || !element->IsInUserAgentShadowRoot())
    return false;
  if (!IsTextControl(element->OwnerShadowHost()))
    return false;
  return element->GetIdAttribute() ==
         shadow_element_names::kIdTextFieldContainer;
}

bool IsTextControlPlaceholder(const Node* node) {
  const auto* element = DynamicTo<Element>(node);
  if (!element || !element->IsInUserAgentShadowRoot())
    return false;
  if (!IsTextControl(element->OwnerShadowHost()))
    return false;
  return element->GetIdAttribute() == shadow_element_names::kIdPlaceholder;
}

namespace shadow_element_utils {

const AtomicString& StringForUAShadowPseudoId(PseudoId pseudo_id) {
  switch (pseudo_id) {
    case kPseudoIdPlaceholder:
      return shadow_element_names::kPseudoInputPlaceholder;
    case kPseudoIdFileSelectorButton:
      return shadow_element_names::kPseudoFileUploadButton;
    case kPseudoIdDetailsContent:
      return shadow_element_names::kIdDetailsContent;
    case kPseudoIdPickerSelect:
      return shadow_element_names::kPickerSelect;
    default:
      return g_null_atom;
  }
}

}  // namespace shadow_element_utils

}  // namespace blink
