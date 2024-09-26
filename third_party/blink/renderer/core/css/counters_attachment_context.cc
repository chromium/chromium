// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/counters_attachment_context.h"

#include "base/containers/adapters.h"
#include "base/not_fatal_until.h"
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
  const ComputedStyle& style = layout_object.StyleRef();
  switch (style.StyleType()) {
    case kPseudoIdNone:
    case kPseudoIdBefore:
    case kPseudoIdAfter:
    case kPseudoIdMarker:
    case kPseudoIdScrollMarkerGroup:
    case kPseudoIdScrollMarker:
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

void CountersAttachmentContext::CounterEntry::Trace(Visitor* visitor) const {
  visitor->Trace(layout_object);
}

CountersAttachmentContext::CountersAttachmentContext()
    : counter_inheritance_table_(
          MakeGarbageCollected<CounterInheritanceTable>()) {}

bool CountersAttachmentContext::ElementGeneratesListItemCounter(
    const Element& element) {
  return IsA<HTMLOListElement>(element) || IsA<HTMLUListElement>(element) ||
         IsA<HTMLLIElement>(element) || IsA<HTMLMenuElement>(element) ||
         IsA<HTMLDirectoryElement>(element);
}

CountersAttachmentContext CountersAttachmentContext::DeepClone() const {
  CountersAttachmentContext clone(*this);
  clone.counter_inheritance_table_ =
      MakeGarbageCollected<CounterInheritanceTable>(
          *counter_inheritance_table_);
  for (auto& [counter_name, stack] : *clone.counter_inheritance_table_) {
    stack = MakeGarbageCollected<CounterStack>(*stack);
    for (Member<CounterEntry>& entry : *stack) {
      entry = MakeGarbageCollected<CounterEntry>(*entry);
    }
  }
  return clone;
}

void CountersAttachmentContext::EnterObject(const LayoutObject& layout_object,
                                            bool is_page_box) {
  if (!attachment_root_is_document_element_) {
    return;
  }
  const ComputedStyle& style = layout_object.StyleRef();
  const CounterDirectiveMap* counter_directives = style.GetCounterDirectives();
  if (counter_directives) {
    for (auto& [counter_name, directives] : *counter_directives) {
      std::optional<std::pair<unsigned, int>> type_and_value =
          DetermineCounterTypeAndValue(layout_object, directives);
      if (!type_and_value.has_value()) {
        continue;
      }
      auto [counter_type, value_argument] = type_and_value.value();
      ProcessCounter(layout_object, counter_name, counter_type, value_argument,
                     is_page_box);
    }
  }
  // If there were no explicit counter related property set for `list-item`
  // counter, maybe we need to create implicit one.
  if (const auto* element = DynamicTo<Element>(layout_object.GetNode())) {
    if (ElementGeneratesListItemCounter(*element) &&
        (!counter_directives ||
         counter_directives->find(list_item_) == counter_directives->end())) {
      MaybeCreateListItemCounter(*element);
    }
  }

  if (is_page_box) {
    // By default, @page boxes keep track of the page number. If the special
    // counter named "page" has no directives at all, increment it by one.
    //
    // See https://drafts.csswg.org/css-page-3/#page-based-counters
    const AtomicString page_str("page");
    if (!counter_directives ||
        counter_directives->find(page_str) == counter_directives->end()) {
      ProcessCounter(layout_object, page_str,
                     static_cast<unsigned>(Type::kIncrementType),
                     /*value_argument=*/1, is_page_box);
    }
  }

  // Create style containment boundary if the element has contains style.
  // Doing it after counters creation as the element itself is not included
  // in the style containment scope.
  if (style.ContainsStyle()) {
    EnterStyleContainmentScope();
  }
}

void CountersAttachmentContext::LeaveObject(const LayoutObject& layout_object,
                                            bool is_page_box) {
  if (!attachment_root_is_document_element_) {
    return;
  }
  const ComputedStyle& style = layout_object.StyleRef();
  // Remove style containment boundary if the element has contains style.
  // Doing it here as reverse to EnterObject().
  if (style.ContainsStyle()) {
    LeaveStyleContainmentScope();
  }
  const CounterDirectiveMap* counter_directives = style.GetCounterDirectives();
  if (counter_directives) {
    for (auto& [counter_name, directives] : *counter_directives) {
      std::optional<std::pair<unsigned, int>> type_and_value =
          DetermineCounterTypeAndValue(layout_object, directives);
      if (!type_and_value.has_value()) {
        continue;
      }
      auto [counter_type, counter_value] = type_and_value.value();
      if (!layout_object.GetNode()) {
        UnobscurePageCounterIfNeeded(counter_name, counter_type, is_page_box);
      }
      if (!IsReset(counter_type)) {
        continue;
      }
      // Remove self from stack if previous counter on stack is ancestor to
      // self. This is done since we should always inherit from ancestor first,
      // and in the case described, all next elements would inherit ancestor
      // instead of self, so remove self.
      RemoveCounterIfAncestorExists(layout_object, counter_name);
    }
  }
  // If there were no explicit counter related property set for `list-item`
  // counter, maybe we need to remove implicit one.
  if (const auto* element = DynamicTo<Element>(layout_object.GetNode())) {
    if (ElementGeneratesListItemCounter(*element) &&
        (!counter_directives ||
         counter_directives->find(list_item_) == counter_directives->end())) {
      RemoveCounterIfAncestorExists(layout_object, list_item_);
    }
  }
}

// Check if we need to create implicit list-item counter.
void CountersAttachmentContext::MaybeCreateListItemCounter(
    const Element& element) {
  const LayoutObject* layout_object = element.GetLayoutObject();
  DCHECK(layout_object);
  RemoveStaleCounters(*layout_object, list_item_);
  if (ListItemOrdinal* ordinal = ListItemOrdinal::Get(element)) {
    if (const auto& explicit_value = ordinal->ExplicitValue()) {
      CreateCounter(*layout_object, list_item_, explicit_value.value());
      return;
    }
    int value = ListItemOrdinal::IsInReversedOrderedList(element) ? -1 : 1;
    unsigned type_mask =
        static_cast<unsigned>(CountersAttachmentContext::Type::kIncrementType);
    UpdateCounterValue(*layout_object, list_item_, type_mask, value);
    return;
  }
  if (auto* olist = DynamicTo<HTMLOListElement>(element)) {
    int value = base::ClampAdd(olist->StartConsideringItemCount(),
                               olist->IsReversed() ? 1 : -1);
    CreateCounter(*layout_object, list_item_, value);
    return;
  }
  if (IsA<HTMLUListElement>(element) || IsA<HTMLMenuElement>(element) ||
      IsA<HTMLDirectoryElement>(element)) {
    CreateCounter(*layout_object, list_item_, 0);
    return;
  }
}

// Traverse the stack and collect counters values for counter() and counters()
// functions.
Vector<int> CountersAttachmentContext::GetCounterValues(
    const LayoutObject& layout_object,
    const AtomicString& counter_name,
    bool only_last) {
  RemoveStaleCounters(layout_object, counter_name);
  Vector<int> result;
  auto counter_stack_it = counter_inheritance_table_->find(counter_name);
  if (counter_stack_it == counter_inheritance_table_->end()) {
    return {0};
  }
  CounterStack& counter_stack = *counter_stack_it->value;
  if (counter_stack.empty()) {
    return {0};
  }

  // Counters changed within a page or page margin context obscure all counters
  // of the same name within the document.
  bool is_page_counter = false;

  for (const CounterEntry* entry : base::Reversed(counter_stack)) {
    // counter() and counters() can cross style containment boundaries.
    if (!entry) {
      if (is_page_counter) {
        // The boundary in this case is there to obscure counters defined in the
        // document (and also also page context, if we're in a page margin
        // context).
        break;
      }
      continue;
    }
    result.push_back(entry->value);
    if (only_last) {
      break;
    }
    if (!is_page_counter) {
      is_page_counter = !entry->layout_object->GetNode();
    }
  }
  return result;
}

void CountersAttachmentContext::ProcessCounter(
    const LayoutObject& layout_object,
    const AtomicString& counter_name,
    unsigned counter_type,
    int value_argument,
    bool is_page_box) {
  // First, there might be some counters on stack that are stale, remove
  // those (e.g. remove counters whose parent is not ancestors from stack).
  RemoveStaleCounters(layout_object, counter_name);

  // Counters in page boxes and page margin boxes may be special. If they are,
  // do the special stuff and return.
  if (!layout_object.GetNode() &&
      ObscurePageCounterIfNeeded(layout_object, counter_name, counter_type,
                                 value_argument, is_page_box)) {
    return;
  }

  // Reset counter always creates counter.
  if (IsReset(counter_type)) {
    CreateCounter(layout_object, counter_name, value_argument);
    return;
  }

  // Otherwise, get the value of last counter from stack and update its value.
  // Note: this can create counter, if there are no counters on stack.
  UpdateCounterValue(layout_object, counter_name, counter_type, value_argument);
}

bool CountersAttachmentContext::ObscurePageCounterIfNeeded(
    const LayoutObject& layout_object,
    const AtomicString& counter_name,
    unsigned counter_type,
    int value_argument,
    bool is_page_box) {
  DCHECK(!layout_object.GetNode());
  auto counter_stack_it = counter_inheritance_table_->find(counter_name);

  // If a counter is reset within a page context, this obscures all counters of
  // the same name within the document. The spec additionally says that this
  // should also happen to counters that are just incremented. But that would
  // make page counters completely useless, as we'd be unable to increment
  // counters across pages. So don't do that.
  // See https://github.com/w3c/csswg-drafts/issues/4759
  //
  // Similarly, if a counter is incremented or reset within a page *margin*
  // context, this obscures all counters of the same name within the document,
  // and in the page context.
  if (counter_stack_it == counter_inheritance_table_->end() ||
      (is_page_box && !IsReset(counter_type))) {
    return false;
  }
  CounterStack& counter_stack = *counter_stack_it->value;
  if (!counter_stack.empty() && counter_stack.back()) {
    int counter_value = CalculateCounterValue(counter_type, value_argument,
                                              counter_stack.back()->value);
    auto* new_entry =
        MakeGarbageCollected<CounterEntry>(layout_object, counter_value);
    // To obscure previous counters, push an empty entry onto the stack.
    counter_stack.push_back(nullptr);
    // Pushing nullptr entries is also a trick used by style containment. But
    // since the 'contain' property doesn't apply in a page / page margin
    // context, there should be no conflicts.
    DCHECK(!layout_object.ShouldApplyStyleContainment());
    // Then add the new counter with the new value.
    counter_stack.push_back(new_entry);

    return true;
  }

  return false;
}

void CountersAttachmentContext::UnobscurePageCounterIfNeeded(
    const AtomicString& counter_name,
    unsigned counter_type,
    bool is_page_box) {
  auto counter_stack_it = counter_inheritance_table_->find(counter_name);
  if (counter_stack_it == counter_inheritance_table_->end() ||
      (is_page_box && !IsReset(counter_type))) {
    return;
  }
  CounterStack& counter_stack = *counter_stack_it->value;
  while (!counter_stack.empty()) {
    // We should only pop page / page margin boxes.
    DCHECK(!counter_stack.back() ||
           !counter_stack.back()->layout_object->GetNode());

    bool is_boundary = !counter_stack.back();
    counter_stack.pop_back();
    if (is_boundary) {
      break;
    }
  }
}

// Push the counter on stack or create stack if there is none. Also set the
// value in the table.
// Also, per https://drafts.csswg.org/css-lists/#instantiating-counters: If
// innermost counter’s originating element is `layout_object` or a previous
// sibling of `layout_object`, remove innermost counter from counters.
void CountersAttachmentContext::CreateCounter(const LayoutObject& layout_object,
                                              const AtomicString& counter_name,
                                              int value) {
  auto* new_entry = MakeGarbageCollected<CounterEntry>(layout_object, value);
  auto counter_stack_it = counter_inheritance_table_->find(counter_name);
  if (counter_stack_it == counter_inheritance_table_->end()) {
    CounterStack* counter_stack =
        MakeGarbageCollected<CounterStack>(1u, new_entry);
    counter_inheritance_table_->insert(counter_name, counter_stack);
    return;
  }
  CounterStack& counter_stack = *counter_stack_it->value;
  if (const auto* element = DynamicTo<Element>(layout_object.GetNode())) {
    // Remove innermost counter with same or previous sibling originating
    // element.
    if (!counter_stack.empty() && counter_stack.back()) {
      const auto* current =
          To<Element>(counter_stack.back()->layout_object->GetNode());
      DCHECK(current);
      if (LayoutTreeBuilderTraversal::ParentElement(*current) ==
          LayoutTreeBuilderTraversal::ParentElement(*element)) {
        counter_stack.pop_back();
      }
    }
  }
  counter_stack.push_back(new_entry);
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
    const LayoutObject& layout_object,
    const AtomicString& counter_name) {
  const auto* element = DynamicTo<Element>(layout_object.GetNode());
  if (!element) {
    return;
  }
  auto counter_stack_it = counter_inheritance_table_->find(counter_name);
  if (counter_stack_it == counter_inheritance_table_->end()) {
    return;
  }
  CounterStack& counter_stack = *counter_stack_it->value;
  while (!counter_stack.empty()) {
    // If we hit style containment boundary, stop.
    const CounterEntry* entry = counter_stack.back();
    if (!entry) {
      break;
    }
    const LayoutObject& last_object = *entry->layout_object;
    if (const auto* last_element = DynamicTo<Element>(last_object.GetNode())) {
      const Element* parent =
          LayoutTreeBuilderTraversal::ParentElement(*last_element);
      // We pop all elements whose parent is not ancestor of `element`.
      if (!parent || IsAncestorOf(*parent, *element)) {
        break;
      }
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
    const LayoutObject& layout_object,
    const AtomicString& counter_name) {
  auto counter_stack_it = counter_inheritance_table_->find(counter_name);
  if (counter_stack_it == counter_inheritance_table_->end()) {
    return;
  }
  CounterStack& counter_stack = *counter_stack_it->value;
  // Don't remove the last on stack counter or style containment boundary.
  if (counter_stack.empty() || counter_stack.size() == 1) {
    return;
  }
  const CounterEntry* last_entry = counter_stack.back();
  // Also don't remove if the last counter's originating element is not
  // `layout_object`.
  if (!last_entry || last_entry->layout_object != layout_object) {
    return;
  }
  const CounterEntry* previous_entry = counter_stack[counter_stack.size() - 2];
  if (!previous_entry) {
    return;
  }
  const LayoutObject& previous_object = *previous_entry->layout_object;
  if (const auto* element = DynamicTo<Element>(layout_object.GetNode())) {
    const auto* previous_element =
        DynamicTo<Element>(previous_object.GetNode());
    if (previous_element && IsAncestorOf(*previous_element, *element)) {
      counter_stack.pop_back();
    }
  }
}

// Update the value of last on stack counter or create a new one, if there
// is no last counter on stack.
void CountersAttachmentContext::UpdateCounterValue(
    const LayoutObject& layout_object,
    const AtomicString& counter_name,
    unsigned counter_type,
    int counter_value) {
  int default_counter_value =
      CalculateCounterValue(counter_type, counter_value, 0);
  auto counter_stack_it = counter_inheritance_table_->find(counter_name);
  // If there are no counters with such counter_name, create stack and push
  // new counter on it.
  if (counter_stack_it == counter_inheritance_table_->end()) {
    CreateCounter(layout_object, counter_name, default_counter_value);
    return;
  }
  // If the stack is empty or the last element on stack is style containment
  // boundary, create and push counter on stack.
  CounterStack& counter_stack = *counter_stack_it->value;
  if (counter_stack.empty() || !counter_stack.back()) {
    CreateCounter(layout_object, counter_name, default_counter_value);
    return;
  }
  // Otherwise take the value of last counter on stack from the table and
  // update it.
  CounterEntry& current = *counter_stack.back();
  current.value =
      CalculateCounterValue(counter_type, counter_value, current.value);
}

void CountersAttachmentContext::EnterStyleContainmentScope() {
  // Push a style containment boundary (nullptr) to each existing stack.
  // Note: if there will be counters with new counter_name created later,
  // it still will work correctly as we will remove all counters until
  // counter stack is empty, when we will leave style containment scope.
  for (auto& [counter_name, counter_stack] : *counter_inheritance_table_) {
    counter_stack->push_back(nullptr);
  }
}

void CountersAttachmentContext::LeaveStyleContainmentScope() {
  // Pop counters until the stack is empty (happens if we created a counter with
  // a previously unseen counter_name after we entered style containment scope)
  // or nullptr is the last on stack (we reached the style containment
  // boundary).
  for (auto& [counter_name, counter_stack] : *counter_inheritance_table_) {
    while (!counter_stack->empty() && counter_stack->back() != nullptr) {
      counter_stack->pop_back();
    }
    if (!counter_stack->empty() && counter_stack->back() == nullptr) {
      counter_stack->pop_back();
    }
  }
}

}  // namespace blink
