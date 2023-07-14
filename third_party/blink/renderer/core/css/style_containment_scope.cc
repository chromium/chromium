// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_containment_scope.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/html/html_olist_element.h"
#include "third_party/blink/renderer/core/html/list_item_ordinal.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/layout/layout_quote.h"

namespace blink {

namespace {

using CounterMap = HeapHashMap<AtomicString, Member<CounterNode>>;
using CounterMaps =
    HeapHashMap<WeakMember<const LayoutObject>, Member<CounterMap>>;

}  // namespace

void StyleContainmentScope::Trace(Visitor* visitor) const {
  visitor->Trace(quotes_);
  visitor->Trace(children_);
  visitor->Trace(parent_);
  visitor->Trace(element_);
  visitor->Trace(counters_);
}

// If the scope is about to be removed, detach self from the parent,
// reattach the quotes, counters and the children scopes to the parent scope.
void StyleContainmentScope::ReattachToParent() {
  if (parent_) {
    auto quotes = std::move(quotes_);
    for (LayoutQuote* quote : quotes) {
      quote->SetScope(nullptr);
      parent_->AttachQuote(*quote);
    }
    auto counters = std::move(counters_);
    for (const auto& [identifier, counters_vec] : counters) {
      for (CounterNode* counter : *counters_vec) {
        counter->SetScope(nullptr);
        parent_->AttachCounter(*counter);
      }
    }
    auto children = std::move(children_);
    for (StyleContainmentScope* child : children) {
      child->SetParent(nullptr);
      parent_->AppendChild(child);
    }
    parent_->RemoveChild(this);
  }
}

bool StyleContainmentScope::IsAncestorOf(const Element* element,
                                         const Element* stay_within) {
  for (const Element* it = element; it && it != stay_within;
       it = LayoutTreeBuilderTraversal::ParentElement(*it)) {
    if (it == GetElement()) {
      return true;
    }
  }
  return false;
}

void StyleContainmentScope::AppendChild(StyleContainmentScope* child) {
  DCHECK(!child->Parent());
  children_.emplace_back(child);
  child->SetParent(this);
}

void StyleContainmentScope::RemoveChild(StyleContainmentScope* child) {
  DCHECK_EQ(this, child->Parent());
  wtf_size_t pos = children_.Find(child);
  DCHECK_NE(pos, kNotFound);
  children_.EraseAt(pos);
  child->SetParent(nullptr);
}

// Get the quote which would be the last in preorder traversal before we hit
// Element*.
const LayoutQuote* StyleContainmentScope::FindQuotePrecedingElement(
    const Element& element) const {
  // comp returns true if the element goes before quote in preorder tree
  // traversal.
  auto comp = [](const Element& element, const LayoutQuote* quote) {
    return LayoutTreeBuilderTraversal::ComparePreorderTreePosition(
               element, *quote->GetOwningPseudo()) < 0;
  };
  // Find the first quote for which comp will return true.
  auto* it = std::upper_bound(quotes_.begin(), quotes_.end(), element, comp);
  // And get the previous quote as it will be the one we are searching for.
  return it == quotes_.begin() ? nullptr : *std::prev(it);
}

void StyleContainmentScope::AttachQuote(LayoutQuote& quote) {
  DCHECK(!quote.IsInScope());
  quote.SetScope(this);
  // Find previous in preorder quote from the current scope.
  auto* pre_quote = FindQuotePrecedingElement(*quote.GetOwningPseudo());
  // Insert at 0 if we are the new head.
  wtf_size_t pos = pre_quote ? quotes_.Find(pre_quote) + 1u : 0u;
  quotes_.insert(pos, &quote);
}

void StyleContainmentScope::DetachQuote(LayoutQuote& quote) {
  if (!quote.IsInScope()) {
    return;
  }
  wtf_size_t pos = quotes_.Find(&quote);
  DCHECK_NE(pos, kNotFound);
  quotes_.EraseAt(pos);
  quote.SetScope(nullptr);
}

int StyleContainmentScope::ComputeInitialQuoteDepth() const {
  // Compute the depth of the previous quote from one of the parents.
  // Depth will be 0, if we are the first quote.
  for (StyleContainmentScope* parent = parent_; parent;
       parent = parent->Parent()) {
    const LayoutQuote* parent_quote =
        parent->FindQuotePrecedingElement(*quotes_.front()->GetOwningPseudo());
    if (parent_quote) {
      return parent_quote->GetNextDepth();
    }
  }
  return 0;
}

void StyleContainmentScope::UpdateQuotes() const {
  bool needs_children_update = false;
  if (quotes_.size()) {
    int depth = ComputeInitialQuoteDepth();
    for (LayoutQuote* quote : quotes_) {
      if (depth != quote->GetDepth()) {
        needs_children_update = true;
      }
      quote->SetDepth(depth);
      quote->UpdateText();
      depth = quote->GetNextDepth();
    }
  }
  // If nothing has changed on this level don't update children.
  if (needs_children_update || !quotes_.size()) {
    for (StyleContainmentScope* child : Children()) {
      child->UpdateQuotes();
    }
  }
}

CounterNode* StyleContainmentScope::GetCounterWithIdentifier(
    const LayoutObject& object,
    const AtomicString& identifier) const {
  auto it = counters_.find(identifier);
  if (it == counters_.end()) {
    return nullptr;
  }
  const CountersVector* vec = it->value;
  for (CounterNode* node : base::Reversed(*vec)) {
    if (node->Owner() == object) {
      return node;
    }
  }
  return nullptr;
}

CounterNode* StyleContainmentScope::CreateCounter(
    LayoutObject& object,
    const AtomicString& identifier) const {
  // Real text nodes don't have their own style so they can't have counters.
  // We can't even look at their styles or we'll see extra resets and
  // increments!
  if (object.IsText() && !object.IsBR()) {
    return nullptr;
  }
  Node* generating_node = object.GeneratingNode();
  // We must have a generating node or else we cannot have a counter.
  if (!generating_node) {
    return nullptr;
  }
  const ComputedStyle& style = object.StyleRef();
  switch (style.StyleType()) {
    case kPseudoIdNone:
      // Sometimes nodes have more than one layout object. Only the first one
      // gets the counter. See web_tests/http/tests/css/counter-crash.html
      if (generating_node->GetLayoutObject() != &object) {
        return nullptr;
      }
      break;
    case kPseudoIdBefore:
    case kPseudoIdAfter:
    case kPseudoIdMarker:
      break;
    default:
      return nullptr;  // Counters are forbidden from all other pseudo elements.
  }

  const CounterDirectives directives = style.GetCounterDirectives(identifier);
  if (directives.IsDefined()) {
    unsigned type_mask = 0;
    int value = directives.CombinedValue();
    type_mask |= directives.IsIncrement() ? CounterNode::kIncrementType : 0;
    type_mask |= directives.IsReset() ? CounterNode::kResetType : 0;
    type_mask |= directives.IsSet() ? CounterNode::kSetType : 0;
    return MakeGarbageCollected<CounterNode>(object, identifier, type_mask,
                                             value);
  }

  if (identifier == "list-item") {
    if (Node* node = object.GetNode()) {
      if (ListItemOrdinal* ordinal = ListItemOrdinal::Get(*node)) {
        if (const auto& explicit_value = ordinal->ExplicitValue()) {
          return MakeGarbageCollected<CounterNode>(object, identifier,
                                                   CounterNode::kResetType,
                                                   explicit_value.value());
        }
        int value = ListItemOrdinal::IsInReversedOrderedList(*node) ? -1 : 1;
        return MakeGarbageCollected<CounterNode>(
            object, identifier, CounterNode::kIncrementType, value);
      }
      if (auto* olist = DynamicTo<HTMLOListElement>(*node)) {
        int value = base::ClampAdd(olist->StartConsideringItemCount(),
                                   olist->IsReversed() ? 1 : -1);
        return MakeGarbageCollected<CounterNode>(
            object, identifier, CounterNode::kResetType, value);
      }
      if (IsA<HTMLUListElement>(*node) || IsA<HTMLMenuElement>(*node) ||
          IsA<HTMLDirectoryElement>(*node)) {
        return MakeGarbageCollected<CounterNode>(object, identifier,
                                                 CounterNode::kResetType, 0);
      }
    }
  }

  return nullptr;
}

void StyleContainmentScope::CreateCounterNodesForLayoutObject(
    LayoutObject& object) {
  for (const auto& [identifier, directives] :
       *object.StyleRef().GetCounterDirectives()) {
    CounterNode* counter = CreateCounter(object, identifier);
    AttachCounter(*counter);
  }
}

void StyleContainmentScope::DeleteCounterNodesForLayoutObject(
    LayoutObject& object,
    const ComputedStyle& style) {
  for (const auto& [identifier, directives] : *style.GetCounterDirectives()) {
    auto it = counters_.find(identifier);
    DCHECK_NE(it, counters_.end());
    CountersVector* vec = it->value;
    auto* vec_it = base::ranges::find_if(
        *vec, [&object](auto entry) { return entry->Owner() == object; });
    DCHECK_NE(vec_it, vec->end());
    vec->erase(vec_it);
    if (vec->empty()) {
      counters_.erase(it);
    }
  }
}

void StyleContainmentScope::AttachCounter(CounterNode& counter) {
  DCHECK(!counter.IsInScope());
  counter.SetScope(this);
  const AtomicString& identifier = counter.Identifier();
  auto it = counters_.find(identifier);
  if (it == counters_.end()) {
    counters_.insert(identifier,
                     MakeGarbageCollected<CountersVector>(1u, counter));
    return;
  }
  CountersVector* counters = it->value;
  const CounterNode* pre_counter =
      FindCounterPrecedingElement(counter.OwnerElement(), *counters);
  wtf_size_t pos = pre_counter ? counters->Find(pre_counter) + 1u : 0;
  counters->insert(pos, counter);
}

void StyleContainmentScope::DetachCounter(CounterNode& counter) {
  if (!counter.IsInScope()) {
    return;
  }
  const AtomicString& identifier = counter.Identifier();
  auto it = counters_.find(identifier);
  DCHECK_NE(it, counters_.end());
  wtf_size_t pos = it->value->Find(&counter);
  DCHECK_NE(pos, kNotFound);
  it->value->EraseAt(pos);
  if (it->value->empty()) {
    counters_.erase(it);
  }
  counter.SetScope(nullptr);
}

void StyleContainmentScope::AttachLayoutCounter(LayoutCounter& counter) {
  // TODO(sakhapov): add
  // DCHECK(!counter.GetCounterNode());
  // after LayoutCounter code is changed.
  CounterNode* counter_node =
      MakeGarbageCollected<CounterNode>(counter, counter.Identifier(), 0u, 0);
  AttachCounter(*counter_node);
}

void StyleContainmentScope::DetachLayoutCounter(LayoutCounter& counter) {
  DetachCounter(*counter.GetCounterNode());
}

const CounterNode* StyleContainmentScope::FindCounterPrecedingElement(
    const Element& element,
    const CountersVector& counters) {
  // comp returns true if the element goes before counter in preorder tree
  // traversal.
  auto comp = [](const Element& element, const CounterNode* counter) {
    return LayoutTreeBuilderTraversal::ComparePreorderTreePosition(
               element, counter->OwnerElement()) < 0;
  };
  // Find the first counter for which comp will return true.
  auto* it = std::upper_bound(counters.begin(), counters.end(), element, comp);
  // And get the previous counter as it will be the one we are searching for.
  return it == counters.begin() ? nullptr : *std::prev(it);
}

int StyleContainmentScope::ComputeInitialCounterValue(
    const CountersVector& counters) const {
  const AtomicString& identifier = counters.front()->Identifier();
  const auto& element = counters.front()->OwnerElement();
  for (StyleContainmentScope* parent = parent_; parent;
       parent = parent->Parent()) {
    auto it = parent->Counters().find(identifier);
    if (it == parent->Counters().end()) {
      continue;
    }
    CountersVector* parent_counters = it->value;
    const CounterNode* parent_counter =
        FindCounterPrecedingElement(element, *parent_counters);
    if (parent_counter) {
      return parent_counter->ValueAfter();
    }
  }
  return 0;
}

void StyleContainmentScope::UpdateAllCounters(
    HashSet<AtomicString>& no_update_identifiers) const {
  for (auto& [identifier, counters] : counters_) {
    if (!no_update_identifiers.Contains(identifier) &&
        !UpdateCounters(*counters)) {
      no_update_identifiers.insert(identifier);
    }
  }
  for (StyleContainmentScope* child : children_) {
    child->UpdateAllCounters(no_update_identifiers);
  }
}

bool StyleContainmentScope::UpdateCounters(CountersVector& counters) const {
  bool needs_children_update = false;
  bool should_reset_increment = true;
  int value = !counters.front()->HasUseType()
                  ? 0
                  : ComputeInitialCounterValue(counters);
  for (CounterNode* counter : counters) {
    // first increment should act as the counter was 0 before.
    if (!counter->HasUseType() && should_reset_increment) {
      should_reset_increment = false;
      value = 0;
    }
    if (!value || value != counter->ValueBefore()) {
      needs_children_update = true;
    }
    counter->SetValueBefore(value);
    if (auto* layout_counter = DynamicTo<LayoutCounter>(counter->Owner())) {
      // TODO(sakhapov): add
      // layout_counter->UpdateCounter();
      // once LayoutCounter code is changed.
    }
    value = counter->ValueAfter();
  }
  return needs_children_update;
}

}  // namespace blink
