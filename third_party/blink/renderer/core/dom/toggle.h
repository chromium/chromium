// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TOGGLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TOGGLE_H_

#include "third_party/blink/renderer/core/style/toggle_root.h"

namespace blink {

class Toggle : public ToggleRoot {
 public:
  Toggle(const AtomicString& name,
         States states,
         State value,
         ToggleOverflow overflow,
         bool is_group,
         ToggleScope scope)
      : ToggleRoot(name, states, value, overflow, is_group, scope) {}
  explicit Toggle(const ToggleRoot& root) : ToggleRoot(root) {}
  // make the protected default constructor on the base class public on this
  // derived class
  Toggle() = default;

  Toggle(const Toggle&) = default;
  ~Toggle() = default;

  // For Toggles, the concept is referred to as the value rather than
  // the initial state (as it is for toggle-root values, also known as
  // toggle specifiers, which we happen to use as a base class
  State InitialState() const = delete;
  State Value() const { return value_; }

  void SetValue(const State& value) { value_ = value; }
  void SetValue(State&& value) { value_ = value; }

  bool ValueMatches(const State& other) const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TOGGLE_H_
