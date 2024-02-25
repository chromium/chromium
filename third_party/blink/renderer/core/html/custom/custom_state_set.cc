// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_state_set.h"

#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_idioms.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

class CustomStateIterationSource : public CustomStateSet::IterationSource {
 public:
  explicit CustomStateIterationSource(CustomStateSet& states)
      : states_(states) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(states_);
    CustomStateSet::IterationSource::Trace(visitor);
  }

  bool FetchNextItem(ScriptState*,
                     String& out_value,
                     ExceptionState&) override {
    if (index_ >= states_->list_.size())
      return false;
    out_value = states_->list_[index_++];
    return true;
  }

  void DidEraseAt(wtf_size_t erased_index) {
    // If index_ is N and an item between 0 and N-1 was erased, decrement
    // index_ in order that Next() will return an item which was at N.
    if (erased_index < index_)
      --index_;
  }

 private:
  Member<CustomStateSet> states_;
  wtf_size_t index_ = 0;
};

CustomStateSet::CustomStateSet(Element& element) : element_(element) {}

void CustomStateSet::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(iterators_);
  ScriptWrappable::Trace(visitor);
}

void CustomStateSet::add(const String& value, ExceptionState& exception_state) {
  if (RuntimeEnabledFeatures::CSSCustomStateNewSyntaxEnabled()) {
    if (!list_.Contains(value)) {
      list_.push_back(value);
    }
    InvalidateStyle();
    return;
  }

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
  if (!list_.Contains(value))
    list_.push_back(value);

  InvalidateStyle();
}

uint32_t CustomStateSet::size() const {
  return list_.size();
}

void CustomStateSet::clearForBinding(ScriptState*, ExceptionState&) {
  list_.clear();
  InvalidateStyle();
}

bool CustomStateSet::deleteForBinding(ScriptState*,
                                      const String& value,
                                      ExceptionState&) {
  wtf_size_t index = list_.Find(value);
  if (index == WTF::kNotFound)
    return false;
  list_.EraseAt(index);
  for (auto& iterator : iterators_)
    iterator->DidEraseAt(index);
  InvalidateStyle();
  return true;
}

bool CustomStateSet::hasForBinding(ScriptState*,
                                   const String& value,
                                   ExceptionState&) const {
  return Has(value);
}

bool CustomStateSet::Has(const String& value) const {
  return list_.Contains(value);
}

CustomStateSet::IterationSource* CustomStateSet::CreateIterationSource(
    ScriptState*,
    ExceptionState&) {
  auto* iterator = MakeGarbageCollected<CustomStateIterationSource>(*this);
  iterators_.insert(iterator);
  return iterator;
}

void CustomStateSet::InvalidateStyle() const {
  // TOOD(tkent): The following line invalidates all of rulesets with any
  // custom state pseudo classes though we should invalidate only rulesets
  // with the updated state ideally. We can improve style resolution
  // performance in documents with various custom state pseudo classes by
  // having blink::InvalidationSet for each of states.
  element_->PseudoStateChanged(CSSSelector::kPseudoState);
  element_->PseudoStateChanged(CSSSelector::kPseudoStateDeprecatedSyntax);
}

}  // namespace blink
