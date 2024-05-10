// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/counters_attachment_context.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/html/html_directory_element.h"
#include "third_party/blink/renderer/core/html/html_li_element.h"
#include "third_party/blink/renderer/core/html/html_menu_element.h"
#include "third_party/blink/renderer/core/html/html_olist_element.h"
#include "third_party/blink/renderer/core/html/html_ulist_element.h"
#include "third_party/blink/renderer/core/html/list_item_ordinal.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

bool IsAncestorOf(const Element& ancestor, const Element& descendant) {
  for (const Element* parent =
           LayoutTreeBuilderTraversal::ParentElement(descendant);
       parent; parent = LayoutTreeBuilderTraversal::ParentElement(*parent)) {
    if (parent == ancestor) {
      return true;
    }
  }
  return false;
}

std::optional<std::pair<unsigned, int>> DetermineCounterTypeAndValue(
    const LayoutObject& layout_object,
    const CounterDirectives& directives) {
  if (layout_object.IsText() && !layout_object.IsBR()) {
    return std::nullopt;
  }
  Node* generating_node = layout_object.GeneratingNode();
  // We must have a generating node or else we cannot have a counter.
  if (!generating_node) {
    return std::nullopt;
  }
  const ComputedStyle& style = layout_object.StyleRef();
  switch (style.StyleType()) {
    case kPseudoIdNone:
    case kPseudoIdBefore:
    case kPseudoIdAfter:
    case kPseudoIdMarker:
      break;
    default:
      return std::nullopt;  // Counters are forbidden from all other pseudo
                            // elements.
  }

  if (directives.IsDefined()) {
    unsigned type_mask = 0;
    int value = directives.CombinedValue();
    type_mask |= directives.IsIncrement()
                     ? static_cast<unsigned>(
                           CountersAttachmentContext::Type::kIncrementType)
                     : 0u;
    type_mask |=
        directives.IsReset()
            ? static_cast<unsigned>(CountersAttachmentContext::Type::kResetType)
            : 0;
    type_mask |=
        directives.IsSet()
            ? static_cast<unsigned>(CountersAttachmentContext::Type::kSetType)
            : 0;
    return std::make_pair(type_mask, value);
  }
  return std::nullopt;
}

inline bool IsReset(unsigned counter_type) {
  return counter_type &
         static_cast<unsigned>(CountersAttachmentContext::Type::kResetType);
}

inline bool IsSetOrReset(unsigned counter_type) {
  return counter_type & static_cast<unsigned>(
                            CountersAttachmentContext::Type::kResetType) ||
         counter_type &
             static_cast<unsigned>(CountersAttachmentContext::Type::kSetType);
}

int CalculateCounterValue(unsigned counter_type,
                          int counter_value,
                          int counter_current_value) {
  if (IsSetOrReset(counter_type)) {
    return counter_value;
  }
  return base::CheckAdd(counter_current_value, counter_value)
      .ValueOrDefault(counter_current_value);
}

}  // namespace

CountersAttachmentContext::CountersAttachmentContext()
    : counter_value_table_(MakeGarbageCollected<CounterValueTable>()),
      counter_inheritance_table_(
          MakeGarbageCollected<CounterInheritanceTable>()) {}

bool CountersAttachmentContext::ElementGeneratesListItemCounter(
    const Element& element) {
  return IsA<HTMLOListElement>(element) || IsA<HTMLUListElement>(element) ||
         IsA<HTMLLIElement>(element) || IsA<HTMLMenuElement>(element) ||
         IsA<HTMLDirectoryElement>(element);
}

void CountersAttachmentContext::EnterElement(const Element& element) {
  if (!attachment_root_is_document_element_) {
    return;
  }
  const ComputedStyle* style = element.GetComputedStyle();
  if (!style) {
    return;
  }
  const CounterDirectiveMap* counter_directives = style->GetCounterDirectives();
  if (!counter_directives) {
    // If this element doesn't have any counter directives on it,
    // it still can generate list-item counter or create a style
    // containment scope.
    if (ElementGeneratesListItemCounter(element)) {
      MaybeCreateListItemCounter(element);
    }
    if (style->ContainsStyle()) {
      EnterStyleContainmentScope();
    }
    return;
  }
  // Element without a box can't do counters operations
  // https://drafts.csswg.org/css-lists/#counters-without-boxes
  const LayoutObject* layout_object = element.GetLayoutObject();
  if (!layout_object) {
    return;
  }
  for (auto& [identifier, directives] : *counter_directives) {
    std::optional<std::pair<unsigned, int>> type_and_value =
        DetermineCounterTypeAndValue(*layout_object, directives);
    if (!type_and_value.has_value()) {
      continue;
    }
    // First, there might be some counters on stack that are stale,
    // remove those (e.g. remove counters whose parent is not
    // ancestor of `element` from stack).
    RemoveStaleCounters(identifier, element);
    auto [counter_type, counter_value] = type_and_value.value();
    // Reset counter always creates counter.
    if (IsReset(counter_type)) {
      CreateCounter(identifier, element, counter_value);
      continue;
    }
    // Otherswise, get the value of last counter from stack and update its
    // value.
    // Note: this can create counter, if there are no counters on stack.
    UpdateCounterValue(identifier, element, counter_type, counter_value);
  }
  // If there were no explicit counter related property set for `list-item`
  // counter, maybe we need to create implicit one.
  if (ElementGeneratesListItemCounter(element) &&
      counter_directives->find(list_item_) == counter_directives->end()) {
    MaybeCreateListItemCounter(element);
  }
  // Create style containment boundary if the element has contains style.
  // Doing it after counters creation as the element itself is not included
  // in the style containment scope.
  if (style->ContainsStyle()) {
    EnterStyleContainmentScope();
  }
}

void CountersAttachmentContext::LeaveElement(const Element& element) {
  if (!attachment_root_is_document_element_) {
    return;
  }
  const ComputedStyle* style = element.GetComputedStyle();
  if (!style) {
    return;
  }
  const CounterDirectiveMap* counter_directives = style->GetCounterDirectives();
  if (!counter_directives) {
    // If this element doesn't have any counter directives on it,
    // it still can generate list-item counter or create a style
    // containment scope.
    if (style->ContainsStyle()) {
      LeaveStyleContainmentScope();
    }
    if (ElementGeneratesListItemCounter(element)) {
      RemoveCounterIfAncestorExists(list_item_);
    }
    return;
  }
  // Element without a box can't do counters operations
  // https://drafts.csswg.org/css-lists/#counters-without-boxes
  const LayoutObject* layout_object = element.GetLayoutObject();
  if (!layout_object) {
    return;
  }
  // Remove style containment boundary if the element has contains style.
  // Doing it here as reverse to VisitElement.
  if (style->ContainsStyle()) {
    LeaveStyleContainmentScope();
  }
  for (auto& [identifier, directives] : *counter_directives) {
    std::optional<std::pair<unsigned, int>> type_and_value =
        DetermineCounterTypeAndValue(*layout_object, directives);
    if (!type_and_value.has_value()) {
      continue;
    }
    auto [counter_type, counter_value] = type_and_value.value();
    if (!IsReset(counter_type)) {
      continue;
    }
    // Remove self from stack if previous counter on stack is ancestor to
    // self. This is done since we should always inherit from ancestor first,
    // and in the case described, all next elements would inherit ancestor
    // instead of self, so remove self.
    RemoveCounterIfAncestorExists(identifier);
  }
  // If there were no explicit counter related property set for `list-item`
  // counter, maybe we need to remove implicit one.
  if (ElementGeneratesListItemCounter(element) &&
      counter_directives->find(list_item_) == counter_directives->end()) {
    RemoveCounterIfAncestorExists(list_item_);
  }
}

// Check if we need to create implicit list-item counter.
void CountersAttachmentContext::MaybeCreateListItemCounter(
    const Element& element) {
  RemoveStaleCounters(list_item_, element);
  if (ListItemOrdinal* ordinal = ListItemOrdinal::Get(element)) {
    if (const auto& explicit_value = ordinal->ExplicitValue()) {
      CreateCounter(list_item_, element, explicit_value.value());
      return;
    }
    int value = ListItemOrdinal::IsInReversedOrderedList(element) ? -1 : 1;
    unsigned type_mask =
        static_cast<unsigned>(CountersAttachmentContext::Type::kIncrementType);
    UpdateCounterValue(list_item_, element, type_mask, value);
    return;
  }
  if (auto* olist = DynamicTo<HTMLOListElement>(element)) {
    int value = base::ClampAdd(olist->StartConsideringItemCount(),
                               olist->IsReversed() ? 1 : -1);
    CreateCounter(list_item_, element, value);
    return;
  }
  if (IsA<HTMLUListElement>(element) || IsA<HTMLMenuElement>(element) ||
      IsA<HTMLDirectoryElement>(element)) {
    CreateCounter(list_item_, element, 0);
    return;
  }
}

// Traverse the stack and collect counters values for counter() and counters()
// functions.
Vector<int> CountersAttachmentContext::GetCounterValues(
    const AtomicString& identifier,
    const Element& element,
    bool only_last) {
  RemoveStaleCounters(identifier, element);
  Vector<int> counter_values;
  auto counter_stack_it = counter_inheritance_table_->find(identifier);
  if (counter_stack_it == counter_inheritance_table_->end()) {
    return {0};
  }
  CounterStack& counter_stack = *counter_stack_it->value;
  if (counter_stack.empty()) {
    return {0};
  }
  for (const Element* counter_element : base::Reversed(counter_stack)) {
    // counter() and counters() can cross style containment boundaries.
    if (counter_element == nullptr) {
      continue;
    }
    counter_values.push_back(GetCounterValue(identifier, *counter_element));
    if (only_last) {
      break;
    }
  }
  return counter_values;
}

// Push the counter on stack or create stack if there is none. Also set the
// value in the table.
void CountersAttachmentContext::CreateCounter(const AtomicString& identifier,
                                              const Element& owner,
                                              int value) {
  auto counter_stack_it = counter_inheritance_table_->find(identifier);
  if (counter_stack_it == counter_inheritance_table_->end()) {
    CounterStack* counter_stack =
        MakeGarbageCollected<CounterStack>(1u, &owner);
    counter_inheritance_table_->insert(identifier, counter_stack);
    SetCounterValue(identifier, owner, value);
    return;
  }
  CounterStack& counter_stack = *counter_stack_it->value;
  counter_stack.push_back(&owner);
  SetCounterValue(identifier, owner, value);
}

// Remove counters parent is not ancestor of current element from stack,
// meaning that we left the scope of such counter already, e.g.:
//        ()
//    ()--------(S)
// (R)-(I)-()
// R will create and put on stack counter;
// I will use it from stack, but when we visit the next sibling of I,
// we don't remove R from stack, even we leave its scope, as we would have
// to check every last child in the tree if there were any counters created
// on this level. Instead, once we reach S we pop all stale counters from stack,
// here R will be removed from stack.
void CountersAttachmentContext::RemoveStaleCounters(
    const AtomicString& identifier,
    const Element& owner) {
  auto counter_stack_it = counter_inheritance_table_->find(identifier);
  if (counter_stack_it == counter_inheritance_table_->end()) {
    return;
  }
  CounterStack& counter_stack = *counter_stack_it->value;
  while (!counter_stack.empty()) {
    // If we hit style containment boundary, stop.
    if (counter_stack.back() == nullptr) {
      break;
    }
    const Element* parent =
        LayoutTreeBuilderTraversal::ParentElement(*counter_stack.back());
    // We pop all elements whose parent is not ancestor of `owner`.
    if (!parent || IsAncestorOf(*parent, owner)) {
      break;
    }
    counter_stack.pop_back();
  }
}

// When leaving the element that created counter we might want to
// pop it from stack, if previous counter on stack is ancestor of it.
// This is done because we should always inherit counters from ancestor first,
// so, if the previous counter is ancestor to the last one, the last one will
// never be inherited, remove it.
void CountersAttachmentContext::RemoveCounterIfAncestorExists(
    const AtomicString& identifier) {
  auto counter_stack_it = counter_inheritance_table_->find(identifier);
  if (counter_stack_it == counter_inheritance_table_->end()) {
    return;
  }
  CounterStack& counter_stack = *counter_stack_it->value;
  // Don't remove the last on stack counter or style containment boundary.
  if (counter_stack.empty() || counter_stack.size() == 1 ||
      counter_stack.back() == nullptr) {
    return;
  }
  const Element* element = counter_stack.back();
  const Element* previous_element = counter_stack[counter_stack.size() - 2];
  if (previous_element && IsAncestorOf(*previous_element, *element)) {
    counter_stack.pop_back();
  }
}

// Set the counter's value by either updating the existing value or create
// a new record in the table, if there is no record yet.
void CountersAttachmentContext::SetCounterValue(const AtomicString& identifier,
                                                const Element& owner,
                                                int value) {
  auto counter_value_it = counter_value_table_->find(identifier);
  if (counter_value_it == counter_value_table_->end()) {
    CounterValues* counter_values = MakeGarbageCollected<CounterValues>();
    counter_values->insert(&owner, value);
    counter_value_table_->insert(identifier, counter_values);
    return;
  }
  CounterValues& counter_values = *counter_value_it->value;
  auto owner_value_it = counter_values.find(&owner);
  if (owner_value_it == counter_values.end()) {
    counter_values.insert(&owner, value);
    return;
  }
  owner_value_it->value = value;
}

// Get the value for counter created on `element`.
int CountersAttachmentContext::GetCounterValue(const AtomicString& identifier,
                                               const Element& element) {
  auto counter_stack_it = counter_inheritance_table_->find(identifier);
  if (counter_stack_it == counter_inheritance_table_->end()) {
    return 0;
  }
  CounterStack& counter_stack = *counter_stack_it->value;
  if (counter_stack.empty()) {
    return 0;
  }
  auto counter_value_it = counter_value_table_->find(identifier);
  DCHECK_NE(counter_value_it, counter_value_table_->end());
  CounterValues& counter_values = *counter_value_it->value;
  auto current_value_it = counter_values.find(&element);
  DCHECK_NE(current_value_it, counter_values.end());
  return current_value_it->value;
}

// Update the value of last on stack counter or create a new one, if there
// is no last counter on stack.
void CountersAttachmentContext::UpdateCounterValue(
    const AtomicString& identifier,
    const Element& element,
    unsigned counter_type,
    int counter_value) {
  int default_counter_value =
      CalculateCounterValue(counter_type, counter_value, 0);
  auto counter_stack_it = counter_inheritance_table_->find(identifier);
  // If there are no counters with such identifier, create stack and push
  // new counter on it.
  if (counter_stack_it == counter_inheritance_table_->end()) {
    CreateCounter(identifier, element, default_counter_value);
    return;
  }
  // If the stack is empty or the last element on stack is style containment
  // boundary, create and push counter on stack.
  CounterStack& counter_stack = *counter_stack_it->value;
  if (counter_stack.empty() || !counter_stack.back()) {
    CreateCounter(identifier, element, default_counter_value);
    return;
  }
  // Otherwise take the value of last counter on stack from the table and
  // update it.
  const Element* current = counter_stack.back();
  auto counter_value_it = counter_value_table_->find(identifier);
  DCHECK_NE(counter_value_it, counter_value_table_->end());
  CounterValues& counter_values = *counter_value_it->value;
  auto current_value_it = counter_values.find(current);
  DCHECK_NE(current_value_it, counter_values.end());
  current_value_it->value = CalculateCounterValue(counter_type, counter_value,
                                                  current_value_it->value);
}

void CountersAttachmentContext::EnterStyleContainmentScope() {
  // Push a style containment boundary (nullptr) to each existing stack.
  // Note: if there will be counters with new identifier created later,
  // it still will work correctly as we will remove all counters until
  // counter stack is empty, when we will leave style containment scope.
  for (auto& [identifier, counter_stack] : *counter_inheritance_table_) {
    counter_stack->push_back(nullptr);
  }
}

void CountersAttachmentContext::LeaveStyleContainmentScope() {
  // Pop counters until the stack is empty (happens if we created a counter with
  // a previously unseen identifier after we entered style containment scope) or
  // nullptr is the last on stack (we reached the style containment boundary).
  for (auto& [identifier, counter_stack] : *counter_inheritance_table_) {
    while (!counter_stack->empty() && counter_stack->back() != nullptr) {
      counter_stack->pop_back();
    }
    if (!counter_stack->empty() && counter_stack->back() == nullptr) {
      counter_stack->pop_back();
    }
  }
}

}  // namespace blink
