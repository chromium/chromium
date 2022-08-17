// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_H_

#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/style/toggle_root.h"

namespace blink {

// TODO(https://crbug.com/1250716) inherit from EventTargetWithInlineData
class CORE_EXPORT CSSToggle : public ScriptWrappable, public ToggleRoot {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSToggle(const AtomicString& name,
            States states,
            State value,
            ToggleOverflow overflow,
            bool is_group,
            ToggleScope scope);
  explicit CSSToggle(const ToggleRoot& root);
  CSSToggle(const CSSToggle&) = delete;
  ~CSSToggle() override;

  // For Toggles, the concept is referred to as the value rather than
  // the initial state (as it is for toggle-root values, also known as
  // toggle specifiers, which we happen to use as a base class).
  State InitialState() const = delete;
  State Value() const { return value_; }

  void SetValue(const State& value) { value_ = value; }
  void SetValue(State&& value) { value_ = value; }

  bool ValueMatches(const State& other) const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_H_
