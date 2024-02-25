// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/counters_scope.h"

#include "third_party/blink/renderer/core/css/style_containment_scope.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/layout/counter_node.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"

namespace blink {

// static
wtf_size_t CountersScope::FindCounterIndexPrecedingCounter(
    const CounterNode& search_counter,
    const CountersVector& counters) {
  // comp returns true if the counter goes before the search_counter in preorder
  // tree traversal. We can have two counters on one element: use and non-use.
  // We want use counter to be after the non-use one, for this we need to return
  // true for the case, when result is 0 and the counter is non-use,
  // meaning we've hit the same element.
  // With such approach if we insert use counter in scope with non-use counter
  // on the same element, we will return the index of the non-use counter, and
  // if we insert non-use counter in the scope with use counter on the same
  // element, we will return the index of the element previous to the use
  // counter. As later we insert the counter to position at index + 1, we always
  // insert use counter after the non-use counter.
  // search_counter == counter is for the case where we search for the use
  // counter in scope with use counter and no non-use counter in scope. We need
  // to return counter, previous to the use counter in such case.
  //
  // Since each element can have up to two counters, they are arranged in the
  // following order:
  // [ e1 non-use, e1 use, e2 non-use, e2 use, ... ].
  // Now, let's say `e1 use` is not yet inserted and we need to find a place for
  // it. The upper_bound will return e2 non-use in this case, as it will be the
  // first one to return -1 (e.g. meaning that e1 use goes before the e2
  // non-use), after that we take prev, and return the index of e1 non-use,
  // which is correct, as it's the one preceding e1 use.
  // Now, let's say `e1 non-use` is not yet inserted. The upper bound will
  // return e1 use in this case, due to the `result == 0` condition. And the
  // return will be kNotFound, meaning we don't have any preceding counter,
  // which is correct. Now, let's say we have both e1 non-use and use inserted,
  // and the search counter is `e1 use`.
  // Let's see, how the array will be partitioned with respect to comp:
  //
  // Search counter - `e1 use`.
  // [ e1 non-use, e1 use, e2 non-use, e2 use, ... ].
  // [ comp: false, true , true      , true ],
  //
  // and the upper_bound will return `e1 use`, and the prev will give us the
  // index of `e1 non-use`, which is correct.
  //
  // Now, let's say we have both e1 non-use and use inserted,
  // and the search counter is `e1 non-use`. Let's
  // see, how the array will be partitioned with respect to comp:
  //
  // Search counter - `e1 non-use`.
  // [  e1 non-use, e1 use, e2 non-use, e2 use, ... ].
  // [ comp: true , true  , true      , true ],
  // and the upper_bound will return `e1 non-use`,
  // and the result will be kNotFound, which is correct, as there are no
  // counters preceding `e1 non-use`.

  // If possible, use fast path, where traversal positions are already
  // available.
  if (!counters.empty()) {
    const Element* search_element = &search_counter.OwnerElement();
    const Element* non_pseudo_search_element =
        &search_counter.OwnerNonPseudoElement();
    const Element* back_element = &counters.back()->OwnerElement();
    if (&search_counter == counters.back()) {
      return counters.size() > 1 ? counters.size() - 2 : kNotFound;
    }
    // The situation when the last existing counter is a pseudo element
    // of the search_counter's previous sibling.
    bool element_after_prev_sibling_pseudo =
        back_element->IsPseudoElement() &&
        search_element->previousSibling() ==
            &counters.back()->OwnerNonPseudoElement();
    // If the last existing counter is our parent.
    bool pseudo_after_parent = search_element->IsPseudoElement() &&
                               !search_counter.HasUseType() &&
                               non_pseudo_search_element == back_element;
    // If the last existing counter is our previous sibling.
    bool element_after_prev_sibling =
        search_element->previousSibling() == back_element;
    // If any of above is true, we can use the fast path, as we are
    // sure that we go after the last counter.
    if (element_after_prev_sibling_pseudo || pseudo_after_parent ||
        element_after_prev_sibling) {
      return counters.size() - 1;
    }
    const Element* front_element = &counters.front()->OwnerElement();
    // If the newly added counter is ::before of the first existing counter,
    // use the fast path.
    pseudo_after_parent = search_element->IsBeforePseudoElement() &&
                          !search_counter.HasUseType() &&
                          non_pseudo_search_element == front_element;
    if (pseudo_after_parent) {
      return 0;
    }
  }
  auto comp = [](const CounterNode& search_counter,
                 const CounterNode* counter) {
    int result = LayoutTreeBuilderTraversal::ComparePreorderTreePosition(
        search_counter.OwnerElement(), counter->OwnerElement());
    return result < 0 || (!search_counter.HasUseType() && result == 0) ||
           &search_counter == counter;
  };
  // Find the first counter for which comp will return true.
  auto* it =
      std::upper_bound(counters.begin(), counters.end(), search_counter, comp);
  // And get the previous counter as it will be the one we are searching for.
  return it == counters.begin() ? kNotFound
                                : wtf_size_t(std::prev(it) - counters.begin());
}

void CountersScope::Trace(Visitor* visitor) const {
  visitor->Trace(parent_);
  visitor->Trace(counters_);
  visitor->Trace(children_);
  visitor->Trace(scope_);
}

void CountersScope::AppendChild(CountersScope& child) {
  CHECK(!child.Parent());
  children_.emplace_back(&child);
  child.SetParent(this);
  child.SetIsDirty(true);
}

void CountersScope::RemoveChild(CountersScope& child) {
  CHECK_EQ(this, child.Parent());
  wtf_size_t pos = children_.Find(&child);
  CHECK_NE(pos, kNotFound);
  children_.EraseAt(pos);
  child.SetParent(nullptr);
  child.SetIsDirty(true);
}

void CountersScope::ClearChildren() {
  children_.clear();
}

Element& CountersScope::RootElement() const {
  // The first counter is the root of the scope.
  return FirstCounter().OwnerElement();
}

Element& CountersScope::RootNonPseudoElement() const {
  // The first counter is the root of the scope.
  return FirstCounter().OwnerNonPseudoElement();
}

CounterNode& CountersScope::FirstCounter() const {
  CHECK(!counters_.empty());
  return *counters_.front();
}

void CountersScope::ClearCounters() {
  counters_.clear();
}

void CountersScope::AttachCounter(CounterNode& counter) {
  // We add counters in such a way that we maintain them in the pre-order
  // traversal order. With such approach we don't need a linear tree traversal
  // to find the previous counter and can just perform a binary search instead.
  CHECK(!counter.IsInScope());
  counter.SetScope(this);
  wtf_size_t pos = FindCounterIndexPrecedingCounter(counter, counters_);
  if (pos == kNotFound) {
    counters_.push_front(counter);
  } else {
    counters_.insert(pos + 1u, counter);
  }
  is_dirty_ = true;
}

void CountersScope::DetachCounter(CounterNode& counter) {
  CHECK(!counter.IsInScope() || counter.Scope() == this);
  wtf_size_t pos = counters_.Find(&counter);
  CHECK_NE(pos, kNotFound);
  CHECK(!Parent() || pos != 0u)
      << "Can not detach the first counter when the parent is defined, as it "
         "can lead to reparenting";
  counters_.EraseAt(pos);
  counter.SetScope(nullptr);
  is_dirty_ = true;
}

CounterNode* CountersScope::FindPreviousCounterWithinStyleScope(
    CounterNode& counter,
    SearchScope search_scope) {
  for (CountersScope* scope =
           search_scope == SearchScope::AncestorSearch ? Parent() : this;
       scope; scope = scope->Parent()) {
    wtf_size_t pos =
        FindCounterIndexPrecedingCounter(counter, scope->Counters());
    if (pos != kNotFound) {
      return scope->Counters().at(pos).Get();
    }
    if (search_scope == SearchScope::SelfSearch) {
      return nullptr;
    }
  }
  return nullptr;
}

CounterNode* CountersScope::FindPreviousCounterInAncestorStyleScopes(
    CounterNode& counter,
    const AtomicString& identifier) {
  for (auto* ancestor = scope_->Parent(); ancestor;
       ancestor = ancestor->Parent()) {
    if (auto* scope_in_ancestor = ancestor->FindCountersScopeForElement(
            counter.OwnerElement(), identifier)) {
      return scope_in_ancestor->FindPreviousCounterFrom(
          counter,
          /* search_scope */ SearchScope::SelfAndAncestorSearch, identifier);
    }
  }
  return nullptr;
}

CounterNode* CountersScope::FindPreviousCounterFrom(
    CounterNode& counter,
    SearchScope search_scope,
    const AtomicString& identifier,
    bool leave_style_scope) {
  CounterNode* result =
      FindPreviousCounterWithinStyleScope(counter, search_scope);
  if (result || search_scope == SearchScope::SelfSearch || !leave_style_scope) {
    return result;
  }
  return FindPreviousCounterInAncestorStyleScopes(counter, identifier);
}

bool CountersScope::UpdateOwnCounters(bool force_update,
                                      const AtomicString& identifier) {
  if (!is_dirty_ && !force_update) {
    return false;
  }
  // If the first counter is of use type, search for the previous in pre-order
  // traversal order in parents' scopes to get the correct value.
  // https://drafts.csswg.org/css-contain/#example-6932a400.
  // But we set the value before for all the counters anyway,
  // so it can be easily used for counters() function.
  int value = 0;
  bool need_children_udpate = false;
  CounterNode* previous_counter = FindPreviousCounterFrom(
      FirstCounter(), /* search_scope */ SearchScope::AncestorSearch,
      identifier,
      /* leave_style_scope */ true);
  if (previous_counter) {
    value = previous_counter->ValueAfter();
    if (FirstCounter().PreviousInParent() != previous_counter) {
      need_children_udpate = true;
      FirstCounter().SetPreviousInParent(previous_counter);
    }
  }
  // The first increment should have the before value 0, if there has not been
  // any reset or set counter before.
  bool should_reset_increment = true;
  int num_counters_in_scope = counters_.size() - 1;
  if (FirstCounter().IsReversed() && FirstCounter().Value()) {
    num_counters_in_scope = FirstCounter().Value();
  }
  for (CounterNode* counter : counters_) {
    if (value != counter->ValueBefore()) {
      need_children_udpate = true;
    }
    counter->SetValueBefore(value);
    counter->CalculateValueAfter(should_reset_increment, num_counters_in_scope);
    if (!counter->HasUseType()) {
      should_reset_increment = false;
    }
    value = counter->ValueAfter();
  }
  is_dirty_ = false;
  return need_children_udpate;
}

void CountersScope::UpdateChildCounters(const AtomicString& identifier,
                                        bool force_update) {
  for (CountersScope* child : children_) {
    child->UpdateCounters(identifier, force_update);
  }
}

void CountersScope::UpdateCounters(const AtomicString& identifier,
                                   bool force_update) {
  bool force_update_children = UpdateOwnCounters(force_update, identifier);
  UpdateChildCounters(identifier, force_update_children);
}

}  // namespace blink
