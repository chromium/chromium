// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_menu_item_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {

HTMLMenuItemElement::HTMLMenuItemElement(Document& document)
    : HTMLElement(html_names::kMenuitemTag, document), is_checked_(false) {}

HTMLMenuItemElement::~HTMLMenuItemElement() = default;

bool HTMLMenuItemElement::MatchesDefaultPseudoClass() const {
  return FastHasAttribute(html_names::kCheckedAttr);
}

bool HTMLMenuItemElement::MatchesEnabledPseudoClass() const {
  return !IsDisabledFormControl();
}

void HTMLMenuItemElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == html_names::kDisabledAttr) {
    if (params.old_value.IsNull() != params.new_value.IsNull()) {
      PseudoStateChanged(CSSSelector::kPseudoDisabled);
      PseudoStateChanged(CSSSelector::kPseudoEnabled);
    }
  } else if (name == html_names::kCheckedAttr) {
    if (params.old_value.IsNull() != params.new_value.IsNull() && !is_dirty_) {
      setChecked(!params.new_value.IsNull());
      is_dirty_ = false;
    }
    PseudoStateChanged(CSSSelector::kPseudoDefault);
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

bool HTMLMenuItemElement::Checked() const {
  return is_checked_;
}

void HTMLMenuItemElement::setChecked(bool checked) {
  is_dirty_ = true;
  if (is_checked_ == checked) {
    return;
  }

  is_checked_ = checked;
  PseudoStateChanged(CSSSelector::kPseudoChecked);

  // TODO: Accessibility mapping.
}

void HTMLMenuItemElement::SetDirty(bool value) {
  is_dirty_ = value;
}

bool HTMLMenuItemElement::IsDisabledFormControl() const {
  return FastHasAttribute(html_names::kDisabledAttr);
}

void HTMLMenuItemElement::DefaultEventHandler(Event& event) {
  // TODO: Handle activation behavior events.
  HTMLElement::DefaultEventHandler(event);
}

}  // namespace blink
