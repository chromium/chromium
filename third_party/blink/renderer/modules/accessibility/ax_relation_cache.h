// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_RELATION_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_RELATION_CACHE_H_

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

// This class should only be used from inside the accessibility directory.
class AXRelationCache {
  USING_FAST_MALLOC(AXRelationCache);

 public:
  explicit AXRelationCache(AXObjectCacheImpl*);

  AXRelationCache(const AXRelationCache&) = delete;
  AXRelationCache& operator=(const AXRelationCache&) = delete;
  virtual ~AXRelationCache();

  void Init();

  //
  // Safe to call at any time. Doesn't make any changes to the tree.
  //

  // Returns true if the given object's position in the tree was due to
  // aria-owns.
  bool IsAriaOwned(AXID) const;
  bool IsAriaOwned(const AXObject*, bool check = true) const;

  // Returns the parent of the given object due to aria-owns, if valid,
  // otherwise, removes the child from maps indicating that it is owned.
  AXObject* ValidatedAriaOwner(const AXObject*);

  // Returns the validated owned children of this element with aria-owns.
  // Any children that are no longer valid are removed from maps indicating they
  // are owned.
  void ValidatedAriaOwnedChildren(const AXObject* owner,
                                  HeapVector<Member<AXObject>>& owned_children);

  // Return true if any label ever pointed to the element via the for attribute.
  bool MayHaveHTMLLabelViaForAttribute(const HTMLElement&);

  // True if any aria-describedy or aria-labelledby ever pointed to the element.
  bool IsARIALabelOrDescription(Element&);

  // Process an element in the DOM tree that was either just added or whose id
  // just changed:
  // * Check to see if another object wants to be its parent due to
  //   aria-owns. If so, add it to a queue of ids to process later during
  //   ProcessUpdatesWithCleanLayout.
  // * Update accessible objects for nodes that are related via
  //   label or description attributes.
  // |node| is not optional.
  // |obj| is optional. If provided, it must match the AXObject for |node|.
  // Returns AXObject* of owner if an aria-owns relation to |obj| exists.
  AXObject* GetOrCreateAriaOwnerFor(Node* node, AXObject* obj);
  // Update aria-owns as well as any name/description related to the node
  // and any aria-activedescendant relations. This is called for any type
  // of node change, even when text changes.
  void UpdateRelatedTree(Node* node, AXObject* obj);
  // Update relations that did or will link the element after its id changes.
  void UpdateRelatedTreeForIdChange(Element& element);

  void MarkOldAndNewRelationSourcesDirty(
      Element& element,
      HashMap<AtomicString, HashSet<DOMNodeId>>& id_attr_to_node_id_map);

  // Update the related maps with the css anchor that |positioned_node| is
  // anchored to. If there are multiple anchors, we assume the anchors are used
  // for layout purposes and do not update the map. One anchor is mapped to one
  // positioned element to reduce noise for AT.
  void UpdateCSSAnchorFor(Node* positioned_node);

  // Return the positioned object anchored to |anchor|.
  AXObject* GetPositionedObjectForAnchor(const AXObject* anchor);

  // Return the anchor for |positioned_obj|.
  AXObject* GetAnchorForPositionedObject(const AXObject* positioned_obj);

  // Remove given AXID from cache.
  void RemoveAXID(AXID);

  // The child cannot be owned, either because the child was removed or the
  // relation was invalid, so remove from all relevant mappings.
  void RemoveOwnedRelation(AXID);

  // Update map of ids to related objects.
  // If one or more ids aren't found, they're added to a lookup table so that if
  // an element with that id appears later, it can be added when you call
  // UpdateRelatedTree.
  void UpdateReverseRelations(
      HashMap<AtomicString, HashSet<DOMNodeId>>& id_attr_to_node_map,
      Node* relation_source,
      const Vector<AtomicString>& target_ids);

  void UpdateReverseTextRelations(Element& relation_source,
                                  const QualifiedName& attr_name);

  // Update map of ids to related objects for aria-labelledby/aria-describedby.
  void UpdateReverseTextRelations(Element& relation_source);
  void UpdateReverseTextRelations(Element& relation_source,
                                  const Vector<AtomicString>& target_ids);

  void UpdateReverseActiveDescendantRelations(Element& relation_source);
  void UpdateReverseOwnsRelations(Element& relation_source);
  void UpdateReverseOtherRelations(Element& relation_source);

  // Update a subtree used for a label so that it will be included in the tree,
  // even if hidden.
  void NotifySubtreeIsUsedForLabel(Element& labelling_subtree_root);

  // Process a new element and cache relations from its relevant attributes
  // using values of type IDREF/IDREFS.
  void CacheRelationIds(Element& element);

#if DCHECK_IS_ON()
  void CheckElementWasProcessed(Element& element);

  // Check that reverse relations were cached when the node was attached via
  // AXObjectCacheImpl::UpdateCacheAfterNodeIsAttached() or in
  // AXRelationCache::DoInitialDocumentScan().
  void CheckRelationsCached(Element& element);
#endif

  // Called when the "for" attribute of a label element changes and the
  // reverse mapping needs to be updated.
  // Returns the control, if any, pointed to by the label.
  Node* LabelChanged(HTMLLabelElement&);

  //
  // Only called when layout is clean, in the kInAccessibility lifecycle
  // stage.
  //

  // Called once towards the end of the kInAccessibility lifecycle stage.
  // Iterates over a list of ququed nodes that may require changes to their
  // set of owned children and calls UpdateAriaOwnsWithCleanLayout on each
  // of them.
  void ProcessUpdatesWithCleanLayout();

  // Determines the set of child nodes that this object owns due to aria-owns
  // (fully validating that the ownership is legal and free of cycles).
  // If that differs from the previously cached set of owned children,
  // calls ChildrenChanged on all affected nodes (old and new parents).
  // This affects the tree, which is why it should only be called at a
  // specific time in the lifecycle.
  // Pass |force=true| when the mappings must be updated even though the
  // owned ids have not changed, e.g. when an object has been refreshed.
  void UpdateAriaOwnsWithCleanLayout(AXObject* owner, bool force = false);

  // Is there work to be done when layout becomes clean?
  bool IsDirty() const;

  static bool IsValidOwner(AXObject* owner);
  static bool IsValidOwnedChild(Node& child);

#if EXPENSIVE_DCHECKS_ARE_ON()
  void ElementHasBeenProcessed(Element&);
#endif

 private:
  // Check that the element has been previouslly processed.
  void CheckElementWasProcessed(Element&) const;

  // Returns the parent of the given object due to aria-owns.
  AXObject* GetAriaOwnedParent(const AXObject*) const;

  // Given an object that has explicitly set elements for aria-owns, update the
  // internal state to reflect the new set of children owned by this object.
  // Note that |owned_children| will be the AXObjects corresponding to the
  // elements in |attr_associated_elements|. These elements are validated -
  // exist in the DOM, and are a descendant of a shadow including ancestor.
  void UpdateAriaOwnsFromAttrAssociatedElementsWithCleanLayout(
      AXObject* owner,
      const HeapVector<Member<Element>>& attr_associated_elements,
      HeapVector<Member<AXObject>>& owned_children,
      bool force);

  // If any object is related to this object via <label for>, aria-owns,
  // aria-describedby or aria-labeledby, update the text for the related object.
  void UpdateRelatedText(Node*);

  // Get ids that the element points to via aria-labelledby/describedby.
  Vector<AtomicString> GetTextRelationIds(Element& relation_source);
  // Get ids that the element points to via aria-controls, aria-details,
  // aria-flowto and aria-errormessage.
  Vector<AtomicString> GetOtherRelationIds(Element& relation_source);

  bool IsValidOwnsRelation(AXObject* owner, Node& child_node) const;
  void UnmapOwnedChildrenWithCleanLayout(const AXObject* owner,
                                         const Vector<AXID>& removed_child_ids,
                                         Vector<AXID>& unparented_child_ids);

  void MapOwnedChildrenWithCleanLayout(const AXObject* owner,
                                       const Vector<AXID>&);
  void GetReverseRelated(
      const AtomicString& target_id_attr,
      HashMap<AtomicString, HashSet<DOMNodeId>>& id_attr_to_node_map,
      HeapVector<Member<AXObject>>& sources);
  void MaybeRestoreParentOfOwnedChild(AXID removed_child_axid);

  // Updates |aria_owner_to_children_mapping_| after calling UpdateAriaOwns for
  // either the content attribute or the attr associated elements.
  void UpdateAriaOwnerToChildrenMappingWithCleanLayout(
      AXObject* owner,
      HeapVector<Member<AXObject>>& validated_owned_children_result,
      bool force);

  // Save the current id attribute for the given DOMNodeId.
  void UpdateRegisteredIdAttribute(Element& element, DOMNodeId node_id);

  WeakPersistent<AXObjectCacheImpl> object_cache_;

  // Map from the AXID of the owner to the AXIDs of the children.
  // This is a validated map, it doesn't contain illegal, duplicate,
  // or cyclical matches, or references to IDs that don't exist.
  HashMap<AXID, Vector<AXID>> aria_owner_to_children_mapping_;

  // Map from the AXID of a child to the AXID of the parent that owns it.
  HashMap<AXID, AXID> aria_owned_child_to_owner_mapping_;

  // Maps for AXID of CSS anchors and AXID of the related positioned item.
  HashMap<AXID, AXID> anchor_to_positioned_obj_mapping_;
  HashMap<AXID, AXID> positioned_obj_to_anchor_mapping_;

  // Reverse relation maps from an ID (the ID attribute of a DOM element) to the
  // set of elements that at some time pointed to that ID via a relation.
  // This is *unvalidated*, it includes possible extras and duplicates.
  // This is used so that:
  // - When an element with an ID is added to the tree or changes its ID, we can
  //   quickly determine if it affects the source node of a relationship.
  // - When text changes, we can recompute any label or description based on it
  //   and fire the appropriate change events.
  HashMap<AtomicString, HashSet<DOMNodeId>> id_attr_to_owns_relation_mapping_;
  HashMap<AtomicString, HashSet<DOMNodeId>> id_attr_to_text_relation_mapping_;
  HashMap<AtomicString, HashSet<DOMNodeId>>
      id_attr_to_active_descendant_mapping_;
  HashMap<AtomicString, HashSet<DOMNodeId>> id_attr_to_other_relation_mapping_;

  // HTML id attributes that at one time have had a <label for> pointing to it.
  // IDs are not necessarily removed from this set. It is not necessary to
  // remove IDs as false positives are ok. Being able to determine that a
  // labelable element has never had an associated label allows the accessible
  // name calculation to be optimized.
  HashSet<AtomicString> all_previously_seen_label_target_ids_;

  // Labels and descriptions set by ariaLabelledByElements,
  // ariaDescribedByElements as opposed to aria-labelledby.describedy="[id]".
  HashSet<DOMNodeId> explicitly_set_text_relations_from_element_attributes_;

  // A set of IDs that need to be update when layout is clean.
  // For each of these, the new set of owned children
  // will be computed, and if it's different than before, ChildrenChanged
  // will be fired on all affected nodes.
  HashSet<DOMNodeId> owner_ids_to_update_;

  // For each DOM node, the most recent id attribute value processed.
  HashMap<DOMNodeId, AtomicString> registered_id_attributes_;

  // Helpers that call back into object cache
  AXObject* ObjectFromAXID(AXID) const;
  AXObject* GetOrCreate(Node*, const AXObject* owner);
  AXObject* Get(Node*);
  void ChildrenChangedWithCleanLayout(AXObject*);

  // Do an initial scan of document to find any relations. We'll catch any
  // subsequent relations when nodes fare attached or attributes change.
  void DoInitialDocumentScan(Document&);

#if DCHECK_IS_ON()
  // A list of all elements that have had a chance to be processed for relations
  // before an AXObject has been created (it's important for crrev.com/c/4778093
  // to process relations first). An error here indicates that
  // UpdateCacheAfterNodeIsAttached() was never called for the element.
  HashSet<DOMNodeId> processed_elements_;

  // Avoid running relation checks until cache is initialized().
  bool is_initialized_ = false;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_RELATION_CACHE_H_
