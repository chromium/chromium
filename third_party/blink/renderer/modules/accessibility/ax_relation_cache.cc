// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_relation_cache.h"

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/shadow_including_tree_order_traversal.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/modules/accessibility/ax_node_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object-inl.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "ui/accessibility/ax_common.h"

namespace blink {

namespace {
void IdsFromAttribute(const Element& element,
                      Vector<AtomicString>& ids,
                      const QualifiedName& attr_name) {
  SpaceSplitString split_ids(AXObject::AriaAttribute(element, attr_name));
  ids.AppendRange(split_ids.begin(), split_ids.end());
}
}  // namespace

AXRelationCache::AXRelationCache(AXObjectCacheImpl* object_cache)
    : object_cache_(object_cache) {}

AXRelationCache::~AXRelationCache() = default;

void AXRelationCache::Init() {
  // Init the relation cache with elements already present.
  // Normally, these relations would be cached when the node is first attached,
  // via AXObjectCacheImpl::NodeIsConnected().
  // The initial scan must include both flat traversal and node traversal,
  // othrwise some connected elements can be missed.
  DoInitialDocumentScan(object_cache_->GetDocument());
  if (Document* popup_doc = object_cache_->GetPopupDocumentIfShowing()) {
    DoInitialDocumentScan(*popup_doc);
  }

#if AX_FAIL_FAST_BUILD()
  is_initialized_ = true;
#endif
}

void AXRelationCache::DoInitialDocumentScan(Document& document) {
#if DCHECK_IS_ON()
  DCHECK(document.Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean)
      << "Unclean document at lifecycle " << document.Lifecycle().ToString();
#endif

  // TODO(crbug.com/1473733) Address flaw that all DOM ids are being cached
  // together regardless of their TreeScope, which can lead to conflicts.
  // Traverse all connected nodes in the document, via both DOM and shadow DOM.
  for (Node& node :
       ShadowIncludingTreeOrderTraversal::DescendantsOf(document)) {
    if (Element* element = DynamicTo<Element>(node)) {
      // Cache relations that do not require an AXObject.
      CacheRelations(*element);

      // Caching aria-owns requires creating target AXObjects.
      // TODO(crbug.com/41469336): Support aria-owns relations set via
      // explicitly set attr-elements on element or element internals.
      if (AXObject::HasAriaAttribute(*element, html_names::kAriaOwnsAttr)) {
        owner_axids_to_update_.insert(element->GetDomNodeId());
      }
    }
  }
}

void AXRelationCache::CacheRelations(Element& element) {
  DOMNodeId node_id = element.GetDomNodeId();

#if AX_FAIL_FAST_BUILD()
  // Register that the relations for this element have been cached, to
  // help enforce that relations are never missed.
  CHECK(node_id);
  processed_elements_.insert(node_id);
#endif

  UpdateRegisteredIdAttribute(element, node_id);

  // Register aria-owns.
  UpdateReverseOwnsRelations(element);

  // Register <label for>.
  // TODO(crbug.com/41469336): Track reverse relations set via explicitly set
  // attr-elements for htmlFor, when/if this is supported.
  const auto& for_id = element.FastGetAttribute(html_names::kForAttr);
  if (!for_id.empty()) {
    all_previously_seen_label_target_ids_.insert(for_id);
  }

  // Register aria-labelledby, aria-describedby relations.
  UpdateReverseTextRelations(element);

  // Register aria-activedescendant.
  UpdateReverseActiveDescendantRelations(element);

  // Register aria-controls, aria-details, aria-errormessage, aria-flowto, and
  // aria-actions.
  UpdateReverseOtherRelations(element);
}

#if AX_FAIL_FAST_BUILD()
void AXRelationCache::CheckRelationsCached(Element& element) {
  if (!is_initialized_) {
    return;
  }
  CheckElementWasProcessed(element);

  // Check aria-owns.
  Vector<AtomicString> owns_ids;
  HeapVector<Member<Element>> owns_elements;
  GetRelationTargets(element, html_names::kAriaOwnsAttr, owns_ids,
                     owns_elements);
  for (const auto& owns_id : owns_ids) {
    CHECK(aria_owns_id_map_.Contains(owns_id))
        << element << " with aria-owns=" << owns_id
        << " and DOMNodeId=" << DOMNodeIds::ExistingIdForNode(&element)
        << " should already be in cache.";
  }
  for (const Member<Element>& owns_element : owns_elements) {
    DOMNodeId owns_dom_node_id =
        DOMNodeIds::ExistingIdForNode(owns_element.Get());
    CHECK(owns_dom_node_id && aria_owns_node_map_.Contains(owns_dom_node_id))
        << element << " with ariaOwnsElements including " << owns_element
        << " and DOMNodeId=" << DOMNodeIds::ExistingIdForNode(&element)
        << " should already be in cache.";
  }

  // Check <label for>.
  // TODO(crbug.com/41469336): Track reverse relations set via explicitly set
  // attr-elements for htmlFor, when/if this is supported.
  if (IsA<HTMLLabelElement>(element)) {
    const auto& for_id = element.FastGetAttribute(html_names::kForAttr);
    if (!for_id.empty()) {
      CHECK(all_previously_seen_label_target_ids_.Contains(for_id))
          << element << " <label for=" << for_id
          << " with DOMNodeId=" << DOMNodeIds::ExistingIdForNode(&element)
          << " should already be in cache.";
    }
  }

  // Check aria-labelledby, aria-describedby.
  for (const auto& [attribute, filter] : GetTextRelationAttributes()) {
    Vector<AtomicString> text_relation_ids;
    HeapVector<Member<Element>> text_relation_elements;
    GetRelationTargets(element, attribute, text_relation_ids,
                       text_relation_elements);

    for (const auto& text_relation_id : text_relation_ids) {
      CHECK(aria_text_relations_id_map_.Contains(text_relation_id))
          << element << " with " << attribute << "=" << text_relation_id
          << " and DOMNodeId=" << DOMNodeIds::ExistingIdForNode(&element)
          << " should already be in cache.";
    }
    for (const Member<Element>& text_relation_element :
         text_relation_elements) {
      DOMNodeId text_relation_dom_node_id =
          DOMNodeIds::ExistingIdForNode(text_relation_element.Get());
      CHECK(text_relation_dom_node_id &&
            aria_text_relations_node_map_.Contains(text_relation_dom_node_id))
          << element << " with " << attribute
          << "-associated elements including " << text_relation_element
          << " and DOMNodeId=" << DOMNodeIds::ExistingIdForNode(&element)
          << " should already be in cache.";
    }
  }

  // Check aria-activedescendant.
  const AtomicString& activedescendant_id =
      AXObject::AriaAttribute(element, html_names::kAriaActivedescendantAttr);

  if (!activedescendant_id.empty()) {
    CHECK(aria_activedescendant_id_map_.Contains(activedescendant_id))
        << element << " with aria-activedescendant=" << activedescendant_id
        << " and DOMNodeId=" << DOMNodeIds::ExistingIdForNode(&element)
        << " should already be in cache.";
  } else {
    HeapVector<Member<Element>> activedescendant_elements;
    GetExplicitlySetElementsForAttr(element,
                                    html_names::kAriaActivedescendantAttr,
                                    activedescendant_elements);
    if (!activedescendant_elements.empty()) {
      Member<Element>& active_descendant_element = activedescendant_elements[0];
      DOMNodeId active_descendant_dom_node_id =
          DOMNodeIds::ExistingIdForNode(active_descendant_element);
      CHECK(active_descendant_dom_node_id &&
            aria_activedescendant_node_map_.Contains(
                active_descendant_dom_node_id))
          << element << " with ariaActiveDescendantElement "
          << active_descendant_element
          << " and DOMNodeId=" << DOMNodeIds::ExistingIdForNode(&element)
          << " should already be in cache.";
    }
  }
}

void AXRelationCache::CheckElementWasProcessed(Element& element) {
  DOMNodeId node_id = DOMNodeIds::ExistingIdForNode(&element);
  if (node_id && processed_elements_.Contains(node_id)) {
    return;
  }

  // Find first ancestor that was not processed.
  Node* ancestor = &element;
  if (element.GetDocument().IsFlatTreeTraversalForbidden()) {
    DVLOG(1) << "Note: flat tree traversal forbidden.";
  } else {
    while (true) {
      Node* next_ancestor = FlatTreeTraversal::Parent(*ancestor);
      if (!next_ancestor) {
        break;
      }
      if (!IsA<Element>(next_ancestor)) {
        break;
      }

      node_id = DOMNodeIds::ExistingIdForNode(next_ancestor);
      if (node_id && processed_elements_.Contains(node_id)) {
        // next_ancestor was not processed, therefore ancestor is the
        // top unprocessed node.
        break;
      }
      ancestor = next_ancestor;
    }
  }

  AXObject* obj = Get(ancestor);
  NOTREACHED()
      << "The following element was attached to the document, but "
         "UpdateCacheAfterNodeIsAttached() was never called with it, and it "
         "did not exist when the cache was first initialized:"
      << "\n* Element: " << ancestor
      << "\n* LayoutObject: " << ancestor->GetLayoutObject()
      << "\n* AXObject: " << obj << "\n"
      << (obj && obj->ParentObjectIncludedInTree()
              ? obj->ParentObjectIncludedInTree()->GetAXTreeForThis()
              : "");
}
#endif  // AX_FAIL_FAST_BUILD()

void AXRelationCache::ProcessUpdatesWithCleanLayout() {
  HashSet<DOMNodeId> old_owner_axids_to_update;
  old_owner_axids_to_update.swap(owner_axids_to_update_);

  for (DOMNodeId aria_owner_axid : old_owner_axids_to_update) {
    AXObject* obj = ObjectFromAXID(aria_owner_axid);
    if (obj) {
      UpdateAriaOwnsWithCleanLayout(obj);
    }
  }

  owner_axids_to_update_.clear();
}

bool AXRelationCache::IsDirty() const {
  return !owner_axids_to_update_.empty();
}

bool AXRelationCache::IsAriaOwned(const AXObject* child, bool check) const {
  if (!child)
    return false;
  DCHECK(!child->IsDetached()) << "Child was detached: " << child;
  bool is_owned =
      aria_owned_child_to_owner_mapping_.Contains(child->AXObjectID());
  if (is_owned) {
    return true;
  }

  if (!check) {
    return false;
  }

  // Ensure that unowned objects have the expected parent.
  AXObject* parent = child->ParentObjectIfPresent();
  if (parent && parent->GetElement() && child->GetElement() &&
      !child->GetElement()->IsPseudoElement()) {
    Node* natural_parent = AXObject::GetParentNodeForComputeParent(
        *object_cache_, child->GetElement());
    if (parent->GetNode() != natural_parent) {
      std::ostringstream msg;
      msg << "Unowned child should have natural parent:"
          << "\n* Child: " << child << "\n* Actual parent: " << parent
          << "\n* Natural ax parent: " << object_cache_->Get(natural_parent)
          << "\n* Natural dom parent: " << natural_parent << " #"
          << natural_parent->GetDomNodeId() << "\n* Owners to update:";
      for (AXID id : owner_axids_to_update_) {
        msg << " " << id;
      }
      DUMP_WILL_BE_CHECK(false) << msg.str();
    }
  }

  return false;
}

AXObject* AXRelationCache::GetAriaOwnedParent(const AXObject* child) const {
  // Child IDs may still be present in owning parents whose list of children
  // have been marked as requiring an update, but have not been updated yet.
  HashMap<AXID, AXID>::const_iterator iter =
      aria_owned_child_to_owner_mapping_.find(child->AXObjectID());
  if (iter == aria_owned_child_to_owner_mapping_.end())
    return nullptr;
  return ObjectFromAXID(iter->value);
}

AXObject* AXRelationCache::ValidatedAriaOwner(const AXObject* child) {
  if (!child->GetNode()) {
    return nullptr;
  }
  AXObject* owner = GetAriaOwnedParent(child);
  if (!owner || IsValidOwnsRelation(owner, *child->GetNode())) {
    return owner;
  }
  RemoveOwnedRelation(child->AXObjectID());
  return nullptr;
}

void AXRelationCache::GetExplicitlySetElementsForAttr(
    const Element& source,
    const QualifiedName& attr_name,
    HeapVector<Member<Element>>& target_elements) {
  if (auto* explicitly_set_elements =
          source.GetExplicitlySetElementsForAttr(attr_name);
      explicitly_set_elements) {
    for (const WeakMember<Element>& element : *explicitly_set_elements) {
      target_elements.push_back(element);
    }
    return;
  }

  const ElementInternals* element_internals = source.GetElementInternals();
  if (!element_internals) {
    return;
  }

  const FrozenArray<Element>* element_internals_attr_elements =
      element_internals->GetElementArrayAttribute(attr_name);

  if (!element_internals_attr_elements) {
    return;
  }

  target_elements = element_internals_attr_elements->AsVector();
}

void AXRelationCache::GetRelationTargets(
    const Element& source,
    const QualifiedName& attr_name,
    Vector<AtomicString>& target_ids,
    HeapVector<Member<Element>>& target_elements) {
  const AtomicString& ids = AXObject::AriaAttribute(source, attr_name);
  if (!ids.empty()) {
    // If the attribute is set to an ID list string, the IDs are the primary key
    // for the relation.
    IdsFromAttribute(source, target_ids, attr_name);
    return;
  }

  GetExplicitlySetElementsForAttr(source, attr_name, target_elements);
}

void AXRelationCache::UpdateReverseRelations(
    Element& source,
    const QualifiedName& attr_name,
    TargetIdToSourceNodeMap& id_map,
    TargetNodeToSourceNodeMap& node_map) {
  Vector<AtomicString> target_ids;
  HeapVector<Member<Element>> target_elements;
  GetRelationTargets(source, attr_name, target_ids, target_elements);
  UpdateReverseIdAttributeRelations(id_map, &source, target_ids);
  Vector<DOMNodeId> target_nodes;
  for (const Member<Element>& element : target_elements) {
    target_nodes.push_back(element->GetDomNodeId());
  }
  UpdateReverseElementAttributeRelations(node_map, &source, target_nodes);
}

void AXRelationCache::GetSingleRelationTarget(const Element& source,
                                              const QualifiedName& attr_name,
                                              AtomicString& target_id,
                                              Element** element) {
  const AtomicString& id = AXObject::AriaAttribute(source, attr_name);
  if (!id.empty()) {
    target_id = id;
    return;
  }

  HeapVector<Member<Element>> target_elements;
  GetExplicitlySetElementsForAttr(source, attr_name, target_elements);
  if (target_elements.empty()) {
    return;
  }

  DCHECK_EQ(target_elements.size(), 1u);
  *element = target_elements.at(0).Get();
}

void AXRelationCache::UpdateReverseSingleRelation(
    Element& source,
    const QualifiedName& attr_name,
    TargetIdToSourceNodeMap& id_map,
    TargetNodeToSourceNodeMap& node_map) {
  AtomicString target_id;
  Element* target_element = nullptr;
  GetSingleRelationTarget(source, attr_name, target_id, &target_element);

  if (!target_id.empty()) {
    UpdateReverseIdAttributeRelations(id_map, &source, {target_id});
    return;
  }

  if (!target_element) {
    return;
  }

  Vector<DOMNodeId> target_nodes;
  target_nodes.push_back(target_element->GetDomNodeId());
  UpdateReverseElementAttributeRelations(node_map, &source, target_nodes);
}

// Update reverse relation map, where source is related to target_ids.
void AXRelationCache::UpdateReverseIdAttributeRelations(
    TargetIdToSourceNodeMap& id_map,
    Node* source,
    const Vector<AtomicString>& target_ids) {
  // Add entries to reverse map.
  for (const AtomicString& target_id : target_ids) {
    auto result = id_map.insert(target_id, HashSet<DOMNodeId>());
    result.stored_value->value.insert(source->GetDomNodeId());
  }
}

// Update reverse relation map, where source is related to
// target_elements.
void AXRelationCache::UpdateReverseElementAttributeRelations(
    TargetNodeToSourceNodeMap& node_map,
    Node* source,
    const Vector<DOMNodeId>& target_nodes) {
  // Add entries to reverse map.
  for (const DOMNodeId& target_node : target_nodes) {
    auto result = node_map.insert(target_node, HashSet<DOMNodeId>());
    result.stored_value->value.insert(source->GetDomNodeId());
  }
}

base::span<std::pair<QualifiedName, Element::TinyBloomFilter>>
AXRelationCache::GetTextRelationAttributes() {
  // Avoid issues with commas within the type name in DEFINE_STATIC_LOCAL().
  using QualifiedNameArray =
      std::array<std::pair<QualifiedName, Element::TinyBloomFilter>, 3>;
  DEFINE_STATIC_LOCAL(
      QualifiedNameArray, text_attributes,
      ({{html_names::kAriaLabelledbyAttr,
         Element::FilterForAttribute(html_names::kAriaLabelledbyAttr)},
        {html_names::kAriaLabeledbyAttr,
         Element::FilterForAttribute(html_names::kAriaLabeledbyAttr)},
        {html_names::kAriaDescribedbyAttr,
         Element::FilterForAttribute(html_names::kAriaDescribedbyAttr)}}));
  return text_attributes;
}

void AXRelationCache::UpdateReverseTextRelations(Element& source) {
  for (const auto& [attribute, filter] : GetTextRelationAttributes()) {
    if (source.CouldMatchFilter(filter) || source.GetElementInternals()) {
      UpdateReverseTextRelations(source, attribute);
    }
  }
}

void AXRelationCache::UpdateReverseTextRelations(
    Element& source,
    const QualifiedName& attr_name) {
  Vector<AtomicString> id_vector;
  HeapVector<Member<Element>> target_elements;
  GetRelationTargets(source, attr_name, id_vector, target_elements);
  UpdateReverseIdAttributeTextRelations(source, id_vector);
  UpdateReverseElementAttributeTextRelations(source, target_elements);
}

void AXRelationCache::UpdateReverseIdAttributeTextRelations(
    Element& source,
    const Vector<AtomicString>& target_ids) {
  if (target_ids.empty()) {
    return;
  }

  Vector<AtomicString> new_target_ids;
  for (const AtomicString& id : target_ids) {
    if (aria_text_relations_id_map_.Contains(id)) {
      continue;
    }
    new_target_ids.push_back(id);
  }

  // Update the target ids so that the point back to the relation source node.
  UpdateReverseIdAttributeRelations(aria_text_relations_id_map_, &source,
                                    target_ids);

  // Mark all of the new text relation targets dirty.
  TreeScope& scope = source.GetTreeScope();
  for (const AtomicString& id : new_target_ids) {
    MarkNewRelationTargetDirty(scope.getElementById(id));
  }
}

void AXRelationCache::UpdateReverseElementAttributeTextRelations(
    Element& source,
    const HeapVector<Member<Element>>& target_elements) {
  if (target_elements.empty()) {
    return;
  }

  Vector<DOMNodeId> target_nodes;
  HeapVector<Member<Element>> new_target_elements;
  for (const Member<Element>& element : target_elements) {
    DOMNodeId dom_node_id = element->GetDomNodeId();
    target_nodes.push_back(dom_node_id);

    if (!aria_text_relations_node_map_.Contains(element->GetDomNodeId())) {
      new_target_elements.push_back(element);
    }
  }

  // Update the target nodes so that they point back to the relation source
  // node.
  UpdateReverseElementAttributeRelations(aria_text_relations_node_map_, &source,
                                         target_nodes);

  // Mark all of the new text relation targets dirty.
  for (const Member<Element>& element : new_target_elements) {
    MarkNewRelationTargetDirty(element.Get());
  }
}

void AXRelationCache::UpdateReverseActiveDescendantRelations(Element& source) {
  if (source.CouldHaveAttribute(html_names::kAriaActivedescendantAttr) ||
      source.GetElementInternals()) {
    UpdateReverseSingleRelation(source, html_names::kAriaActivedescendantAttr,
                                aria_activedescendant_id_map_,
                                aria_activedescendant_node_map_);
  }
}

void AXRelationCache::UpdateReverseOwnsRelations(Element& source) {
  if (source.CouldHaveAttribute(html_names::kAriaOwnsAttr) ||
      source.GetElementInternals()) {
    UpdateReverseRelations(source, html_names::kAriaOwnsAttr, aria_owns_id_map_,
                           aria_owns_node_map_);
  }
}

base::span<std::pair<QualifiedName, Element::TinyBloomFilter>>
AXRelationCache::GetOtherRelationAttributes() {
  // Avoid issues with commas within the type name in DEFINE_STATIC_LOCAL().
  using QualifiedNameArray =
      std::array<std::pair<QualifiedName, Element::TinyBloomFilter>, 5>;
  DEFINE_STATIC_LOCAL(
      QualifiedNameArray, attributes,
      ({{html_names::kAriaControlsAttr,
         Element::FilterForAttribute(html_names::kAriaControlsAttr)},
        {html_names::kAriaDetailsAttr,
         Element::FilterForAttribute(html_names::kAriaDetailsAttr)},
        {html_names::kAriaErrormessageAttr,
         Element::FilterForAttribute(html_names::kAriaErrormessageAttr)},
        {html_names::kAriaFlowtoAttr,
         Element::FilterForAttribute(html_names::kAriaFlowtoAttr)},
        {html_names::kAriaActionsAttr,
         Element::FilterForAttribute(html_names::kAriaActionsAttr)}}));
  return attributes;
}

void AXRelationCache::UpdateReverseOtherRelations(Element& source) {
  for (const auto& [attribute, filter] : GetOtherRelationAttributes()) {
    if (source.CouldMatchFilter(filter) || source.GetElementInternals()) {
      UpdateReverseRelations(source, attribute, aria_other_relations_id_map_,
                             aria_other_relations_node_map_);
    }
  }
}

void AXRelationCache::MarkNewRelationTargetDirty(Node* target) {
  // Mark root of label dirty so that we can change inclusion states as
  // necessary (label subtrees are included in the tree even if hidden).
  if (object_cache_->lifecycle().StateAllowsImmediateTreeUpdates()) {
    // WHen the relation cache is first initialized, we are already in
    // processing deferred events, and must manually invalidate the
    // cached values (is_used_for_label_or_description may have changed).
    if (AXObject* ax_target = Get(target)) {
      ax_target->InvalidateCachedValues(
          TreeUpdateReason::kNewRelationTargetDirty);
    }
    // Must use clean layout method.
    object_cache_->MarkElementDirtyWithCleanLayout(target);
  } else {
    // This will automatically invalidate the cached values of the target.
    object_cache_->MarkElementDirty(target);
  }
}

// ContainsCycle() should:
// * Return true when a cycle is an authoring error, but not an error in Blink.
// * CHECK failure when Blink should have caught this error earlier ... we
//   should have never gotten into this state.
//
// For example, if a web page specifies that grandchild owns it's grandparent,
// what should happen is the ContainsCycle will start at the grandchild and go
// up, finding that it's grandparent is already in the ancestor chain, and
// return false, thus disallowing the relation. However, if on the way to the
// root, it discovers that any other two objects are repeated in the ancestor
// chain, this is unexpected, and results in the CHECK() failure.
static bool ContainsCycle(AXObject* owner, Node& child_node) {
  if (FlatTreeTraversal::IsDescendantOf(*owner->GetNode(), child_node)) {
    // A DOM descendant cannot own its ancestor.
    return true;
  }
  HashSet<AXID> visited;
  // Walk up the parents of the owner object, make sure that this child
  // doesn't appear there, as that would create a cycle.
  for (AXObject* ancestor = owner; ancestor;
       ancestor = ancestor->ParentObject()) {
    if (ancestor->GetNode() == &child_node) {
      return true;
    }
    CHECK(visited.insert(ancestor->AXObjectID()).is_new_entry)
        << "Cycle in unexpected place:\n"
        << "* Owner = " << owner << "* Child = " << child_node;
  }
  return false;
}

bool AXRelationCache::IsValidOwnsRelation(AXObject* owner,
                                          Node& child_node) const {
  if (!IsValidOwner(owner)) {
    return false;
  }

  if (!IsValidOwnedChild(child_node)) {
    return false;
  }

  // If this child is already aria-owned by a different owner, continue.
  // It's an author error if this happens and we don't worry about which of
  // the two owners wins ownership, as long as only one of them does.
  if (AXObject* child = object_cache_->Get(&child_node)) {
    if (IsAriaOwned(child) && GetAriaOwnedParent(child) != owner) {
      return false;
    }
  }

  // You can't own yourself or an ancestor!
  if (ContainsCycle(owner, child_node)) {
    return false;
  }

  return true;
}

// static
bool AXRelationCache::IsValidOwner(AXObject* owner) {
  if (!owner->GetNode()) {
    NOTREACHED() << "Cannot use aria-owns without a node on both ends";
  }

  // Can't have element children.
  // <br> is special in that it is allowed to have inline textbox children,
  // but no element children.
  if (!owner->CanHaveChildren() || IsA<HTMLBRElement>(owner->GetNode())) {
    return false;
  }

  // An aria-owns is disallowed on editable roots and atomic text fields, such
  // as <input>, <textarea> and content editables, otherwise the result would be
  // unworkable and totally unexpected on the browser side.
  if (owner->IsTextField())
    return false;

  // A frame/iframe/fencedframe can only parent a document.
  if (AXObject::IsFrame(owner->GetNode()))
    return false;

  // Images can only use <img usemap> to "own" <area> children.
  // This requires special parenting logic, and aria-owns is prevented here in
  // order to keep things from getting too complex.
  if (owner->RoleValue() == ax::mojom::blink::Role::kImage)
    return false;

  // Many types of nodes cannot be used as parent in normal situations.
  // These rules also apply to allowing aria-owns.
  if (!AXObject::CanComputeAsNaturalParent(owner->GetNode()))
    return false;

  // Problematic for cycles, and does not solve a known use case.
  // Easiest to omit the possibility.
  if (owner->IsAriaHidden())
    return false;

  return true;
}

// static
bool AXRelationCache::IsValidOwnedChild(Node& child_node) {
  Element* child_element = DynamicTo<Element>(child_node);
  if (!child_element) {
    return false;
  }

  // Require a layout object, in order to avoid strange situations where
  // a node tries to parent an AXObject that cannot exist because its node
  // cannot partake in layout tree building (e.g. unused fallback content of a
  // media element). This is the simplest way to avoid many types of abnormal
  // situations, and there's no known use case for pairing aria-owns with
  // invisible content.
  if (!child_node.GetLayoutObject()) {
    return false;
  }

  // An area can't be owned, only parented by <img usemap>.
  if (IsA<HTMLAreaElement>(child_node)) {
    return false;
  }

  // <select> options can only be children of AXMenuListPopup or AXListBox.
  if (IsA<HTMLOptionElement>(child_node) ||
      IsA<HTMLOptGroupElement>(child_node)) {
    return false;
  }

  // aria-hidden is problematic for cycles, and does not solve a known use case.
  // Easiest to omit the possibility.
  if (AXObject::IsAriaAttributeTrue(*child_element,
                                    html_names::kAriaHiddenAttr)) {
    return false;
  }

  return true;
}

void AXRelationCache::UnmapOwnedChildrenWithCleanLayout(
    const AXObject* owner,
    const Vector<AXID>& removed_child_ids,
    Vector<AXID>& unparented_child_ids) {
  DCHECK(owner);
  DCHECK(!owner->IsDetached());
  for (AXID removed_child_id : removed_child_ids) {
    // Find the AXObject for the child that this owner no longer owns.
    AXObject* removed_child = ObjectFromAXID(removed_child_id);

    // It's possible that this child has already been owned by some other
    // owner, in which case we don't need to do anything other than marking
    // the original parent dirty.
    if (removed_child && GetAriaOwnedParent(removed_child) != owner) {
      ChildrenChangedWithCleanLayout(removed_child->ParentObjectIfPresent());
      continue;
    }

    // Remove it from the child -> owner mapping so it's not owned by this
    // owner anymore.
    aria_owned_child_to_owner_mapping_.erase(removed_child_id);

    if (removed_child) {
      // Return the unparented children so their parent can be restored after
      // all aria-owns changes are complete.
      unparented_child_ids.push_back(removed_child_id);
    }
  }
}

void AXRelationCache::MapOwnedChildrenWithCleanLayout(
    const AXObject* owner,
    const Vector<AXID>& child_ids) {
  DCHECK(owner);
  DCHECK(!owner->IsDetached());
  for (AXID added_child_id : child_ids) {
    AXObject* added_child = ObjectFromAXID(added_child_id);
    DCHECK(added_child);
    DCHECK(!added_child->IsDetached());

    // Invalidating ensures that cached "included in tree" state is recomputed
    // on objects with changed ownership -- owned children must always be
    // included in the tree.
    added_child->InvalidateCachedValues(TreeUpdateReason::kUpdateAriaOwns);

    // Add this child to the mapping from child to owner.
    aria_owned_child_to_owner_mapping_.Set(added_child_id, owner->AXObjectID());

    // Now detach the object from its original parent and call childrenChanged
    // on the original parent so that it can recompute its list of children.
    AXObject* original_parent = added_child->ParentObjectIfPresent();
    if (original_parent != owner) {
      if (original_parent) {
        added_child->DetachFromParent();
      }
      added_child->SetParent(const_cast<AXObject*>(owner));
      if (original_parent) {
        ChildrenChangedWithCleanLayout(original_parent);
        // Reparenting detection requires the parent of the original parent to
        // be reserialized.
        // This change prevents several DumpAccessibilityEventsTest failures:
        // - AccessibilityEventsSubtreeReparentedViaAriaOwns/linux
        // - AccessibilityEventsSubtreeReparentedViaAriaOwns2/linux
        // TODO(crbug.com/1299031) Find out why this is necessary.
        object_cache_->MarkAXObjectDirtyWithCleanLayout(
            original_parent->ParentObject());
      }
    }
    // Now that the child is owned, it's "included in tree" state must be
    // recomputed because owned children are always included in the tree.
    added_child->UpdateCachedAttributeValuesIfNeeded(false);

    // If the added child had a change in an inherited state because of the new
    // owner, that state needs to propagate into the subtree. Remove its
    // descendants so they are re-added with the correct cached states.
    // The new states would also be propagted in FinalizeTree(), but this is
    // safer for certain situations such as the aria-owns + aria-hidden state,
    // where the aria-hidden state could be invalidated late in the cycle due
    // to focus changes.
    if (added_child->ChildrenNeedToUpdateCachedValues()) {
      object_cache_->RemoveSubtree(added_child->GetNode(),
                                   /*remove_root*/ false);
    }
  }
}

void AXRelationCache::UpdateAriaOwnsFromAttrAssociatedElementsWithCleanLayout(
    AXObject* owner,
    const GCedHeapVector<Member<Element>>& attr_associated_elements,
    HeapVector<Member<AXObject>>& validated_owned_children_result,
    bool force) {
  CHECK(!object_cache_->IsFrozen());

  // attr-associated elements have already had their scope validated, but they
  // need to be further validated to determine if they introduce a cycle or are
  // already owned by another element.

  Vector<DOMNodeId> owned_dom_node_ids;
  for (const auto& element : attr_associated_elements) {
    CHECK(element);
    if (!IsValidOwnsRelation(const_cast<AXObject*>(owner), *element)) {
      continue;
    }
    AXObject* child = GetOrCreate(element, owner);
    if (!child) {
      return;
    }
    owned_dom_node_ids.push_back(element->GetDomNodeId());
    validated_owned_children_result.push_back(child);
  }

  // Track reverse relations for future tree updates.
  UpdateReverseElementAttributeRelations(aria_owns_node_map_, owner->GetNode(),
                                         owned_dom_node_ids);

  // Update the internal mappings of owned children.
  UpdateAriaOwnerToChildrenMappingWithCleanLayout(
      owner, validated_owned_children_result, force);
}

void AXRelationCache::ValidatedAriaOwnedChildren(
    const AXObject* owner,
    HeapVector<Member<AXObject>>& validated_owned_children_result) {
  if (!aria_owner_to_children_mapping_.Contains(owner->AXObjectID()))
    return;
  Vector<AXID> current_child_axids =
      aria_owner_to_children_mapping_.at(owner->AXObjectID());
  for (AXID child_id : current_child_axids) {
    AXObject* child = ObjectFromAXID(child_id);
    if (!child) {
      RemoveOwnedRelation(child_id);
    } else if (ValidatedAriaOwner(child) == owner) {
      validated_owned_children_result.push_back(child);
      DCHECK(IsAriaOwned(child))
          << "Owned child not in owned child map:"
          << "\n* Owner = " << owner << "\n* Child = " << child;
    }
  }
}

void AXRelationCache::UpdateAriaOwnsWithCleanLayout(AXObject* owner,
                                                    bool force) {
  CHECK(!object_cache_->IsFrozen());
  DCHECK(owner);
  Element* element = owner->GetElement();
  if (!element)
    return;

  DCHECK(!element->GetDocument().NeedsLayoutTreeUpdateForNode(*element));

  // A refresh can occur even if not a valid owner, because the old object
  // that |owner| is replacing may have previously been a valid owner. In this
  // case, the old owned child mappings will need to be removed.
  bool is_valid_owner = IsValidOwner(owner);
  if (!force && !is_valid_owner) {
    // Make sure that the owner's children are updated even in the case where
    // aria-owns is empty, or the object is not a valid owner. This protects
    // from ending up with a previous owner containing invalid children.
    ChildrenChangedWithCleanLayout(owner);
    return;
  }

  HeapVector<Member<AXObject>> owned_children;

  // We first check if the element has an explicitly set aria-owns association.
  // Explicitly set elements are validated when they are read (that they are in
  // a valid scope etc). The content attribute can contain ids that are not
  // legally ownable.
  if (!is_valid_owner) {
    DCHECK(force) << "Should not reach here except when an AXObject was "
                     "invalidated and is being refreshed: "
                  << owner;
  } else if (element && element->HasExplicitlySetAttrAssociatedElements(
                            html_names::kAriaOwnsAttr)) {
    // TODO (crbug.com/41469336): Also check ElementInternals here.
    UpdateAriaOwnsFromAttrAssociatedElementsWithCleanLayout(
        owner, *element->GetAttrAssociatedElements(html_names::kAriaOwnsAttr),
        owned_children, force);
  } else {
    // Figure out the ids that actually correspond to children that exist
    // and that we can legally own (not cyclical, not already owned, etc.) and
    // update the maps and |validated_owned_children_result| based on that.
    //
    // Figure out the children that are owned by this object and are in the
    // tree.
    TreeScope& scope = element->GetTreeScope();
    SpaceSplitString owned_id_vector(
        AXObject::AriaAttribute(*element, html_names::kAriaOwnsAttr));
    HeapVector<Member<Element>> valid_owned_child_elements;
    for (AtomicString id_name : owned_id_vector) {
      Element* child_element = scope.getElementById(id_name);
      if (!child_element ||
          !IsValidOwnsRelation(const_cast<AXObject*>(owner), *child_element)) {
        continue;
      }
      AXID future_child_axid = child_element->GetDomNodeId();
      HashMap<AXID, AXID>::const_iterator iter =
          aria_owned_child_to_owner_mapping_.find(future_child_axid);
      bool has_previous_owner =
          iter != aria_owned_child_to_owner_mapping_.end();
      if (has_previous_owner && owner->AXObjectID() != iter->value) {
        // Already has a different aria-owns parent.
        continue;
      }

      // Preemptively add the child to owner mapping to satisfy checks
      // that this child is owned, and therefore does not need to be added by
      // any other node who's subtree is eagerly updated during the
      // GetOrCreate() call, as this call recursively fills out subtrees.
      aria_owned_child_to_owner_mapping_.Set(future_child_axid,
                                             owner->AXObjectID());
      if (!has_previous_owner) {
        // Force UpdateAriaOwnerToChildrenMappingWithCleanLayout() to map
        // the new owner.
        force = true;
      }
      valid_owned_child_elements.emplace_back(child_element);
    }

    for (Element* child_element : valid_owned_child_elements) {
      AXObject* child = GetOrCreate(child_element, owner);
      if (child) {
        owned_children.push_back(child);
      }
    }
  }

  // Update the internal validated mapping of owned children. This will
  // fire an event if the mapping has changed.
  UpdateAriaOwnerToChildrenMappingWithCleanLayout(owner, owned_children, force);
}

void AXRelationCache::UpdateAriaOwnerToChildrenMappingWithCleanLayout(
    AXObject* owner,
    HeapVector<Member<AXObject>>& validated_owned_children_result,
    bool force) {
  DCHECK(owner);
  if (!owner->CanHaveChildren())
    return;

  Vector<AXID> validated_owned_child_axids;
  for (auto& child : validated_owned_children_result) {
    validated_owned_child_axids.push_back(child->AXObjectID());
  }

  // Compare this to the current list of owned children, and exit early if
  // there are no changes.
  Vector<AXID> previously_owned_child_ids;
  auto it = aria_owner_to_children_mapping_.find(owner->AXObjectID());
  if (it != aria_owner_to_children_mapping_.end()) {
    previously_owned_child_ids = it->value;
  }

  // Only force the refresh if there was or will be owned children; otherwise,
  // there is nothing to refresh even for a new AXObject replacing an old owner.
  if (previously_owned_child_ids == validated_owned_child_axids &&
      (!force || previously_owned_child_ids.empty())) {
    ChildrenChangedWithCleanLayout(owner);
    return;
  }

  // The list of owned children has changed. Even if they were just reordered,
  // to be safe and handle all cases we remove all of the current owned
  // children and add the new list of owned children.
  Vector<AXID> unparented_child_ids;
  UnmapOwnedChildrenWithCleanLayout(owner, previously_owned_child_ids,
                                    unparented_child_ids);
  MapOwnedChildrenWithCleanLayout(owner, validated_owned_child_axids);

#if DCHECK_IS_ON()
  // Owned children must be in tree to avoid serialization issues.
  for (AXObject* child : validated_owned_children_result) {
    DCHECK(IsAriaOwned(child));
    DCHECK(child->ComputeIsIgnoredButIncludedInTree())
        << "Owned child not in tree: " << child
        << "\nRecompute included in tree: "
        << child->ComputeIsIgnoredButIncludedInTree();
  }
#endif

  // Finally, update the mapping from the owner to the list of child IDs.
  if (validated_owned_child_axids.empty()) {
    aria_owner_to_children_mapping_.erase(owner->AXObjectID());
  } else {
    aria_owner_to_children_mapping_.Set(owner->AXObjectID(),
                                        validated_owned_child_axids);
  }

  // Ensure that objects that have lost their parent have one, or that their
  // subtree is pruned if there is no available parent.
  for (AXID unparented_child_id : unparented_child_ids) {
    if (validated_owned_child_axids.Contains(unparented_child_id)) {
      continue;
    }
    // Recompute the real parent and cache it.
    if (AXObject* ax_unparented = ObjectFromAXID(unparented_child_id)) {
      // Invalidating ensures that cached "included in tree" state is recomputed
      // on objects with changed ownership -- owned children must always be
      // included in the tree.
      ax_unparented->InvalidateCachedValues(TreeUpdateReason::kUpdateAriaOwns);

      // Find the unparented child's new parent, and reparent it to that
      // back to its real parent in the tree by finding  its current parent,
      // marking that dirty and detaching from that parent.
      AXObject* original_parent = ax_unparented->ParentObjectIfPresent();

      // Recompute the real parent .
      ax_unparented->DetachFromParent();
      MaybeRestoreParentOfOwnedChild(unparented_child_id);

      // Mark everything dirty so that the serializer sees all changes.
      ChildrenChangedWithCleanLayout(original_parent);
      ChildrenChangedWithCleanLayout(ax_unparented->ParentObjectIfPresent());
      if (!ax_unparented->IsDetached()) {
        object_cache_->MarkAXObjectDirtyWithCleanLayout(ax_unparented);
      }
    }
  }

  ChildrenChangedWithCleanLayout(owner);
}

bool AXRelationCache::MayHaveHTMLLabelViaForAttribute(
    const HTMLElement& labelable) {
  const AtomicString& id = labelable.GetIdAttribute();
  if (id.empty())
    return false;
  return all_previously_seen_label_target_ids_.Contains(id);
}

bool AXRelationCache::IsARIALabelOrDescription(Element& element) {
  // Labels and descriptions set by ariaLabelledByElements,
  // ariaDescribedByElements.
  if (aria_text_relations_node_map_.find(element.GetDomNodeId()) !=
      aria_text_relations_node_map_.end()) {
    return true;
  }

  // Labels and descriptions set by aria-labelledby, aria-describedby.
  const AtomicString& id_value = element.GetIdAttribute();
  if (id_value.IsNull()) {
    return false;
  }

  bool found_in_id_mapping = aria_text_relations_id_map_.find(id_value) !=
                             aria_text_relations_id_map_.end();
  return found_in_id_mapping;
}

// Fill source_objects with AXObjects for relations pointing to target.
void AXRelationCache::GetRelationSourcesById(
    const AtomicString& target_id_attr,
    TargetIdToSourceNodeMap& id_map,
    HeapVector<Member<AXObject>>& source_objects) {
  if (target_id_attr == g_null_atom) {
    return;
  }

  auto it = id_map.find(target_id_attr);
  if (it == id_map.end()) {
    return;
  }

  for (DOMNodeId source_node : it->value) {
    AXObject* source_object = Get(DOMNodeIds::NodeForId(source_node));
    if (source_object)
      source_objects.push_back(source_object);
  }
}

void AXRelationCache::GetRelationSourcesByElementReference(
    const DOMNodeId target_node,
    TargetNodeToSourceNodeMap& node_map,
    HeapVector<Member<AXObject>>& source_objects) {
  auto it = node_map.find(target_node);
  if (it == node_map.end()) {
    return;
  }

  for (const DOMNodeId& source_node : it->value) {
    AXObject* source_object = Get(DOMNodeIds::NodeForId(source_node));
    if (source_object) {
      source_objects.push_back(source_object);
    }
  }
}

AXObject* AXRelationCache::GetOrCreateAriaOwnerFor(Node* node, AXObject* obj) {
  CHECK(object_cache_->lifecycle().StateAllowsImmediateTreeUpdates());

  Element* element = DynamicTo<Element>(node);
  if (!element) {
    return nullptr;
  }

#if DCHECK_IS_ON()
  if (obj)
    DCHECK(!obj->IsDetached());
  AXObject* obj_for_node = object_cache_->Get(node);
  DCHECK(!obj || obj_for_node == obj)
      << "Object and node did not match:"
      << "\n* node = " << node << "\n* obj = " << obj
      << "\n* obj_for_node = " << obj_for_node;
#endif

  // Look for any new aria-owns relations.
  // Schedule an update on any potential new owner.
  HeapVector<Member<AXObject>> related_sources;
  GetRelationSourcesById(element->GetIdAttribute(), aria_owns_id_map_,
                         related_sources);
  GetRelationSourcesByElementReference(element->GetDomNodeId(),
                                       aria_owns_node_map_, related_sources);

  // First check for an existing aria-owns relation to the related AXObject.
  AXObject* ax_new_owner = nullptr;
  for (AXObject* related : related_sources) {
    if (related) {
      // Ensure that the candidate owner updates its children in its validity
      // as an owner is changing.
      owner_axids_to_update_.insert(related->AXObjectID());
      object_cache_->MarkAXObjectDirtyWithCleanLayout(related);
      related->SetNeedsToUpdateChildren();
      if (IsValidOwnsRelation(related, *node)) {
        if (!ax_new_owner) {
          ax_new_owner = related;
        }
        owner_axids_to_update_.insert(related->AXObjectID());
      }
    }
  }

  // Schedule an update on any previous owner. This owner takes priority over
  // any new owners.
  AXObject* related_target = obj ? obj : Get(node);
  if (related_target && IsAriaOwned(related_target)) {
    AXObject* ax_previous_owner = ValidatedAriaOwner(related_target);
    if (ax_previous_owner) {
      owner_axids_to_update_.insert(ax_previous_owner->AXObjectID());
      return ax_previous_owner;
    }
  }

  // Only the first aria-owns relation can be used.
  return ax_new_owner;
}

void AXRelationCache::UpdateRelatedTree(Node* node, AXObject* obj) {
  // This can happen if MarkAXObjectDirtyWithCleanLayout is
  // called and then UpdateRelatedTree is called on the same object,
  // e.g. in TextChangedWithCleanLayout.
  if (obj && obj->IsDetached()) {
    return;
  }

  if (GetOrCreateAriaOwnerFor(node, obj)) {
    // Ensure the aria-owns relation is processed, which in turn ensures that
    // both the owner and owned child exist, and that the parent-child
    // relations are correctly set on each.
    ProcessUpdatesWithCleanLayout();
  }

  // Update names and descriptions.
  UpdateRelatedText(node);
}

void AXRelationCache::UpdateRelatedTreeAfterChange(Element& element) {
  // aria-activedescendant requires special handling, because additional events
  // may be fired when it changes.
  // Check whether aria-activedescendant on the focused object points to
  // `element`. If so, fire activedescendantchanged event now. This is only for
  // ARIA active descendants, not in a native control like a listbox, which
  // has its own initial active descendant handling.
  MarkOldAndNewRelationSourcesDirty(element, aria_activedescendant_id_map_,
                                    aria_activedescendant_node_map_);
  Element* focused_element = element.GetDocument().FocusedElement();
  if (AXObject* ax_focus = Get(focused_element)) {
    if (AXObject::ElementFromAttributeOrInternals(
            focused_element, html_names::kAriaActivedescendantAttr) ==
        &element) {
      ax_focus->HandleActiveDescendantChanged();
    }
  }

  // aria-labelledby and aria-describedby.
  // Additional processing occurs in UpdateRelatedTree() when any node within
  // the label or description subtree changes.
  MarkOldAndNewRelationSourcesDirty(element, aria_text_relations_id_map_,
                                    aria_text_relations_node_map_);

  // aria-controls, aria-details, aria-errormessage, aria-flowto, and
  // aria-actions.
  MarkOldAndNewRelationSourcesDirty(element, aria_other_relations_id_map_,
                                    aria_other_relations_node_map_);
  UpdateReverseOtherRelations(element);

  // Finally, update the registered id attribute for this element.
  UpdateRegisteredIdAttribute(element, element.GetDomNodeId());
}

void AXRelationCache::UpdateRegisteredIdAttribute(Element& element,
                                                  DOMNodeId node_id) {
  const auto& id_attr = element.GetIdAttribute();
  if (id_attr == g_null_atom) {
    registered_id_attributes_.erase(node_id);
  } else {
    registered_id_attributes_.Set(node_id, id_attr);
  }
}

void AXRelationCache::UpdateRelatedText(Node* node) {
  // Shortcut: used cached value to determine whether this node contributes to
  // a name or description. Return early if not.
  AXObject* obj = Get(node);
  if (!obj || !obj->IsUsedForLabelOrDescription()) {
    // Nothing to do, as this node is not part of a label or description.
    return;
  }

  // Walk up ancestor chain from node and refresh text of any related content.
  while ((obj = obj->ParentObjectIncludedInTree()) != nullptr &&
         obj->IsUsedForLabelOrDescription()) {
    Element* ancestor_element = obj->GetElement();
    if (!ancestor_element) {
      // Can occur in the CSS column case.
      continue;
    }
    // Reverse relations via aria-labelledby, aria-describedby, aria-owns.
    HeapVector<Member<AXObject>> related_sources;
    GetRelationSourcesById(ancestor_element->GetIdAttribute(),
                           aria_text_relations_id_map_, related_sources);
    GetRelationSourcesByElementReference(ancestor_element->GetDomNodeId(),
                                         aria_text_relations_node_map_,
                                         related_sources);
    for (AXObject* related : related_sources) {
      if (related && related->IsIncludedInTree() &&
          !related->NeedsToUpdateChildren()) {
        object_cache_->MarkAXObjectDirtyWithCleanLayout(related);
      }
    }

    // Ancestors that may derive their accessible name from descendant content
    // should also handle text changed events when descendant content changes.
    if ((!obj->IsIgnored() || obj->CanSetFocusAttribute()) &&
        obj->SupportsNameFromContents(/*recursive=*/false) &&
        !obj->NeedsToUpdateChildren()) {
      object_cache_->MarkAXObjectDirtyWithCleanLayout(obj);
    }

    // Forward relation via <label for="[id]">.
    if (HTMLLabelElement* label =
            DynamicTo<HTMLLabelElement>(ancestor_element)) {
      object_cache_->MarkElementDirtyWithCleanLayout(LabelChanged(*label));
    }
  }
}

void AXRelationCache::MarkOldAndNewRelationSourcesDirty(
    Element& element,
    TargetIdToSourceNodeMap& id_map,
    TargetNodeToSourceNodeMap& node_map) {
  HeapVector<Member<AXObject>> related_sources;
  const AtomicString& id_attr = element.GetIdAttribute();
  GetRelationSourcesById(id_attr, id_map, related_sources);

  const DOMNodeId dom_node_id = element.GetDomNodeId();
  GetRelationSourcesByElementReference(dom_node_id, node_map, related_sources);

  // If id attribute changed, also mark old relation source dirty, and update
  // the map that points from the id attribute to the node id
  auto iter = registered_id_attributes_.find(element.GetDomNodeId());
  if (iter != registered_id_attributes_.end()) {
    const AtomicString& old_id_attr = iter->value;
    if (old_id_attr != id_attr) {
      GetRelationSourcesById(old_id_attr, id_map, related_sources);
    }
  }
  for (AXObject* related : related_sources) {
    object_cache_->MarkAXObjectDirtyWithCleanLayout(related);
  }
}

void AXRelationCache::UpdateCSSAnchorFor(Node* positioned_node) {
  // Remove existing mapping.
  AXID positioned_id = positioned_node->GetDomNodeId();
  if (positioned_obj_to_anchor_mapping_.Contains(positioned_id)) {
    AXID prev_anchor = positioned_obj_to_anchor_mapping_.at(positioned_id);
    anchor_to_positioned_obj_mapping_.erase(prev_anchor);
    positioned_obj_to_anchor_mapping_.erase(positioned_id);
    object_cache_->MarkAXObjectDirtyWithCleanLayout(
        ObjectFromAXID(prev_anchor));
  }

  LayoutBox* layout_box =
      DynamicTo<LayoutBox>(positioned_node->GetLayoutObject());
  if (!layout_box) {
    return;
  }

  Element* anchor = layout_box->AccessibilityAnchor();
  if (!anchor) {
    return;
  }

  // AccessibilityAnchor() only returns an anchor if there is one anchor, so
  // the map is only updated when there is a 1:1 anchor to positioned element
  // mapping.
  AXID anchor_id = anchor->GetDomNodeId();
  anchor_to_positioned_obj_mapping_.Set(anchor_id, positioned_id);
  positioned_obj_to_anchor_mapping_.Set(positioned_id, anchor_id);
  object_cache_->MarkElementDirtyWithCleanLayout(anchor);
}

AXObject* AXRelationCache::GetPositionedObjectForAnchor(
    const AXObject* anchor) {
  CHECK(!RuntimeEnabledFeatures::NoAriaDetailsForAnchorPosEnabled());
  HashMap<AXID, AXID>::const_iterator iter =
      anchor_to_positioned_obj_mapping_.find(anchor->AXObjectID());
  if (iter == anchor_to_positioned_obj_mapping_.end()) {
    return nullptr;
  }
  return ObjectFromAXID(iter->value);
}

AXObject* AXRelationCache::GetAnchorForPositionedObject(
    const AXObject* positioned_obj) {
  HashMap<AXID, AXID>::const_iterator iter =
      positioned_obj_to_anchor_mapping_.find(positioned_obj->AXObjectID());
  if (iter == positioned_obj_to_anchor_mapping_.end()) {
    return nullptr;
  }
  return ObjectFromAXID(iter->value);
}

void AXRelationCache::RemoveAXID(AXID obj_id) {
  // Need to remove from maps.
  // There are maps from children to their owners, and owners to their children.
  // In addition, the removed id may be an owner, or be owned, or both.

  // |obj_id| owned others:
  if (aria_owner_to_children_mapping_.Contains(obj_id)) {
    // |obj_id| no longer owns anything.
    Vector<AXID> child_axids = aria_owner_to_children_mapping_.at(obj_id);
    aria_owned_child_to_owner_mapping_.RemoveAll(child_axids);
    // Owned children are no longer owned by |obj_id|
    aria_owner_to_children_mapping_.erase(obj_id);
    // When removing nodes in AXObjectCacheImpl::Dispose we do not need to
    // reparent (that could anyway fail trying to attach to an already removed
    // node.
    // TODO(jdapena@igalia.com): explore if we can skip all processing of the
    // mappings in AXRelationCache in dispose case.
    if (!object_cache_->IsDisposing()) {
      for (const auto& child_axid : child_axids) {
        if (AXObject* owned_child = ObjectFromAXID(child_axid)) {
          owned_child->DetachFromParent();
          CHECK(object_cache_->lifecycle().StateAllowsReparentingAXObjects())
              << "Removing owned child at a bad time, which leads to "
                 "parentless objects at a bad time: "
              << owned_child;
        }
        MaybeRestoreParentOfOwnedChild(child_axid);
      }
    }
    registered_id_attributes_.erase(obj_id);
  }

  // Another id owned |obj_id|:
  RemoveOwnedRelation(obj_id);
}

void AXRelationCache::RemoveOwnedRelation(AXID obj_id) {
  // Another id owned |obj_id|.
  if (aria_owned_child_to_owner_mapping_.Contains(obj_id)) {
    CHECK(object_cache_->lifecycle().StateAllowsReparentingAXObjects());
    // Previous owner no longer relevant to this child.
    // Also, remove |obj_id| from previous owner's owned child list:
    AXID owner_id = aria_owned_child_to_owner_mapping_.Take(obj_id);
    if (aria_owner_to_children_mapping_.Contains(owner_id)) {
      const Vector<AXID>& owners_owned_children =
          aria_owner_to_children_mapping_.at(owner_id);
      for (wtf_size_t index = 0; index < owners_owned_children.size();
           index++) {
        if (owners_owned_children[index] == obj_id) {
          aria_owner_to_children_mapping_.at(owner_id).EraseAt(index);
          break;
        }
      }
    } else {
      // TODO(crbug.com/437579600) This is not a situation we expect, but it
      // also shouldn't cause a renderer crash. Once we have fixed the
      // underlying issue and verified that this dump does not exist in
      // telemetry, we should upgrade this to a NOTREACHED or remove the
      // `Contains(owner_id)` check above.
      DUMP_WILL_BE_NOTREACHED() << "Inconsistent aria-owns mapping: owner "
                                << owner_id << " not found";
    }
    if (AXObject* owner = ObjectFromAXID(owner_id)) {
      // The child is removed, so the owner needs to make sure its maps
      // are updated because it could point to something new or even back to the
      // same child if it's recreated, because it still has aria-owns markup.
      // The next call AXRelationCache::ProcessUpdatesWithCleanLayout()
      // will refresh this owner before the tree is frozen.
      owner_axids_to_update_.insert(owner_id);

      if (object_cache_->lifecycle().StateAllowsImmediateTreeUpdates()) {
        // Currently in CommitAXUpdates(). Changing the children of the owner
        // here could interfere with the execution of RemoveSubtree().
        object_cache_->MarkAXObjectDirtyWithCleanLayout(owner);
      } else {
        object_cache_->ChildrenChanged(owner);
      }
    }
    if (AXObject* owned_child = ObjectFromAXID(obj_id)) {
      owned_child->DetachFromParent();
    }
  }
}

AXObject* AXRelationCache::ObjectFromAXID(AXID axid) const {
  return object_cache_->ObjectFromAXID(axid);
}

AXObject* AXRelationCache::Get(Node* node) {
  return object_cache_->Get(node);
}

AXObject* AXRelationCache::GetOrCreate(Node* node, const AXObject* owner) {
  return object_cache_->GetOrCreate(node, const_cast<AXObject*>(owner));
}

void AXRelationCache::ChildrenChangedWithCleanLayout(AXObject* object) {
  if (!object) {
    return;
  }
  object->ChildrenChangedWithCleanLayout();
  object_cache_->MarkAXObjectDirtyWithCleanLayout(object);
}

Node* AXRelationCache::LabelChanged(HTMLLabelElement& label) {
  const auto& id = label.FastGetAttribute(html_names::kForAttr);
  if (id.empty()) {
    return nullptr;
  }

  all_previously_seen_label_target_ids_.insert(id);
  return label.Control();
}

void AXRelationCache::MaybeRestoreParentOfOwnedChild(AXID removed_child_axid) {
  // This works because AXIDs are equal to the DOMNodeID for their DOM nodes.
  if (Node* child_node = DOMNodeIds::NodeForId(removed_child_axid)) {
    object_cache_->RestoreParentOrPrune(child_node);
    // Handle case where there were multiple elements aria-owns=|child|,
    // by making sure they are updated in the next round, in case one of them
    // can now own it because of the removal the old_parent.
    HeapVector<Member<AXObject>> other_potential_owners;
    if (Element* child_element = DynamicTo<Element>(child_node)) {
      GetRelationSourcesById(child_element->GetIdAttribute(), aria_owns_id_map_,
                             other_potential_owners);
      for (AXObject* other_potential_owner : other_potential_owners) {
        owner_axids_to_update_.insert(other_potential_owner->AXObjectID());
      }
    }
  }
}

}  // namespace blink
