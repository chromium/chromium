// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_state_set.h"

#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_idioms.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

CustomStateSet::CustomStateSet(Element& element) : element_(element) {}

void CustomStateSet::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  ScriptWrappable::Trace(visitor);
}

void CustomStateSet::add(const String& value, ExceptionState& exception_state) {
  // https://wicg.github.io/custom-state-pseudo-class/#dom-customstateset-add

  // 1. If value does not match to <dashed-ident>, then throw a "SyntaxError"
  // DOMException.
  if (!value.StartsWith("--")) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The specified value '" + value + "' must start with '--'.");
    return;
  }
  for (wtf_size_t i = 2; i < value.length(); ++i) {
    if (IsNameCodePoint(value[i]))
      continue;
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The specified value '" + value +
            "' must match to <dashed-ident> production. '" + value[i] +
            "' is invalid.");
    return;
  }

  // 2. Invoke the default add operation, which the setlike<DOMString> would
  // have if CustomStateSet interface had no add(value) operation, with value
  // argument.
  set_.insert(value);

  InvalidateStyle();
}

uint32_t CustomStateSet::size() const {
  return set_.size();
}

void CustomStateSet::clearForBinding(ScriptState*, ExceptionState&) {
  set_.clear();
  InvalidateStyle();
}

bool CustomStateSet::deleteForBinding(ScriptState*,
                                      const String& value,
                                      ExceptionState&) {
  auto iter = set_.find(value);
  if (iter == set_.cend())
    return false;
  set_.erase(iter);
  InvalidateStyle();
  return true;
}

bool CustomStateSet::hasForBinding(ScriptState*,
                                   const String& value,
                                   ExceptionState&) const {
  return Has(value);
}

bool CustomStateSet::Has(const String& value) const {
  return set_.Contains(value);
}

class CustomStateIterationSource : public CustomStateSet::IterationSource {
 public:
  explicit CustomStateIterationSource(CustomStateSet& states)
      : states_(states), iterator_(states.set_.begin()) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(states_);
    CustomStateSet::IterationSource::Trace(visitor);
  }

  bool Next(ScriptState*,
            String& out_key,
            String& out_value,
            ExceptionState&) override {
    if (iterator_ == states_->set_.end())
      return false;
    String value = *iterator_;
    ++iterator_;
    out_key = value;
    out_value = value;
    return true;
  }

 private:
  Member<CustomStateSet> states_;
  LinkedHashSet<String>::const_iterator iterator_;
};

CustomStateSet::IterationSource* CustomStateSet::StartIteration(
    ScriptState*,
    ExceptionState&) {
  return MakeGarbageCollected<CustomStateIterationSource>(*this);
}

void CustomStateSet::InvalidateStyle() const {
  // TOOD(tkent): The following line invalidates all of rulesets with any
  // custom state pseudo classes though we should invalidate only rulesets
  // with the updated state ideally. We can improve style resolution
  // performance in documents with various custom state pseudo classes by
  // having blink::InvalidationSet for each of states.
  element_->PseudoStateChanged(CSSSelector::kPseudoState);
}

}  // namespace blink
