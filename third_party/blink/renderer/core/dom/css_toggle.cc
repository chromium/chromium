// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/css_toggle.h"

#include <limits>

#include "third_party/blink/renderer/bindings/core/v8/v8_css_toggle_cycle.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_css_toggle_data.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_css_toggle_scope.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_unsignedlong.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_stringarray_unsignedlong.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_stringsequence_unsignedlong.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/css_toggle_event.h"
#include "third_party/blink/renderer/core/dom/css_toggle_map.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/toggle_group_list.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

CSSToggle::CSSToggle(const ToggleRoot& root, CSSToggleMap& owner_toggle_map)
    : ToggleRoot(root), owner_toggle_map_(owner_toggle_map) {}

CSSToggle::CSSToggle(const AtomicString& name,
                     States states,
                     State initial_state,
                     ToggleOverflow overflow,
                     bool is_group,
                     ToggleScope scope)
    : ToggleRoot(name, states, initial_state, overflow, is_group, scope),
      owner_toggle_map_(nullptr) {}

CSSToggle::~CSSToggle() = default;

void CSSToggle::Trace(Visitor* visitor) const {
  visitor->Trace(owner_toggle_map_);

  ScriptWrappable::Trace(visitor);
}

Element* CSSToggle::OwnerElement() const {
  if (!owner_toggle_map_)
    return nullptr;
  return owner_toggle_map_->OwnerElement();
}

V8UnionStringOrUnsignedLong* CSSToggle::value() {
  if (value_.IsInteger()) {
    return MakeGarbageCollected<V8UnionStringOrUnsignedLong>(
        value_.AsInteger());
  } else {
    return MakeGarbageCollected<V8UnionStringOrUnsignedLong>(value_.AsName());
  }
}

void CSSToggle::setValue(const V8UnionStringOrUnsignedLong* value) {
  State new_value(0);
  if (value->IsUnsignedLong()) {
    new_value = State(value->GetAsUnsignedLong());
  } else {
    new_value = State(AtomicString(value->GetAsString()));
  }

  SetValueAndCheckGroup(new_value);
}

absl::optional<unsigned> CSSToggle::valueAsNumber() {
  if (value_.IsInteger())
    return value_.AsInteger();

  if (states_.IsNames()) {
    auto ident_index = states_.AsNames().Find(value_.AsName());
    if (ident_index != kNotFound)
      return ident_index;
  }
  return absl::nullopt;
}

void CSSToggle::setValueAsNumber(absl::optional<unsigned> value,
                                 ExceptionState& exception_state) {
  if (!value) {
    exception_state.ThrowTypeError("The provided value is null.");
    return;
  }

  SetValueAndCheckGroup(State(*value));
}

String CSSToggle::valueAsString() {
  if (value_.IsName())
    return value_.AsName();

  if (states_.IsNames()) {
    const auto& state_names = states_.AsNames();
    auto v = value_.AsInteger();
    static_assert(!std::numeric_limits<decltype(v)>::is_signed);
    if (v < state_names.size())
      return state_names[v];
  }

  return g_null_atom;
}

void CSSToggle::setValueAsString(const String& value,
                                 ExceptionState& exception_state) {
  if (value.IsNull()) {
    exception_state.ThrowTypeError("The provided value is null.");
    return;
  }

  SetValueAndCheckGroup(State(AtomicString(value)));
}

V8UnionStringArrayOrUnsignedLong* CSSToggle::states() {
  if (states_.IsInteger()) {
    return MakeGarbageCollected<V8UnionStringArrayOrUnsignedLong>(
        states_.AsInteger());
  } else {
    Vector<String> string_array;
    for (const AtomicString& state : states_.AsNames())
      string_array.push_back(state.GetString());
    return MakeGarbageCollected<V8UnionStringArrayOrUnsignedLong>(string_array);
  }
}

void CSSToggle::setStates(const V8UnionStringArrayOrUnsignedLong* value,
                          ExceptionState& exception_state) {
  States new_states(1);
  if (value->IsUnsignedLong()) {
    new_states = States(value->GetAsUnsignedLong());
  } else {
    Vector<AtomicString> new_array;
    for (const String& state : value->GetAsStringArray()) {
      new_array.push_back(AtomicString(state));
    }
    new_states = States(new_array);
  }

  setStatesInternal(new_states, exception_state);
}

void CSSToggle::setStatesInternal(const States& states,
                                  ExceptionState& exception_state) {
  if (states.IsNames()) {
    const auto& states_vec = states.AsNames();

    if (states_vec.size() < 2u) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "The value provided contains fewer than 2 states.");
      return;
    }

    HashSet<AtomicString> states_present;
    for (const AtomicString& state : states_vec) {
      auto add_result = states_present.insert(state);
      if (!add_result.is_new_entry) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kSyntaxError,
            "The value provided contains \"" + state + "\" more than once.");
        return;
      }
    }
  }

  Element* toggle_element = OwnerElement();
  bool changed = toggle_element && states != states_;
  states_ = std::move(states);

  if (changed)
    SetNeedsStyleRecalc(toggle_element, PostRecalcAt::kNow);
}

bool CSSToggle::group() {
  return is_group_;
}

void CSSToggle::setGroup(bool group) {
  is_group_ = group;
  // No updates are needed; the group only makes a difference when changing
  // toggles.
}

V8CSSToggleScope CSSToggle::scope() {
  V8CSSToggleScope::Enum e;
  switch (scope_) {
    case ToggleScope::kWide:
      e = V8CSSToggleScope::Enum::kWide;
      break;
    case ToggleScope::kNarrow:
      e = V8CSSToggleScope::Enum::kNarrow;
      break;
  }
  return V8CSSToggleScope(e);
}

void CSSToggle::setScope(V8CSSToggleScope scope) {
  ToggleScope new_scope;
  switch (scope.AsEnum()) {
    case V8CSSToggleScope::Enum::kWide:
      new_scope = ToggleScope::kWide;
      break;
    case V8CSSToggleScope::Enum::kNarrow:
      new_scope = ToggleScope::kNarrow;
      break;
  }
  if (scope_ == new_scope)
    return;

  scope_ = new_scope;
  if (Element* toggle_element = OwnerElement())
    SetLaterSiblingsNeedStyleRecalc(toggle_element, PostRecalcAt::kNow);
}

V8CSSToggleCycle CSSToggle::cycle() {
  V8CSSToggleCycle::Enum e;
  switch (overflow_) {
    case ToggleOverflow::kCycle:
      e = V8CSSToggleCycle::Enum::kCycle;
      break;
    case ToggleOverflow::kCycleOn:
      e = V8CSSToggleCycle::Enum::kCycleOn;
      break;
    case ToggleOverflow::kSticky:
      e = V8CSSToggleCycle::Enum::kSticky;
      break;
  }
  return V8CSSToggleCycle(e);
}

void CSSToggle::setCycle(V8CSSToggleCycle cycle) {
  ToggleOverflow new_overflow;
  switch (cycle.AsEnum()) {
    case V8CSSToggleCycle::Enum::kCycle:
      new_overflow = ToggleOverflow::kCycle;
      break;
    case V8CSSToggleCycle::Enum::kCycleOn:
      new_overflow = ToggleOverflow::kCycleOn;
      break;
    case V8CSSToggleCycle::Enum::kSticky:
      new_overflow = ToggleOverflow::kSticky;
      break;
  }

  overflow_ = new_overflow;
  // No updates are needed; the overflow only makes a difference when changing
  // toggles.
}

CSSToggle* CSSToggle::Create(ExceptionState& exception_state) {
  return MakeGarbageCollected<CSSToggle>(g_empty_atom, States(1), State(0),
                                         ToggleOverflow::kCycle, false,
                                         ToggleScope::kWide);
}

CSSToggle* CSSToggle::Create(CSSToggleData* options,
                             ExceptionState& exception_state) {
  DCHECK(!exception_state.HadException());
  CSSToggle* result = CSSToggle::Create(exception_state);
  DCHECK(!exception_state.HadException());
  result->setValue(options->value());
  States new_states(1);
  const auto* states_value = options->states();
  if (states_value->IsUnsignedLong()) {
    new_states = States(states_value->GetAsUnsignedLong());
  } else {
    Vector<AtomicString> new_array;
    for (const String& state : states_value->GetAsStringSequence()) {
      new_array.push_back(AtomicString(state));
    }
    new_states = States(new_array);
  }
  result->setStatesInternal(new_states, exception_state);
  if (exception_state.HadException())
    return nullptr;
  result->setGroup(options->group());
  result->setScope(options->scope());
  result->setCycle(options->cycle());
  return result;
}

void CSSToggle::SetValueAndCheckGroup(const State& value) {
  // The specification says that we should go through the whole ChangeToggle
  // algorithm (with a "set" value), but this implements a more direct way of
  // doing the same thing.
  SetValue(value);

  if (is_group_ && OwnerElement()) {
    const ToggleRoot* specifier = FindToggleSpecifier();
    const States* states = nullptr;
    if (specifier)
      states = &specifier->StateSet();

    if (ValueIsActive(states)) {
      MakeRestOfToggleGroupZero();
    }
  }
}

void CSSToggle::SetValue(const State& value) {
  Element* toggle_element = OwnerElement();
  bool need_recalc_style = toggle_element && !ValueMatches(value);

  value_ = value;

  if (need_recalc_style)
    SetNeedsStyleRecalc(toggle_element, PostRecalcAt::kNow);
}

namespace {

void SetElementNeedsStyleRecalc(Element* element,
                                CSSToggle::PostRecalcAt when,
                                const StyleChangeReasonForTracing& reason) {
  if (when == CSSToggle::PostRecalcAt::kNow)
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

void CSSToggle::SetLaterSiblingsNeedStyleRecalc(Element* toggle_element,
                                                PostRecalcAt when) {
  const auto& reason = StyleChangeReasonForTracing::CreateWithExtraData(
      style_change_reason::kPseudoClass, style_change_extra_data::g_toggle);
  Element* e = toggle_element;
  while (true) {
    e = ElementTraversal::NextSibling(*e);
    if (!e)
      break;
    SetElementNeedsStyleRecalc(e, when, reason);
  }
}

const ToggleRoot* CSSToggle::FindToggleSpecifier() const {
  Element* owner_element = OwnerElement();
  if (!owner_element)
    return nullptr;

  const ToggleRoot* toggle_specifier = nullptr;
  if (const ComputedStyle* style = owner_element->GetComputedStyle()) {
    if (const ToggleRootList* toggle_root = style->ToggleRoot()) {
      for (const auto& item : toggle_root->Roots()) {
        if (item.Name() == name_) {
          toggle_specifier = &item;
        }
      }
    }
  }
  return toggle_specifier;
}

// https://tabatkins.github.io/css-toggle/#toggle-match-value
bool CSSToggle::ValueMatches(const State& other,
                             const States* states_override) const {
  if (value_ == other)
    return true;

  const States& states = states_override ? *states_override : states_;

  if (value_.IsInteger() == other.IsInteger() || !states.IsNames())
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

  auto ident_index = states.AsNames().Find(*ident);
  return ident_index != kNotFound && integer == ident_index;
}

CSSToggle* CSSToggle::FindToggleInScope(Element& start_element,
                                        const AtomicString& name) {
  Element* element = &start_element;
  bool allow_narrow_scope = true;
  while (true) {
    if (CSSToggleMap* toggle_map = element->GetToggleMap()) {
      ToggleMap& toggles = toggle_map->Toggles();
      auto iter = toggles.find(name);
      if (iter != toggles.end()) {
        CSSToggle* toggle = iter->value;
        // TODO(https://github.com/tabatkins/css-toggle/issues/20): Should we
        // allow the current toggle specifier (if any) on the element to
        // override the stored one, like it does for other aspects?
        if (allow_narrow_scope || toggle->Scope() == ToggleScope::kWide) {
          return toggle;
        }
      }
    }

    if (Element* sibling = ElementTraversal::PreviousSibling(*element)) {
      allow_narrow_scope = false;
      element = sibling;
      continue;
    }

    allow_narrow_scope = true;
    element = element->parentElement();

    if (!element)
      return nullptr;
  }
}

void CSSToggle::FireToggleActivation(Element& activated_element,
                                     const ToggleTrigger& activation) {
  const AtomicString& name = activation.Name();
  CSSToggle* toggle = FindToggleInScope(activated_element, name);
  if (!toggle)
    return;

  CSSToggle::State old_value = toggle->Value();
  toggle->ChangeToggle(activation, toggle->FindToggleSpecifier());
  CSSToggle::State new_value = toggle->Value();

  if (old_value != new_value)
    toggle->FireToggleChangeEvent();
}

// Implement https://tabatkins.github.io/css-toggle/#change-a-toggle
void CSSToggle::ChangeToggle(const ToggleTrigger& action,
                             const ToggleRoot* override_spec) {
  using State = ToggleRoot::State;

  if (!override_spec)
    override_spec = this;
  DCHECK_EQ(Name(), override_spec->Name());
  const auto states = override_spec->StateSet();
  const bool is_group = override_spec->IsGroup();
  const auto overflow = override_spec->Overflow();

  if (action.Mode() == ToggleTriggerMode::kSet) {
    SetValue(action.Value());
  } else {
    using IntegerType = ToggleRoot::State::IntegerType;
    DCHECK_EQ(std::numeric_limits<IntegerType>::lowest(), 0u);
    const IntegerType infinity = std::numeric_limits<IntegerType>::max();
    bool overflowed = false;

    IntegerType index;
    if (Value().IsInteger()) {
      index = Value().AsInteger();
    } else if (states.IsNames()) {
      index = states.AsNames().Find(Value().AsName());
      if (index == kNotFound) {
        index = infinity;
        overflowed = true;
      }
    } else {
      index = infinity;
      overflowed = true;
    }

    IntegerType max_index;
    if (states.IsInteger()) {
      max_index = states.AsInteger();
    } else {
      max_index = states.AsNames().size() - 1;
    }

    if (action.Mode() == ToggleTriggerMode::kNext) {
      if (!overflowed) {
        IntegerType new_index = index + action.Value().AsInteger();
        if (new_index < index || new_index > max_index)
          overflowed = true;
        else
          index = new_index;
      }

      if (overflowed) {
        switch (overflow) {
          case ToggleOverflow::kCycle:
            index = 0u;
            break;
          case ToggleOverflow::kCycleOn:
            index = 1u;
            break;
          case ToggleOverflow::kSticky:
            index = max_index;
            break;
        }
      }
    } else {
      DCHECK_EQ(action.Mode(), ToggleTriggerMode::kPrev);
      bool overflowed_negative = false;
      if (!overflowed) {
        IntegerType new_index = index - action.Value().AsInteger();
        if (new_index > index) {
          overflowed = true;
          overflowed_negative = true;
        }
        index = new_index;
      }
      switch (overflow) {
        case ToggleOverflow::kCycle:
          DCHECK_EQ(std::numeric_limits<IntegerType>::lowest(), 0u);
          if (overflowed || index > max_index)
            index = max_index;
          break;
        case ToggleOverflow::kCycleOn:
          if (overflowed || index < 1u || index > max_index)
            index = max_index;
          break;
        case ToggleOverflow::kSticky:
          if (overflowed_negative)
            index = 0u;
          else if (overflowed || index > max_index)
            index = max_index;
          break;
      }
    }

    if (states.IsNames()) {
      const auto& names = states.AsNames();
      if (index < names.size()) {
        SetValue(State(names[index]));
      } else {
        SetValue(State(index));
      }
    } else {
      SetValue(State(index));
    }
  }

  // If t’s value does not match 0, and group is true, then set the value of
  // all other toggles in the same toggle group as t to 0.
  if (is_group && ValueIsActive(&states))
    MakeRestOfToggleGroupZero();
}

namespace {

std::pair<Element*, ToggleScope> FindToggleGroupElement(
    Element* toggle_element,
    const AtomicString& name) {
  Element* element = toggle_element;
  bool allow_narrow_scope = true;
  while (true) {
    Element* parent = element->parentElement();
    if (!parent) {
      // An element is in the root's group if we don't find any other group.
      //
      // TODO(https://github.com/tabatkins/css-toggle/issues/23): See if the
      // spec ends up describing it this way.
      return std::make_pair(element, ToggleScope::kNarrow);
    }

    if (const ComputedStyle* style = element->GetComputedStyle()) {
      if (const ToggleGroupList* toggle_groups = style->ToggleGroup()) {
        for (const auto& group : toggle_groups->Groups()) {
          if (group.Name() == name &&
              (allow_narrow_scope || group.Scope() == ToggleScope::kWide)) {
            return std::make_pair(element, group.Scope());
          }
        }
      }
    }

    if (Element* sibling = ElementTraversal::PreviousSibling(*element)) {
      allow_narrow_scope = false;
      element = sibling;
      continue;
    }

    allow_narrow_scope = true;

    element = parent;
  }
}

}  // namespace

void CSSToggle::MakeRestOfToggleGroupZero() {
  // We do not attempt to maintain any persistent state representing toggle
  // groups, since doing so without noticeable overhead would require a decent
  // amount of code.  Instead, we will simply find the elements in the toggle
  // group here.  If this turns out to be too slow, we could try to maintain
  // data structures to represent groups, but doing so requires monitoring
  // style changes on *elements*.

  using State = ToggleRoot::State;

  Element* toggle_element = OwnerElement();
  const AtomicString& name = Name();
  auto [toggle_group_element, toggle_scope] =
      FindToggleGroupElement(toggle_element, name);
  Element* stay_within;
  switch (toggle_scope) {
    case ToggleScope::kNarrow:
      stay_within = toggle_group_element;
      break;
    case ToggleScope::kWide:
      stay_within = toggle_group_element->parentElement();
      break;
  }

  Element* e = toggle_group_element;
  do {
    if (e == toggle_element) {
      e = ElementTraversal::Next(*e, stay_within);
      continue;
    }
    if (e != toggle_group_element) {
      // Skip descendants in a different group.
      //
      // TODO(dbaron): What if style is null?  See
      // https://github.com/tabatkins/css-toggle/issues/24 .
      if (const ComputedStyle* style = e->GetComputedStyle()) {
        if (const ToggleGroupList* toggle_groups = style->ToggleGroup()) {
          bool found_group = false;  // to continue the outer loop
          for (const ToggleGroup& group : toggle_groups->Groups()) {
            if (group.Name() == name) {
              // TODO(https://github.com/tabatkins/css-toggle/issues/25):
              // Consider multiple occurrences of the same name.
              switch (group.Scope()) {
                case ToggleScope::kWide:
                  if (e != stay_within && e->parentElement()) {
                    e = ElementTraversal::NextSkippingChildren(
                        *e->parentElement(), stay_within);
                  } else {
                    e = nullptr;
                  }
                  break;
                case ToggleScope::kNarrow:
                  e = ElementTraversal::NextSkippingChildren(*e, stay_within);
                  break;
              }
              found_group = true;
              break;
            }
          }
          if (found_group)
            continue;
        }
      }
    }
    if (CSSToggleMap* toggle_map = e->GetToggleMap()) {
      ToggleMap& toggles = toggle_map->Toggles();
      auto iter = toggles.find(name);
      if (iter != toggles.end()) {
        CSSToggle* toggle = iter->value;
        if (toggle->IsGroup()) {
          toggle->SetValue(State(0u));
        }
      }
    }
    e = ElementTraversal::Next(*e, stay_within);
  } while (e);
}

void CSSToggle::FireToggleChangeEvent() {
  DCHECK(OwnerElement());
  OwnerElement()->DispatchEvent(
      *CSSToggleEvent::Create(event_type_names::kTogglechange, Name(), this));
}

}  // namespace blink
