// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/css_toggle.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"

namespace blink {

CSSToggle::CSSToggle(const ToggleRoot& root) : ToggleRoot(root) {}

CSSToggle::~CSSToggle() = default;

void CSSToggle::SetValue(const State& value, Element* toggle_element) {
  bool need_recalc_style = !ValueMatches(value);

  value_ = value;

  if (need_recalc_style)
    SetNeedsStyleRecalc(toggle_element, PostRecalcAt::NOW);
}

namespace {

void SetElementNeedsStyleRecalc(Element* element,
                                CSSToggle::PostRecalcAt when,
                                const StyleChangeReasonForTracing& reason) {
  if (when == CSSToggle::PostRecalcAt::NOW)
    element->SetNeedsStyleRecalc(StyleChangeType::kSubtreeStyleChange, reason);
  else
    element->GetDocument().AddToRecalcStyleForToggle(element);
}

}  // namespace

void CSSToggle::SetNeedsStyleRecalc(Element* toggle_element,
                                    PostRecalcAt when) {
  const auto& reason = StyleChangeReasonForTracing::CreateWithExtraData(
      style_change_reason::kPseudoClass, style_change_extra_data::g_toggle);
  SetElementNeedsStyleRecalc(toggle_element, when, reason);
  if (scope_ == ToggleScope::kWide) {
    Element* e = toggle_element;
    while (true) {
      e = ElementTraversal::NextSibling(*e);
      if (!e)
        break;
      SetElementNeedsStyleRecalc(e, when, reason);
    }
  }
}

// https://tabatkins.github.io/css-toggle/#toggle-match-value
bool CSSToggle::ValueMatches(const State& other) const {
  if (value_ == other)
    return true;

  if (value_.IsInteger() == other.IsInteger() || !states_.IsNames())
    return false;

  State::IntegerType integer;
  const AtomicString* ident;
  if (value_.IsInteger()) {
    integer = value_.AsInteger();
    ident = &other.AsName();
  } else {
    integer = other.AsInteger();
    ident = &value_.AsName();
  }

  auto ident_index = states_.AsNames().Find(*ident);
  return ident_index != kNotFound && integer == ident_index;
}

}  // namespace blink
