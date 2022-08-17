// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/css_toggle.h"

namespace blink {

CSSToggle::CSSToggle(const AtomicString& name,
                     States states,
                     State value,
                     ToggleOverflow overflow,
                     bool is_group,
                     ToggleScope scope)
    : ToggleRoot(name, states, value, overflow, is_group, scope) {}

CSSToggle::CSSToggle(const ToggleRoot& root) : ToggleRoot(root) {}

CSSToggle::~CSSToggle() = default;

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
