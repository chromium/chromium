// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/counters_scope_tree.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/css/style_containment_scope.h"
#include "third_party/blink/renderer/core/css/style_containment_scope_tree.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/html/html_olist_element.h"
#include "third_party/blink/renderer/core/html/list_item_ordinal.h"
#include "third_party/blink/renderer/core/layout/counter_node.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"

namespace blink {

namespace {

bool IsAncestorOf(const Element& ancestor, const Element& element) {
  for (const Element* it = LayoutTreeBuilderTraversal::ParentElement(element);
       it; it = LayoutTreeBuilderTraversal::ParentElement(*it)) {
    if (it == ancestor) {
      return true;
    }
  }
  return false;
}

bool IsAncestorScopeElement(const Element& ancestor, const Element& child) {
  // Counter scope covers element, its descendants and following siblings
  // descendants.
  return LayoutTreeBuilderTraversal::ComparePreorderTreePosition(ancestor,
                                                                 child) <= 0 &&
         IsAncestorOf(*ancestor.ParentOrShadowHostElement(), child);
}

bool IsAncestorScopeElement(const Element& ancestor,
                            const Element& child,
                            const Element& old_parent) {
  // If the previous parent is direct ancestor and the new ancestor is not,
  // leave the old ancestor.
  if (IsAncestorOf(old_parent, child) && !IsAncestorOf(ancestor, child)) {
    return false;
  }
  // If they are both not direct ancestors, but the old parent goes before
  // the ancestor, leave old_parent, only if they are not siblings.
  if (!IsAncestorOf(old_parent, child) && !IsAncestorOf(ancestor, child) &&
      old_parent.ParentOrShadowHostElement() !=
          ancestor.ParentOrShadowHostElement() &&
      LayoutTreeBuilderTraversal::ComparePreorderTreePosition(old_parent,
                                                              ancestor) <= 0) {
    return false;
  }
  // Counter scope covers element, its descendants and following siblings
  // descendants.
  return IsAncestorOf(*ancestor.ParentOrShadowHostElement(), child) &&
         LayoutTreeBuilderTraversal::ComparePreorderTreePosition(ancestor,
                                                                 child) <= 0;
}

bool IsAncestorScope(CountersScope& ancestor, CountersScope& child) {
  return IsAncestorScopeElement(ancestor.RootElement(), child.RootElement());
}

void ReparentEmptyScope(CountersScope& scope) {
  CHECK(scope.Counters().empty());
  // As scope has no counters, move its children to its parent,
  // or leave them foster.
  CountersScope* parent = scope.Parent();
  for (CountersScope* child : scope.Children()) {
    child->SetParent(nullptr);
    if (parent) {
      parent->AppendChild(*child);
    }
  }
  if (parent) {
    parent->RemoveChild(scope);
  }
}

void MoveScopeDuringRemove(CountersScope& from,
                           CountersScope& to,
                           CounterNode* previous_in_parent) {
  // If during remove the first counter of `from` is removed,
  // we need to move the counters of `from` that are left to `to`.
  // For this we take the cached position of the first counter
  // from `from` in `to` and move all the `from` counters there.
  wtf_size_t pos_in_to = 0u;
  if (previous_in_parent) {
    pos_in_to = to.Counters().Find(previous_in_parent);
    pos_in_to = pos_in_to == kNotFound ? 0 : pos_in_to + 1;
  } else {
    to.FirstCounter().SetPreviousInParent(nullptr);
  }
  for (CounterNode* counter : from.Counters()) {
    counter->SetScope(&to);
    to.Counters().insert(pos_in_to++, counter);
  }
  from.ClearCounters();
  for (CountersScope* child : from.Children()) {
    child->SetParent(nullptr);
    to.AppendChild(*child);
  }
  from.ClearChildren();
  to.SetIsDirty(true);
}

void MoveScope(CountersScope& from, CountersScope& to) {
  if (!from.Counters().empty()) {
    for (CounterNode* counter : from.Counters()) {
      counter->SetScope(&to);
    }
    wtf_size_t pos = 1u;
    if (to.RootElement().PseudoAwareNextSibling() != from.RootElement()) {
      pos = CountersScope::FindCounterIndexPrecedingCounter(from.FirstCounter(),
                                                            to.Counters());
      pos = pos == kNotFound ? 0 : pos + 1;
    }
    to.Counters().InsertVector(pos, from.Counters());
    from.ClearCounters();
  }
  for (CountersScope* child : from.Children()) {
    child->SetParent(nullptr);
    to.AppendChild(*child);
  }
  from.ClearChildren();
  to.SetIsDirty(true);
}

void ReparentCounters(CountersScope& from, CountersScope& to) {
  CountersVector& counters = from.Counters();
  Vector<wtf_size_t> remove_positions;
  wtf_size_t start_pos =
      !counters.empty() && counters.front()->HasResetType() ? 1u : 0u;
  // Reparent only counters to which `to` is the new parent.
  for (wtf_size_t i = start_pos; i < counters.size(); ++i) {
    if (IsAncestorScopeElement(to.RootElement(), counters[i]->OwnerElement(),
                               from.RootElement())) {
      from.SetIsDirty(true);
      counters[i]->SetScope(nullptr);
      remove_positions.emplace_back(i);
      to.AttachCounter(*counters[i]);
    }
  }
  for (wtf_size_t pos : base::Reversed(remove_positions)) {
    counters.EraseAt(pos);
  }
}

void ReparentParentScopes(CountersScope& new_scope, CountersScope& parent) {
  // Reparent parent's scopes to which `new_scope` might've become parent.
  Vector<wtf_size_t> remove_positions;
  ScopesVector& children = parent.Children();
  for (wtf_size_t pos = 0u; pos < children.size(); ++pos) {
    CountersScope* child = children[pos];
    if (IsAncestorScopeElement(new_scope.RootElement(), child->RootElement(),
                               parent.RootElement())) {
      child->SetParent(nullptr);
      remove_positions.emplace_back(pos);
      new_scope.AppendChild(*child);
    }
  }
  for (wtf_size_t pos : base::Reversed(remove_positions)) {
    children.EraseAt(pos);
  }
  // Reparent parent's counters for which we might've become parent.
  ReparentCounters(parent, new_scope);
  // Parent will never be left empty.
  CHECK(!parent.Counters().empty());
}

void MoveOrReparentScope(CountersScope& from, CountersScope& to) {
  // If the counter that created the from scope is reset,
  // append from as a child to to.
  if (from.FirstCounter().HasResetType()) {
    to.AppendChild(from);
    ReparentParentScopes(to, from);
  } else {
    // Move counters from `from` to `to`.
    MoveScope(from, to);
  }
}

void ReparentFosterScopes(CountersScope& new_scope, ScopesVector& scopes) {
  // If the new_scope became parent to foster scopes, reparent such scopes or
  // move their counters to the new_scope and delete them.
  Vector<wtf_size_t> empty_positions;
  for (wtf_size_t pos = 0u; pos < scopes.size(); ++pos) {
    CountersScope* scope = scopes[pos];
    if (scope != &new_scope && !scope->Parent() &&
        IsAncestorScope(new_scope, *scope)) {
      MoveOrReparentScope(*scope, new_scope);
      if (scope->Counters().empty()) {
        empty_positions.emplace_back(pos);
      }
    }
  }
  for (wtf_size_t pos : base::Reversed(empty_positions)) {
    scopes.EraseAt(pos);
  }
}

CounterNode* CreateCounter(LayoutObject& object,
                           const AtomicString& identifier) {
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
    return MakeGarbageCollected<CounterNode>(object, type_mask, value);
  }
  return nullptr;
}

CounterNode* CreateListItemCounter(LayoutObject& object) {
  Node* node = object.GetNode();
  if (!node) {
    return nullptr;
  }
  if (ListItemOrdinal* ordinal = ListItemOrdinal::Get(*node)) {
    if (const auto& explicit_value = ordinal->ExplicitValue()) {
      return MakeGarbageCollected<CounterNode>(object, CounterNode::kResetType,
                                               explicit_value.value());
    }
    int value = ListItemOrdinal::IsInReversedOrderedList(*node) ? -1 : 1;
    return MakeGarbageCollected<CounterNode>(
        object, CounterNode::kIncrementType, value);
  }
  if (auto* olist = DynamicTo<HTMLOListElement>(node)) {
    int value = base::ClampAdd(olist->StartConsideringItemCount(),
                               olist->IsReversed() ? 1 : -1);
    return MakeGarbageCollected<CounterNode>(object, CounterNode::kResetType,
                                             value, olist->IsReversed());
  }
  if (IsA<HTMLUListElement>(node)) {
    return MakeGarbageCollected<CounterNode>(object, CounterNode::kResetType,
                                             0);
  }
  return nullptr;
}

bool PreorderTreePositionComparator(const Element& element,
                                    CountersScope* scope) {
  return LayoutTreeBuilderTraversal::ComparePreorderTreePosition(
             element, scope->RootElement()) < 0;
}

wtf_size_t FindScopePositionPrecedingElement(const Element& element,
                                             const ScopesVector& scopes) {
  auto* it = std::upper_bound(scopes.begin(), scopes.end(), element,
                              PreorderTreePositionComparator);
  return it == scopes.begin() ? kNotFound
                              : wtf_size_t(std::prev(it) - scopes.begin());
}

}  // namespace

void CountersScopeTree::Trace(Visitor* visitor) const {
  visitor->Trace(scopes_);
  visitor->Trace(style_scope_);
}

CountersScope* CountersScopeTree::FindScopeForElement(
    const Element& element,
    const AtomicString& identifier) {
  // At first check if we have counters scope with such identifier.
  auto scopes_it = scopes_.find(identifier);
  if (scopes_it == scopes_.end()) {
    return nullptr;
  }
  // Next find the scope whose root element goes before element in pre-order
  // traversal order.
  ScopesVector& scopes = *scopes_it->value;
  const ScopesVector::iterator it = std::upper_bound(
      scopes.begin(), scopes.end(), element, PreorderTreePositionComparator);
  // `it` points to the first root element that goes after `element`. So, if it
  // points to the `begin`, it means that all the root elements go after
  // `element` in pre-order traversal.
  if (it == scopes.begin()) {
    return nullptr;
  }
  // Now we need to find the scope to which `element` belong.
  // As per https://drafts.csswg.org/css-lists/#inheriting-counters
  // we should always inherit the counter from the parent element,
  // if that's not the case, inherit from the previous sibling.
  CountersScope* sibling_scope = nullptr;
  // Ancestor's sibling can be inherited via the ancestor.
  CountersScope* ancestor_sibling_scope = nullptr;
  ScopesVector::reverse_iterator rev_it(it);
  for (; rev_it != scopes.rend(); ++rev_it) {
    const Element* parent =
        (*rev_it)->RootElement().ParentOrShadowHostElement();
    bool is_ancestor_by_parent = IsAncestorOf(*parent, element);
    // Remember the first previous sibling.
    if (!sibling_scope && (!parent || is_ancestor_by_parent)) {
      sibling_scope = *rev_it;
    }
    // We need to find the upper-most scope which is the previous sibling of
    // one of the ancestors of element, but not sibling of the element.
    if (parent && is_ancestor_by_parent &&
        parent != element.ParentOrShadowHostElement() &&
        (!ancestor_sibling_scope ||
         ancestor_sibling_scope->RootElement().ParentOrShadowHostElement() !=
             parent)) {
      ancestor_sibling_scope = *rev_it;
    }
    // If we found direct ancestor.
    if (IsAncestorOf((*rev_it)->RootElement(), element) ||
        &(*rev_it)->RootElement() == &element) {
      return rev_it->Get();
    }
  }
  if (ancestor_sibling_scope) {
    return ancestor_sibling_scope;
  }
  return sibling_scope;
}

void CountersScopeTree::CreateScope(CounterNode& counter,
                                    CountersScope* parent,
                                    const AtomicString& identifier) {
  const Element& element = counter.OwnerElement();
  CountersScope* new_scope = MakeGarbageCollected<CountersScope>();
  new_scope->SetStyleScope(style_scope_);
  new_scope->AttachCounter(counter);
  auto it = scopes_.find(identifier);
  if (it != scopes_.end()) {
    // Insert new scope in correct pre-order traversal order with other scopes'
    // root elements.
    ScopesVector& scopes = *it->value;
    wtf_size_t pos = FindScopePositionPrecedingElement(element, scopes);
    scopes.insert(pos + 1, new_scope);
  } else {
    scopes_.insert(identifier,
                   MakeGarbageCollected<ScopesVector>(1u, new_scope));
    return;
  }
  // As per https://drafts.csswg.org/css-lists/#inheriting-counters
  // we don't take counter from previous sibling, if we create a new counter.
  if (parent && parent->RootElement().ParentOrShadowHostElement() ==
                    counter.OwnerElement().ParentOrShadowHostElement()) {
    ReparentParentScopes(*new_scope, *parent);
    parent = parent->Parent();
  }
  // We might've become parent to our parent's children scopes or counters. If
  // so, correctly reparent things.
  if (!parent) {
    ScopesVector& scopes = *it->value;
    ReparentFosterScopes(*new_scope, scopes);
  } else {
    ReparentParentScopes(*new_scope, *parent);
    parent->AppendChild(*new_scope);
  }
}

void CountersScopeTree::AttachCounter(CounterNode& counter,
                                      const AtomicString& identifier) {
  CHECK(!counter.Scope());
  CountersScope* scope =
      FindScopeForElement(counter.OwnerElement(), identifier);
  // counter-reset() or first in scope counter creates a new scope.
  if (counter.HasResetType() || !scope ||
      (scope->FirstCounter().HasUseType() && !counter.HasUseType())) {
    CreateScope(counter, scope, identifier);
  } else {
    scope->AttachCounter(counter);
  }
}

void CountersScopeTree::CreateCountersForLayoutObject(LayoutObject& object) {
  for (const auto& [identifier, directives] :
       *object.StyleRef().GetCounterDirectives()) {
    CounterNode* counter = CreateCounter(object, identifier);
    if (counter) {
      AttachCounter(*counter, identifier);
      // Add the counter info in the counters cache to remove
      // during when the flat tree traversal is not available.
      StyleScope()->GetStyleContainmentScopeTree()->AddCounterToObjectMap(
          object, identifier, *counter);
    }
  }
}

void CountersScopeTree::CreateCounterForLayoutObject(
    LayoutObject& object,
    const AtomicString& identifier) {
  CounterNode* counter = CreateCounter(object, identifier);
  if (counter) {
    AttachCounter(*counter, identifier);
    // Add the counter info in the counters cache to remove
    // during when the flat tree traversal is not available.
    StyleScope()->GetStyleContainmentScopeTree()->AddCounterToObjectMap(
        object, identifier, *counter);
  }
}

void CountersScopeTree::CreateListItemCounterForLayoutObject(
    LayoutObject& object) {
  CounterNode* counter = CreateListItemCounter(object);
  if (counter) {
    AttachCounter(*counter, list_item_);
    // Add the counter info in the counters cache to remove
    // during when the flat tree traversal is not available.
    StyleScope()->GetStyleContainmentScopeTree()->AddCounterToObjectMap(
        object, list_item_, *counter);
  }
}

void CountersScopeTree::RemoveEmptyScope(CountersScope& scope,
                                         const AtomicString& identifier) {
  auto it = scopes_.find(identifier);
  CHECK_NE(it, scopes_.end());
  ScopesVector& scopes = *it->value;
  wtf_size_t pos = scopes.Find(&scope);
  CHECK_NE(pos, kNotFound);
  scopes.EraseAt(pos);
  if (scopes.empty()) {
    scopes_.erase(it);
  }
}

void CountersScopeTree::RemoveCounterFromScope(CounterNode& counter,
                                               CountersScope& scope,
                                               const AtomicString& identifier) {
  // If the counter has been a root of the scope with parent,
  // we should reparent other counters in the scope, as they
  // will now be in scope of parent's root counter, as only one
  // counter-reset can be in the scope. Else, just remove the counter,
  // and if it has been the first one, but with no parent, the next counter
  // will become a new root.
  if (&counter == &scope.FirstCounter() && scope.Parent()) {
    scope.Counters().EraseAt(0u);
    if (scope.Counters().size()) {
      MoveScopeDuringRemove(scope, *scope.Parent(), counter.PreviousInParent());
    }
  } else {
    scope.DetachCounter(counter);
  }
  // Also delete the scope if it's empty.
  if (scope.Counters().empty()) {
    ReparentEmptyScope(scope);
    RemoveEmptyScope(scope, identifier);
  }
}

void CountersScopeTree::CreateCounterForLayoutCounter(LayoutCounter& counter) {
  CounterNode* counter_node = MakeGarbageCollected<CounterNode>(counter, 0u, 0);
  AttachCounter(*counter_node, counter.Identifier());
}

void CountersScopeTree::RemoveCounterForLayoutCounter(LayoutCounter& counter) {
  CounterNode* counter_node = counter.GetCounterNode();
  CHECK(counter_node);
  CHECK(counter_node->HasUseType());
  CountersScope* scope = counter_node->Scope();
  CHECK(scope);
  // We don't need to reparent the scope, as if the use counter is the root of
  // the scope, it means that all the children are non-reset counters, so we can
  // just delete the counter.
  if (counter_node == &scope->FirstCounter()) {
    scope->Counters().EraseAt(0u);
  } else {
    scope->DetachCounter(*counter_node);
  }
  if (scope->Counters().empty()) {
    ReparentEmptyScope(*scope);
    RemoveEmptyScope(*scope, counter.Identifier());
  }
}

void CountersScopeTree::UpdateCounters() {
  for (auto& [identifier, scopes] : scopes_) {
    for (CountersScope* scope : *scopes) {
      // Run update only from the top level scopes, as the update is recursive.
      if (!scope->Parent()) {
        scope->UpdateCounters(identifier);
      }
    }
  }
}

void CountersScopeTree::ReparentCountersToStyleScope(
    StyleContainmentScope& new_parent) {
  // This functions reparents all counters, for which the new style containment
  // scope has become a parent instead of the current style containment scope.
  // It will be more efficient to reparent the whole scopes and sub-scopes, but
  // for now we reparent all the counters individually.
  const Element* new_parent_element = new_parent.GetElement();
  CountersScopeTree* new_parent_tree = new_parent.GetCountersScopeTree();
  Vector<AtomicString> empty_identifiers;

  // Iterate over the identifier <-> scopes  pairs of current style containment
  // scope.
  for (auto& [identifier, scopes] : scopes_) {
    Vector<wtf_size_t> empty_scopes_positions;

    for (wtf_size_t scope_pos = 0u; scope_pos < scopes->size(); ++scope_pos) {
      CountersScope* scope = (*scopes)[scope_pos];
      Vector<wtf_size_t> remove_counters_positions;
      CountersVector& counters = scope->Counters();

      // Check for every counter, if it should belong to the new
      // style containment scope and move, if so.
      for (wtf_size_t counter_pos = 0u; counter_pos < counters.size();
           ++counter_pos) {
        CounterNode* counter = counters[counter_pos];
        if (!new_parent_element ||
            IsAncestorOf(*new_parent_element, counter->OwnerElement())) {
          counter->SetScope(nullptr);
          new_parent_tree->AttachCounter(*counter, identifier);
          remove_counters_positions.emplace_back(counter_pos);
        }
      }

      // If we moved all the counters from the scope, delete the scope.
      if (remove_counters_positions.size() != counters.size()) {
        for (wtf_size_t pos : base::Reversed(remove_counters_positions)) {
          counters.EraseAt(pos);
        }
        scope->SetIsDirty(true);
      } else {
        counters.clear();
        empty_scopes_positions.emplace_back(scope_pos);
      }
    }

    // If we moved all the scopes, remove the identifier <-> scopes pair.
    if (empty_scopes_positions.size() != scopes->size()) {
      for (wtf_size_t pos : base::Reversed(empty_scopes_positions)) {
        ReparentEmptyScope(*(*scopes)[pos]);
        scopes->EraseAt(pos);
      }
    } else {
      scopes->clear();
      empty_identifiers.emplace_back(identifier);
    }
  }

  for (const AtomicString& identifier : empty_identifiers) {
    scopes_.erase(identifier);
  }
}

#if DCHECK_IS_ON()
String CountersScopeTree::ToString(wtf_size_t depth) const {
  StringBuilder builder;
  for (auto& [identifier, scopes] : scopes_) {
    for (wtf_size_t i = 0; i < depth; ++i) {
      builder.Append(" ");
    }
    builder.AppendFormat("ID: %s [ \n", identifier.Ascii().c_str());
    for (CountersScope* scope : *scopes) {
      for (wtf_size_t i = 0; i < depth; ++i) {
        builder.Append(" ");
      }
      builder.AppendFormat(
          "CSCOPE AT: %s, parent %s { ",
          scope->FirstCounter().DebugName().Ascii().c_str(),
          scope->Parent()
              ? scope->Parent()->FirstCounter().DebugName().Ascii().c_str()
              : "NO");
      for (CounterNode* counter : scope->Counters()) {
        builder.AppendFormat("%s <%d>:<%d>; ",
                             counter->DebugName().Ascii().c_str(),
                             counter->ValueBefore(), counter->ValueAfter());
      }
      builder.Append(" }\n");
    }
    builder.Append(" ]\n");
  }
  builder.Append("\n");
  return builder.ReleaseString();
}
#endif  // DCHECK_IS_ON()

}  // namespace blink
