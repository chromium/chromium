// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/view_transition/view_transition_type_set.h"

#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_supplement.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ViewTransitionTypeIterationSource
    : public ViewTransitionTypeSet::IterationSource {
 public:
  explicit ViewTransitionTypeIterationSource(ViewTransitionTypeSet& types)
      : types_(types) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(types_);
    ViewTransitionTypeSet::IterationSource::Trace(visitor);
  }

  bool FetchNextItem(ScriptState*,
                     String& out_value,
                     ExceptionState&) override {
    if (index_ >= types_->size()) {
      return false;
    }
    out_value = types_->At(index_++);
    return true;
  }

  void DidEraseAt(wtf_size_t erased_index) {
    // If index_ is N and an item between 0 and N-1 was erased, decrement
    // index_ in order that Next() will return an item which was at N.
    if (erased_index < index_) {
      --index_;
    }
  }

 private:
  Member<ViewTransitionTypeSet> types_;
  wtf_size_t index_ = 0;
};

bool ViewTransitionTypeSet::IsValidType(const String& value) {
  String lower = value.LowerASCII();
  return lower != "none" && !lower.StartsWith("-ua-");
}

ViewTransitionTypeSet::ViewTransitionTypeSet(
    ViewTransition* view_transition,
    const Vector<String>& initial_values) {
  view_transition_ = view_transition;
  for (const String& type : initial_values) {
    AddInternal(type);
  }
}

void ViewTransitionTypeSet::AddInternal(const String& type) {
  if (types_.Contains(type)) {
    return;
  }

  types_.push_back(type);
  if (IsValidType(type)) {
    InvalidateStyle();
  }
}

void ViewTransitionTypeSet::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(view_transition_);
  visitor->Trace(iterators_);
}

void ViewTransitionTypeSet::add(const String& value,
                                ExceptionState& exception_state) {
  AddInternal(value);
}

void ViewTransitionTypeSet::InvalidateStyle() {
  if (!view_transition_) {
    return;
  }

  if (!view_transition_->DomWindow()) {
    return;
  }

  Document* document = view_transition_->DomWindow()->document();
  if (ViewTransitionSupplement::From(*document)->GetTransition() !=
      view_transition_) {
    return;
  }

  if (Element* document_element = document->documentElement()) {
    document_element->PseudoStateChanged(
        CSSSelector::kPseudoActiveViewTransitionType);
  }
}

void ViewTransitionTypeSet::clearForBinding(ScriptState*, ExceptionState&) {
  if (!types_.empty()) {
    types_.clear();
    InvalidateStyle();
  }
}

bool ViewTransitionTypeSet::deleteForBinding(ScriptState*,
                                             const String& value,
                                             ExceptionState&) {
  wtf_size_t index = types_.Find(value);
  if (index == WTF::kNotFound) {
    return false;
  }
  types_.EraseAt(index);
  for (auto& iterator : iterators_) {
    iterator->DidEraseAt(index);
  }
  InvalidateStyle();
  return true;
}

ViewTransitionTypeSet::IterationSource*
ViewTransitionTypeSet::CreateIterationSource(ScriptState*, ExceptionState&) {
  auto* iterator =
      MakeGarbageCollected<ViewTransitionTypeIterationSource>(*this);
  iterators_.insert(iterator);
  return iterator;
}

}  // namespace blink
