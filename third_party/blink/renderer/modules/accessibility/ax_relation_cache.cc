// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_relation_cache.h"

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "ui/accessibility/ax_common.h"

namespace blink {

AXRelationCache::AXRelationCache(AXObjectCacheImpl* object_cache)
    : object_cache_(object_cache) {}

AXRelationCache::~AXRelationCache() = default;

void AXRelationCache::DoInitialDocumentScan() {
  // Init the relation cache with elements already in the document.
  Document& document = object_cache_->GetDocument();
  for (Element& element :
       ElementTraversal::DescendantsOf(*document.documentElement())) {
    const auto& id = element.FastGetAttribute(html_names::kForAttr);
    if (!id.empty())
      all_previously_seen_label_target_ids_.insert(id);

    // Ensure correct ancestor chains even when not all AXObject's in the
    // document are created, e.g. in the devtools accessibility panel.
    // Defers adding aria-owns targets as children of their new parents,
    // and to the relation cache, until the appropriate document lifecycle.
#if DCHECK_IS_ON()
    DCHECK(document.Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean)
        << "Unclean document at lifecycle " << document.Lifecycle().ToString();
#endif
    if (element.FastHasAttribute(html_names::kAriaOwnsAttr)) {
      if (AXObject* owner = GetOrCreate(&element, nullptr)) {
        owner_ids_to_update_.insert(owner->AXObjectID());
      }
    }
  }

  initialized_ = true;
}

void AXRelationCache::ProcessUpdatesWithCleanLayout() {
  if (!initialized_)
    DoInitialDocumentScan();

  HashSet<AXID> old_owner_ids_to_update;
  old_owner_ids_to_update.swap(owner_ids_to_update_);

  for (AXID aria_owns_obj_id : old_owner_ids_to_update) {
    AXObject* obj = ObjectFromAXID(aria_owns_obj_id);
    if (obj)
      UpdateAriaOwnsWithCleanLayout(obj);
  }

  // TODO(1301117): this is a workaround to avoid an infinite loop.
  // owner_ids_to_update_ is modified in calls to
  // UpdateAriaOwnsWithCleanLayout and add again AXIDs that will end up
  // looping forever in AXObjectCacheImpl::ProcessDeferredAccessibilityEvents
  owner_ids_to_update_.clear();
}

bool AXRelationCache::IsDirty() const {
  return !initialized_ || !owner_ids_to_update_.empty();
}

bool AXRelationCache::IsAriaOwned(const AXObject* child) const {
  if (!child)
    return false;
  DCHECK(!child->IsDetached())
      << "Child was detached: " << child->ToString(true, true);
  return aria_owned_child_to_owner_mapping_.Contains(child->AXObjectID());
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
  AXObject* owner = GetAriaOwnedParent(child);
  if (!owner || IsValidOwnsRelation(owner, const_cast<AXObject*>(child)))
    return owner;
  RemoveOwnedRelation(child->AXObjectID());
  return nullptr;
}

// Update reverse relation map, where relation_source is related to target_ids.
// TODO Support when HasExplicitlySetAttrAssociatedElement() == true.
void AXRelationCache::UpdateReverseRelations(
    HashMap<String, HashSet<DOMNodeId>>& id_attr_to_node_map,
    Node* relation_source,
    const Vector<String>& target_ids) {
  // Add entries to reverse map.
  for (const String& target_id : target_ids) {
    auto result = id_attr_to_node_map.insert(target_id, HashSet<DOMNodeId>());
    result.stored_value->value.insert(DOMNodeIds::IdForNode(relation_source));
  }
}

void AXRelationCache::UpdateReverseTextRelations(
    Node* relation_source,
    const Vector<String>& target_ids) {
  UpdateReverseRelations(id_attr_to_text_relation_mapping_, relation_source,
                         target_ids);
}

void AXRelationCache::UpdateReverseActiveDescendantRelations(
    Node* relation_source,
    const String& id) {
  Vector<String> ids = {id};
  UpdateReverseRelations(id_attr_to_active_descendant_mapping_, relation_source,
                         ids);
}

// ContainsCycle() should:
// * Return true when a cycle is an authoring error, but not an error in Blink.
// * CHECK(false) when Blink should have caught this error earlier ... we should
// have never gotten into this state.
//
// For example, if a web page specifies that grandchild owns it's grandparent,
// what should happen is the ContainsCycle will start at the grandchild and go
// up, finding that it's grandparent is already in the ancestor chain, and
// return false, thus disallowing the relation. However, if on the way to the
// root, it discovers that any other two objects are repeated in the ancestor
// chain, this is unexpected, and results in the CHECK(false) condition.
static bool ContainsCycle(AXObject* owner, AXObject* child) {
  HashSet<AXID> visited;
  // Walk up the parents of the owner object, make sure that this child
  // doesn't appear there, as that would create a cycle.
  for (AXObject* ancestor = owner; ancestor;
       ancestor = ancestor->CachedParentObject()) {
    if (ancestor == child)
      return true;
    CHECK(visited.insert(ancestor->AXObjectID()).is_new_entry)
        << "Cycle in unexpected place:\n"
        << "* Owner = " << owner->ToString(true, true)
        << "* Child = " << child->ToString(true, true);
  }
  return false;
}

bool AXRelationCache::IsValidOwnsRelation(AXObject* owner,
                                          AXObject* child) const {
  if (!IsValidOwner(owner) || !IsValidOwnedChild(child))
    return false;

  // If this child is already aria-owned by a different owner, continue.
  // It's an author error if this happens and we don't worry about which of
  // the two owners wins ownership, as long as only one of them does.
  if (IsAriaOwned(child) && GetAriaOwnedParent(child) != owner)
    return false;

  // You can't own yourself or an ancestor!
  if (ContainsCycle(owner, child))
    return false;

  return true;
}

// static
bool AXRelationCache::IsValidOwner(AXObject* owner) {
  if (!owner->GetNode()) {
    NOTREACHED() << "Cannot use aria-owns without a node on both ends";
    return false;
  }

  // Can't have children.
  if (!owner->CanHaveChildren())
    return false;

  // An aria-owns is disallowed on editable roots and atomic text fields, such
  // as <input>, <textarea> and content editables, otherwise the result would be
  // unworkable and totally unexpected on the browser side.
  if (owner->IsTextField())
    return false;

  // Temporary fix for M117 only. Ignore aria-owns on HTML <label for> where
  // they have the same value. This redundant markup is using angular and
  // causes an OOM crash, in 117 only. See crbug.com/1484715.
  if (auto* label = DynamicTo<HTMLLabelElement>(owner->GetNode())) {
    if (label->getAttribute(html_names::kForAttr) ==
        label->getAttribute(html_names::kAriaOwnsAttr)) {
      return false;
    }
  }

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
bool AXRelationCache::IsValidOwnedChild(AXObject* child) {
  if (!child)
    return false;

  Node* node = child->GetNode();
  if (!node) {
    NOTREACHED() << "Cannot use aria-owns without a node on both ends";
    return false;
  }

  // Require a layout object, in order to avoid strange situations where
  // a node tries to parent an AXObject that cannot exist because its node
  // cannot partake in layout tree building (e.g. unused fallback content of a
  // media element). This is the simplest way to avoid many types of abnormal
  // situations, and there's no known use case for pairing aria-owns with
  // invisible content.
  if (!node->GetLayoutObject())
    return false;

  if (child->IsImageMapLink())
    return false;  // An area can't be owned, only parented by <img usemap>.

  // <select> options can only be children of AXMenuListPopup or AXListBox.
  if (IsA<HTMLOptionElement>(node) || IsA<HTMLOptGroupElement>(node))
    return false;

  // Problematic for cycles, and does not solve a known use case.
  // Easiest to omit the possibility.
  if (child->IsAriaHidden())
    return false;

  return true;
}

void AXRelationCache::UnmapOwnedChildrenWithCleanLayout(
    const AXObject* owner,
    const Vector<AXID>& removed_child_ids,
    const Vector<AXID>& newly_owned_ids) {
  DCHECK(owner);
  DCHECK(!owner->IsDetached());
  for (AXID removed_child_id : removed_child_ids) {
    // Find the AXObject for the child that this owner no longer owns.
    AXObject* removed_child = ObjectFromAXID(removed_child_id);

    // It's possible that this child has already been owned by some other
    // owner, in which case we don't need to do anything.
    if (removed_child && GetAriaOwnedParent(removed_child) != owner)
      continue;

    // Remove it from the child -> owner mapping so it's not owned by this
    // owner anymore.
    aria_owned_child_to_owner_mapping_.erase(removed_child_id);

    if (removed_child) {
      // Invalidating ensures that cached "included in tree" state is recomputed
      // on objects with changed ownership -- owned children must always be
      // included in the tree.
      removed_child->InvalidateCachedValues();
      // If the child still exists, find its "real" parent, and reparent it
      // back to its real parent in the tree by detaching it from its current
      // parent and calling childrenChanged on its real parent.
      removed_child->DetachFromParent();
      // Recompute the real parent and cache it.
      // Don't do this if it's also in the newly owned ids, as it's about to
      // get a new parent, and we want to avoid accidentally pruning it.
      if (!newly_owned_ids.Contains(removed_child_id)) {
        MaybeRestoreParentOfOwnedChild(removed_child);

        // Now that the child is not owned, it's "included in tree" state must
        // be recomputed because while owned children are always included in the
        // tree, unowned children may not be included.
        removed_child->UpdateCachedAttributeValuesIfNeeded(false);
      }
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
    added_child->InvalidateCachedValues();

    // Add this child to the mapping from child to owner.
    aria_owned_child_to_owner_mapping_.Set(added_child_id, owner->AXObjectID());

    // Now detach the object from its original parent and call childrenChanged
    // on the original parent so that it can recompute its list of children.
    AXObject* original_parent = added_child->CachedParentObject();
    if (original_parent != owner) {
      added_child->DetachFromParent();
      added_child->SetParent(const_cast<AXObject*>(owner));
      if (original_parent) {
        ChildrenChanged(original_parent);
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
  }
}

void AXRelationCache::UpdateAriaOwnsFromAttrAssociatedElementsWithCleanLayout(
    AXObject* owner,
    const HeapVector<Member<Element>>& attr_associated_elements,
    HeapVector<Member<AXObject>>& validated_owned_children_result,
    bool force) {
  // attr-associated elements have already had their scope validated, but they
  // need to be further validated to determine if they introduce a cycle or are
  // already owned by another element.

  Vector<String> owned_id_vector;
  for (const auto& element : attr_associated_elements) {
    // Pass in owner parent assuming that the owns relationship will be valid.
    // It will be cleared below if the owns relationship is found to be invalid.
    AXObject* child = GetOrCreate(element, owner);

    // TODO(meredithl): Determine how to update reverse relations for elements
    // without an id.
    if (IsValidOwnsRelation(const_cast<AXObject*>(owner), child)) {
      if (element->GetIdAttribute())
        owned_id_vector.push_back(element->GetIdAttribute());
      validated_owned_children_result.push_back(child);
    } else if (child) {
      // Invalid owns relation: repair the parent that was set above.
      object_cache_->RestoreParentOrPrune(child);
      DCHECK_NE(child->CachedParentObject(), owner);
    }
  }

  // Track reverse relations for future tree updates.
  UpdateReverseRelations(id_attr_to_owns_relation_mapping_, owner->GetNode(),
                         owned_id_vector);

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
          << "\n* Owner = " << owner->ToString(true, true)
          << "\n* Child = " << child->ToString(true, true);
    }
  }
}

void AXRelationCache::UpdateAriaOwnsWithCleanLayout(AXObject* owner,
                                                    bool force) {
  DCHECK(owner);
  Element* element = owner->GetElement();
  if (!element)
    return;

  DCHECK(!element->GetDocument().NeedsLayoutTreeUpdateForNode(*element));

  // A refresh can occur even if not a valid owner, because the old object
  // that |owner| is replacing may have previously been a valid owner. In this
  // case, the old owned child mappings will need to be removed.
  bool is_valid_owner = IsValidOwner(owner);
  if (!force && !is_valid_owner)
    return;

  HeapVector<Member<AXObject>> owned_children;

  // We first check if the element has an explicitly set aria-owns association.
  // Explicitly set elements are validated when they are read (that they are in
  // a valid scope etc). The content attribute can contain ids that are not
  // legally ownable.
  if (!is_valid_owner) {
    DCHECK(force) << "Should not reach here except when an AXObject was "
                     "invalidated and is being refreshed: "
                  << owner->ToString(true, true);
  } else if (element && element->HasExplicitlySetAttrAssociatedElements(
                            html_names::kAriaOwnsAttr)) {
    UpdateAriaOwnsFromAttrAssociatedElementsWithCleanLayout(
        owner, *element->GetElementArrayAttribute(html_names::kAriaOwnsAttr),
        owned_children, force);
  } else {
    // Figure out the ids that actually correspond to children that exist
    // and that we can legally own (not cyclical, not already owned, etc.) and
    // update the maps and |validated_owned_children_result| based on that.
    //
    // Figure out the children that are owned by this object and are in the
    // tree.
    TreeScope& scope = element->GetTreeScope();
    Vector<String> owned_id_vector;
    owner->TokenVectorFromAttribute(element, owned_id_vector,
                                    html_names::kAriaOwnsAttr);
    // Track reverse relations for future tree updates.
    UpdateReverseRelations(id_attr_to_owns_relation_mapping_, element,
                           owned_id_vector);
    for (const String& id_name : owned_id_vector) {
      Element* child_element = scope.getElementById(AtomicString(id_name));
      // Pass in owner parent assuming that the owns relationship will be valid.
      // It will be cleared below if the owns relationship is found to be
      // invalid.
      AXObject* child = GetOrCreate(child_element, owner);
      if (IsValidOwnsRelation(const_cast<AXObject*>(owner), child)) {
        owned_children.push_back(child);
      } else if (child) {
        // Invalid owns relation: repair the parent that was set above.
        object_cache_->RestoreParentOrPrune(child);
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
    return;
  }

  // The list of owned children has changed. Even if they were just reordered,
  // to be safe and handle all cases we remove all of the current owned
  // children and add the new list of owned children.
  UnmapOwnedChildrenWithCleanLayout(owner, previously_owned_child_ids,
                                    validated_owned_child_axids);
  MapOwnedChildrenWithCleanLayout(owner, validated_owned_child_axids);

#if DCHECK_IS_ON()
  // Owned children must be in tree to avoid serialization issues.
  for (AXObject* child : validated_owned_children_result) {
    DCHECK(IsAriaOwned(child));
    DCHECK(child->ComputeAccessibilityIsIgnoredButIncludedInTree())
        << "Owned child not in tree: " << child->ToString(true, false)
        << "\nRecompute included in tree: "
        << child->ComputeAccessibilityIsIgnoredButIncludedInTree();
  }
#endif

  // Finally, update the mapping from the owner to the list of child IDs.
  if (validated_owned_child_axids.empty()) {
    aria_owner_to_children_mapping_.erase(owner->AXObjectID());
  } else {
    aria_owner_to_children_mapping_.Set(owner->AXObjectID(),
                                        validated_owned_child_axids);
  }

  ChildrenChanged(owner);
}

bool AXRelationCache::MayHaveHTMLLabelViaForAttribute(
    const HTMLElement& labelable) {
  const AtomicString& id = labelable.GetIdAttribute();
  if (id.empty())
    return false;
  return all_previously_seen_label_target_ids_.Contains(id);
}

// Fill source_objects with AXObjects for relations pointing to target.
void AXRelationCache::GetReverseRelated(
    Node* target,
    HashMap<String, HashSet<DOMNodeId>>& id_attr_to_node_map,
    HeapVector<Member<AXObject>>& source_objects) {
  auto* element = DynamicTo<Element>(target);
  if (!element)
    return;

  if (!element->HasID())
    return;

  auto it = id_attr_to_node_map.find(element->GetIdAttribute());
  if (it == id_attr_to_node_map.end()) {
    return;
  }

  for (DOMNodeId source_node : it->value) {
    AXObject* source_object = Get(DOMNodeIds::NodeForId(source_node));
    if (source_object)
      source_objects.push_back(source_object);
  }
}

void AXRelationCache::UpdateRelatedTree(Node* node, AXObject* obj) {
  // This can happen if MarkAXObjectDirtyWithCleanLayout is
  /// called and then UpdateRelatedTree is called on the same object,
  // e.g. in TextChangedWithCleanLayout.
  if (obj && obj->IsDetached()) {
    return;
  }
  HeapVector<Member<AXObject>> related_sources;
#if DCHECK_IS_ON()
  DCHECK(node);
  if (obj)
    DCHECK(!obj->IsDetached());
  AXObject* obj_for_node = object_cache_->SafeGet(node);
  DCHECK(!obj || obj_for_node == obj)
      << "Object and node did not match:"
      << "\n* node = " << node << "\n* obj = " << obj->ToString(true, true)
      << "\n* obj_for_node = "
      << (obj_for_node ? obj_for_node->ToString(true, true) : "null");
#endif
  AXObject* related_target = obj ? obj : Get(node);
  // Schedule an update on any previous owner.
  if (related_target && IsAriaOwned(related_target)) {
    AXObject* owned_parent = ValidatedAriaOwner(related_target);
    if (owned_parent)
      owner_ids_to_update_.insert(owned_parent->AXObjectID());
  }

  // Schedule an update on any potential new owner.
  GetReverseRelated(node, id_attr_to_owns_relation_mapping_, related_sources);
  for (AXObject* related : related_sources) {
    if (related) {
      owner_ids_to_update_.insert(related->AXObjectID());
      object_cache_->MarkAXObjectDirtyWithCleanLayout(related);
    }
  }

  UpdateRelatedText(node);

  UpdateRelatedActiveDescendant(node);
}

void AXRelationCache::UpdateRelatedText(Node* node) {
  // Walk up ancestor chain from node and refresh text of any related content.
  // TODO(crbug.com/1109265): It's very likely this loop should only walk the
  // unignored AXObject chain, but doing so breaks a number of tests related to
  // name or description computation / invalidation.
  int count = 0;
  constexpr int kMaxAncestorsForNameChangeCheck = 8;
  for (Node* current_node = node;
       ++count < kMaxAncestorsForNameChangeCheck && current_node &&
       !IsA<HTMLBodyElement>(current_node);
       current_node = current_node->parentNode()) {
    // Reverse relations via aria-labelledby, aria-describedby, aria-owns.
    HeapVector<Member<AXObject>> related_sources;
    GetReverseRelated(current_node, id_attr_to_text_relation_mapping_,
                      related_sources);
    for (AXObject* related : related_sources) {
      if (related && related->AccessibilityIsIncludedInTree() &&
          !related->NeedsToUpdateChildren()) {
        object_cache_->MarkAXObjectDirtyWithCleanLayout(related);
      }
    }

    // Ancestors that may derive their accessible name from descendant content
    // should also handle text changed events when descendant content changes.
    if (current_node != node) {
      AXObject* obj = Get(current_node);
      if (obj && obj->AccessibilityIsIncludedInTree() &&
          obj->SupportsNameFromContents(/*recursive=*/false) &&
          !obj->NeedsToUpdateChildren()) {
        object_cache_->MarkAXObjectDirtyWithCleanLayout(obj);
        break;  // Unlikely/unusual to need multiple name/description changes.
      }
    }

    // Forward relation via <label for="[id]">.
    if (IsA<HTMLLabelElement>(*current_node)) {
      LabelChanged(current_node);
      break;  // Unlikely/unusual to need multiple name/description changes.
    }
  }
}

void AXRelationCache::UpdateRelatedActiveDescendant(Node* node) {
  HeapVector<Member<AXObject>> related_sources;
  GetReverseRelated(node, id_attr_to_active_descendant_mapping_,
                    related_sources);
  for (AXObject* related : related_sources) {
    object_cache_->MarkAXObjectDirtyWithCleanLayout(related);
  }
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
    if (!object_cache_->HasBeenDisposed()) {
      for (const auto& child_axid : child_axids) {
        if (AXObject* owned_child = ObjectFromAXID(child_axid)) {
          owned_child->DetachFromParent();
          MaybeRestoreParentOfOwnedChild(owned_child);
        }
      }
    }
  }

  // Another id owned |obj_id|:
  RemoveOwnedRelation(obj_id);
}

void AXRelationCache::RemoveOwnedRelation(AXID obj_id) {
  // Another id owned |obj_id|.
  if (aria_owned_child_to_owner_mapping_.Contains(obj_id)) {
    // Previous owner no longer relevant to this child.
    // Also, remove |obj_id| from previous owner's owned child list:
    AXID owner_id = aria_owned_child_to_owner_mapping_.Take(obj_id);
    const Vector<AXID>& owners_owned_children =
        aria_owner_to_children_mapping_.at(owner_id);
    for (wtf_size_t index = 0; index < owners_owned_children.size(); index++) {
      if (owners_owned_children[index] == obj_id) {
        aria_owner_to_children_mapping_.at(owner_id).EraseAt(index);
        break;
      }
    }
    if (AXObject* owned_child = ObjectFromAXID(obj_id))
      owned_child->DetachFromParent();
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

void AXRelationCache::ChildrenChanged(AXObject* object) {
  object->ChildrenChangedWithCleanLayout();
}

void AXRelationCache::LabelChanged(Node* node) {
  const auto& id =
      To<HTMLElement>(node)->FastGetAttribute(html_names::kForAttr);
  if (!id.empty()) {
    all_previously_seen_label_target_ids_.insert(id);
    if (AXObject* obj = Get(To<HTMLLabelElement>(node)->control())) {
      if (obj->AccessibilityIsIncludedInTree())
        object_cache_->MarkAXObjectDirtyWithCleanLayout(obj);
    }
  }
}

void AXRelationCache::MaybeRestoreParentOfOwnedChild(AXObject* child) {
  DCHECK(child);
  if (child->IsDetached())
    return;
  if (AXObject* new_parent = object_cache_->RestoreParentOrPrune(child)) {
    object_cache_->ChildrenChanged(new_parent);
  }
}

void AXRelationCache::RegisterIncompleteRelation(
    AXObject* source,
    const QualifiedName& relation_attr) {
  DCHECK(source);
  Element* source_element = source->GetElement();
  if (!source_element) {
    return;
  }

  AtomicString relation_value = source_element->getAttribute(relation_attr);
  if (relation_value.IsNull()) {
    return;
  }
  String relation_value_as_string =
      relation_value.GetString().SimplifyWhiteSpace();
  Vector<String> tokens;
  relation_value_as_string.Split(' ', tokens);

  // Lookup each id within the same tree scope.
  for (auto id : tokens) {
    if (!source_element->GetTreeScope().getElementById(AtomicString(id))) {
      // Missing id: store source AXID so that it can be marked dirty once
      // the target node becomes available in the DOM.
      auto entry = incomplete_relations_.insert(id, Vector<AXID>());
      entry.stored_value->value.push_back(source->AXObjectID());
    }
  }
}

void AXRelationCache::RegisterIncompleteRelations(AXObject* source) {
  // When a new relation is discovered to have a target id that's missing from
  // the tree, record the incomplete relation so that when the id appears in the
  // tree, the source node can be reserialized with completed relation. Note:
  // aria-owns, aria-labelledy, aria-describedby affect more than just the
  // serialized relation property itself, and thus handled separately.
  DCHECK(source);
  const QualifiedName relation_attrs[] = {
      html_names::kAriaControlsAttr, html_names::kAriaDetailsAttr,
      html_names::kAriaErrormessageAttr, html_names::kAriaFlowtoAttr};

  for (const QualifiedName& relation_attr : relation_attrs) {
    RegisterIncompleteRelation(source, relation_attr);
  }
}

void AXRelationCache::ProcessCompletedRelationsForNewId(
    const AtomicString& id) {
  // When a new ID becomes available in the tree, we need to reserialize all
  // of the nodes that pointed to it with a relation attribute.
  auto iter = incomplete_relations_.find(id);
  if (iter == incomplete_relations_.end()) {
    return;
  }

  for (AXID source_axid : iter->value) {
    if (AXObject* obj = object_cache_->ObjectFromAXID(source_axid)) {
      object_cache_->MarkAXObjectDirtyWithCleanLayout(obj);
    }
  }

  incomplete_relations_.erase(iter);
}

}  // namespace blink
