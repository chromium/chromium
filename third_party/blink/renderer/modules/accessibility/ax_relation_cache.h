// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_RELATION_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_RELATION_CACHE_H_

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/modules_export.h"
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
  bool IsAriaOwned(const AXObject*) const;

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

  // Given an element in the DOM tree that was either just added or whose id
  // just changed, check to see if another object wants to be its parent due to
  // aria-owns. If so, add it to a queue of ids to process later during
  // ProcessUpdatesWithCleanLayout.
  // |node| is not optional.
  // |obj| is optional. If provided, it must match the AXObject for |node|.
  void UpdateRelatedTree(Node* node, AXObject* obj);

  void UpdateRelatedActiveDescendant(Node* node);

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
      HashMap<String, HashSet<DOMNodeId>>& id_attr_to_node_map,
      Node* relation_source,
      const Vector<String>& target_ids);

  void UpdateReverseTextRelations(Element& relation_source,
                                  const QualifiedName& attr_name);

  // Update map of ids to related objects for aria-labelledby/aria-describedby.
  void UpdateReverseTextRelations(Element& relation_source);
  void UpdateReverseTextRelations(Element& relation_source,
                                  const Vector<String>& target_ids);

  void UpdateReverseActiveDescendantRelations(Element& relation_source);
  void UpdateReverseOwnsRelations(Element& relation_source);

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

  // -- Incomplete relation handling --
  // An incomplete relation is one where the target id is not yet connected in
  // the DOM. Call this when there is a new object that may have incomplete
  // relations.
  void RegisterIncompleteRelations(AXObject*);
  // Call this when there is an object with a new value for the provided
  // relation |attr|, so that if it is incomplete it will be registered.
  void RegisterIncompleteRelation(AXObject*, const QualifiedName& attr);
  // When a new id becomes available, call this so that any source relations
  // that point to it are marked dirty.
  void ProcessCompletedRelationsForNewId(const AtomicString& id);

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
  static bool IsValidOwnedChild(AXObject* child);

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
  Vector<String> GetTextRelationIds(Element& relation_source);

  bool IsValidOwnsRelation(AXObject* owner, AXObject* child) const;
  void UnmapOwnedChildrenWithCleanLayout(const AXObject* owner,
                                         const Vector<AXID>& removed_child_ids,
                                         const Vector<AXID>& newly_owned_ids);

  void MapOwnedChildrenWithCleanLayout(const AXObject* owner,
                                       const Vector<AXID>&);
  void GetReverseRelated(
      Node*,
      HashMap<String, HashSet<DOMNodeId>>& id_attr_to_node_map,
      HeapVector<Member<AXObject>>& sources);
  void MaybeRestoreParentOfOwnedChild(AXObject* removed_child);

  // Updates |aria_owner_to_children_mapping_| after calling UpdateAriaOwns for
  // either the content attribute or the attr associated elements.
  void UpdateAriaOwnerToChildrenMappingWithCleanLayout(
      AXObject* owner,
      HeapVector<Member<AXObject>>& validated_owned_children_result,
      bool force);

  WeakPersistent<AXObjectCacheImpl> object_cache_;

  // Map from the AXID of the owner to the AXIDs of the children.
  // This is a validated map, it doesn't contain illegal, duplicate,
  // or cyclical matches, or references to IDs that don't exist.
  HashMap<AXID, Vector<AXID>> aria_owner_to_children_mapping_;

  // Map from the AXID of a child to the AXID of the parent that owns it.
  HashMap<AXID, AXID> aria_owned_child_to_owner_mapping_;

  // Reverse relation maps from an ID (the ID attribute of a DOM element) to the
  // set of elements that at some time pointed to that ID via aria-owns,
  // aria-labelledby or aria-desribedby. This is *unvalidated*, it includes
  // possible extras and duplicates.
  // This is used so that:
  // - When an element with an ID is added to the tree or changes its ID, we can
  //   quickly determine if it affects an aria-owns relationship.
  // - When text changes, we can recompute any label or description based on it
  //   and fire the appropriate change events.
  HashMap<String, HashSet<DOMNodeId>> id_attr_to_owns_relation_mapping_;
  HashMap<String, HashSet<DOMNodeId>> id_attr_to_text_relation_mapping_;
  HashMap<String, HashSet<DOMNodeId>> id_attr_to_active_descendant_mapping_;

  // HTML id attributes that at one time have had a <label for> pointing to it.
  // IDs are not necessarily removed from this set. It is not necessary to
  // remove IDs as false positives are ok. Being able to determine that a
  // labelable element has never had an associated label allows the accessible
  // name calculation to be optimized.
  HashSet<AtomicString> all_previously_seen_label_target_ids_;

  // A set of IDs that need to be updated during the kInAccessibility
  // lifecycle phase. For each of these, the new set of owned children
  // will be computed, and if it's different than before, ChildrenChanged
  // will be fired on all affected nodes.
  HashSet<AXID> owner_ids_to_update_;

  // A map from a source AXObject to the id attribute values for relations
  // targets that are not yet in the DOM tree.
  HashMap<String, Vector<AXID>> incomplete_relations_;

  // Helpers that call back into object cache
  AXObject* ObjectFromAXID(AXID) const;
  AXObject* GetOrCreate(Node*, const AXObject* owner);
  AXObject* Get(Node*);
  void ChildrenChanged(AXObject*);

  // Do an initial scan of document to find any relations. We'll catch any
  // subsequent relations when nodes fare attached or attributes change.
  void DoInitialDocumentScan(Document&);

#if DCHECK_IS_ON()
  // A list of all elements that have had a chance to be processed for relations
  // before an AXObject has been created (it's important for crrev.com/c/4778093
  // to process relations first). An error here indicates that
  // UpdateCacheAfterNodeIsAttached() was never called for the element.
  HashSet<DOMNodeId> processed_elements_;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_RELATION_CACHE_H_
