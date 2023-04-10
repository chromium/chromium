// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/style/toggle_root.h"

namespace blink {

class CSSToggleData;
class CSSToggleMap;
class Element;
class ExceptionState;
class ToggleTrigger;
class V8CSSToggleCycle;
class V8CSSToggleScope;
class V8UnionStringOrUnsignedLong;
class V8UnionStringArrayOrUnsignedLong;

class CORE_EXPORT CSSToggle : public ScriptWrappable, public ToggleRoot {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSToggle(const ToggleRoot& root, CSSToggleMap& owner_toggle_map);
  CSSToggle(const AtomicString& name,
            States states,
            State initial_state,
            ToggleOverflow overflow,
            bool is_group,
            ToggleScope scope);
  CSSToggle(const CSSToggle&) = delete;
  ~CSSToggle() override;

  void Trace(Visitor*) const override;

  CSSToggleMap* OwnerToggleMap() const { return owner_toggle_map_; }
  Element* OwnerElement() const;

  // Find the toggle and corresponding element that has the toggle named name
  // that is in scope on this element, or both null if no toggle is in scope.
  // The element may be this.
  //
  // See https://tabatkins.github.io/css-toggle/#toggle-in-scope .
  static CSSToggle* FindToggleInScope(Element& start_element,
                                      const AtomicString& name);

  // Implement https://tabatkins.github.io/css-toggle/#fire-a-toggle-activation
  static bool FireToggleActivation(Element& activated_element,
                                   const ToggleTrigger& activation);

  void ChangeToggle(const ToggleTrigger& action,
                    const ToggleRoot* override_spec);

  // CSSToggle API
  V8UnionStringOrUnsignedLong* value();
  void setValue(const V8UnionStringOrUnsignedLong* value);
  absl::optional<unsigned> valueAsNumber();
  void setValueAsNumber(absl::optional<unsigned> value,
                        ExceptionState& exception_state);
  String valueAsString();
  void setValueAsString(const String& value, ExceptionState& exception_state);
  V8UnionStringArrayOrUnsignedLong* states();
  void setStates(const V8UnionStringArrayOrUnsignedLong* value,
                 ExceptionState& exception_state);
  bool group();
  void setGroup(bool group);
  V8CSSToggleScope scope();
  void setScope(V8CSSToggleScope scope);
  V8CSSToggleCycle cycle();
  void setCycle(V8CSSToggleCycle cycle);
  static CSSToggle* Create(ExceptionState& exception_state);
  static CSSToggle* Create(CSSToggleData* options,
                           ExceptionState& exception_state);

  // For Toggles, the concept is referred to as the value rather than
  // the initial state (as it is for toggle-root values, also known as
  // toggle specifiers, which we happen to use as a base class).
  const State& InitialState() const = delete;
  const State& Value() const { return value_; }

  void ChangeOwner(CSSToggleMap& owner_toggle_map, const AtomicString& name) {
    owner_toggle_map_ = &owner_toggle_map;
    name_ = name;
  }

  void SetValue(const State& value);
  void MakeRestOfToggleGroupZero();
  const ToggleRoot* FindToggleSpecifier() const;
  void FireToggleChangeEvent();

  enum class PostRecalcAt : uint8_t {
    kNow = 0,
    kLater = 1,
  };
  void SetNeedsStyleRecalc(Element* toggle_element, PostRecalcAt when);

  bool ValueMatches(const State& other,
                    const States* states_override = nullptr) const;

  // Is the value a non-zero (active) value?
  bool ValueIsActive(const States* states_override = nullptr) const {
    return !ValueMatches(State(0u), states_override);
  }

 private:
  void setStatesInternal(const States& states, ExceptionState& exception_state);
  void SetValueAndCheckGroup(const State& value);
  void SetLaterSiblingsNeedStyleRecalc(Element* toggle_element,
                                       PostRecalcAt when);

  Member<CSSToggleMap> owner_toggle_map_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_H_
