/*
 * Copyright (C) 2014, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_OBJECT_CACHE_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_OBJECT_CACHE_IMPL_H_

#include <memory>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache_base.h"
#include "third_party/blink/renderer/core/accessibility/blink_ax_event_intent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/accessibility/ax_enums.mojom-blink-forward.h"

namespace blink {

class AbstractInlineTextBox;
class AXRelationCache;
class HTMLAreaElement;
class LocalFrameView;

// This class should only be used from inside the accessibility directory.
class MODULES_EXPORT AXObjectCacheImpl
    : public AXObjectCacheBase,
      public mojom::blink::PermissionObserver {
 public:
  static AXObjectCache* Create(Document&);

  explicit AXObjectCacheImpl(Document&);
  ~AXObjectCacheImpl() override;
  void Trace(Visitor*) const override;

  Document& GetDocument() { return *document_; }
  AXObject* FocusedObject();

  void Dispose() override;

  // Register/remove popups
  void InitializePopup(Document* document) override;
  void DisposePopup(Document* document) override;

  //
  // Iterators.
  //

  AXObject::InOrderTraversalIterator InOrderTraversalBegin();
  AXObject::InOrderTraversalIterator InOrderTraversalEnd();

  void SelectionChanged(Node*) override;
  void UpdateReverseRelations(const AXObject* relation_source,
                              const Vector<String>& target_ids);
  void ChildrenChanged(Node*) override;
  void ChildrenChanged(const LayoutObject*) override;
  void ChildrenChanged(AccessibleNode*) override;
  void CheckedStateChanged(Node*) override;
  void ListboxOptionStateChanged(HTMLOptionElement*) override;
  void ListboxSelectedChildrenChanged(HTMLSelectElement*) override;
  void ListboxActiveIndexChanged(HTMLSelectElement*) override;
  void LocationChanged(const LayoutObject*) override;
  void RadiobuttonRemovedFromGroup(HTMLInputElement*) override;
  void ImageLoaded(const LayoutObject*) override;

  void Remove(AccessibleNode*) override;
  void Remove(LayoutObject*) override;
  void Remove(Node*) override;
  void Remove(AbstractInlineTextBox*) override;

  const Element* RootAXEditableElement(const Node*) override;

  // Called when aspects of the style (e.g. color, alignment) change.
  void StyleChanged(const LayoutObject*) override;

  // Called by a node when text or a text equivalent (e.g. alt) attribute is
  // changed.
  void TextChanged(const LayoutObject*) override;
  void TextChangedWithCleanLayout(Node* optional_node, AXObject*);
  void FocusableChangedWithCleanLayout(Element* element);
  void DocumentTitleChanged() override;
  // Called when a node has just been attached, so we can make sure we have the
  // right subclass of AXObject.
  void UpdateCacheAfterNodeIsAttached(Node*) override;
  void DidInsertChildrenOfNode(Node*) override;

  void HandleAttributeChanged(const QualifiedName& attr_name,
                              Element*) override;
  void HandleValidationMessageVisibilityChanged(
      const Element* form_control) override;
  void HandleFocusedUIElementChanged(Element* old_focused_element,
                                     Element* new_focused_element) override;
  void HandleInitialFocus() override;
  void HandleTextFormControlChanged(Node*) override;
  void HandleEditableTextContentChanged(Node*) override;
  void HandleScaleAndLocationChanged(Document*) override;
  void HandleTextMarkerDataAdded(Node* start, Node* end) override;
  void HandleValueChanged(Node*) override;
  void HandleUpdateActiveMenuOption(LayoutObject*, int option_index) override;
  void DidShowMenuListPopup(LayoutObject*) override;
  void DidHideMenuListPopup(LayoutObject*) override;
  void HandleLoadComplete(Document*) override;
  void HandleLayoutComplete(Document*) override;
  void HandleClicked(Node*) override;
  void HandleAttributeChanged(const QualifiedName& attr_name,
                              AccessibleNode*) override;

  void SetCanvasObjectBounds(HTMLCanvasElement*,
                             Element*,
                             const LayoutRect&) override;

  void InlineTextBoxesUpdated(LineLayoutItem) override;
  void ProcessDeferredAccessibilityEvents(Document&) override;

  // Called when a HTMLFrameOwnerElement (such as an iframe element) changes the
  // embedding token of its child frame.
  void EmbeddingTokenChanged(HTMLFrameOwnerElement*) override;

  // Called when the scroll offset changes.
  void HandleScrollPositionChanged(LocalFrameView*) override;
  void HandleScrollPositionChanged(LayoutObject*) override;

  void HandleScrolledToAnchor(const Node* anchor_node) override;

  // Called when the frame rect changes, which can sometimes happen
  // without producing any layout or other notifications.
  void HandleFrameRectsChanged(Document&) override;

  // Invalidates the bounding box, which can be later retrieved by
  // GetAllObjectsWithChangedBounds.
  void InvalidateBoundingBox(const LayoutObject*) override;

  const AtomicString& ComputedRoleForNode(Node*) override;
  String ComputedNameForNode(Node*) override;

  void OnTouchAccessibilityHover(const IntPoint&) override;

  AXObject* ObjectFromAXID(AXID id) const { return objects_.at(id); }
  AXObject* Root();

  // Used for objects without backing DOM nodes, layout objects, etc.
  AXObject* Create(ax::mojom::blink::Role, AXObject* parent);

  AXObject* GetOrCreate(AccessibleNode*);
  AXObject* GetOrCreate(LayoutObject*) override;
  AXObject* GetOrCreate(const Node*);
  AXObject* GetOrCreate(Node*);
  AXObject* GetOrCreate(AbstractInlineTextBox*);

  AXID GetAXID(Node*) override;
  Element* GetElementFromAXID(AXID) override;

  // will only return the AXObject if it already exists
  AXObject* Get(AccessibleNode*);
  AXObject* Get(const Node*) override;
  AXObject* Get(const LayoutObject*);
  AXObject* Get(AbstractInlineTextBox*);

  AXObject* FirstAccessibleObjectFromNode(const Node*);

  void Remove(AXID);

  void ChildrenChangedWithCleanLayout(Node* optional_node_for_relation_update,
                                      AXObject*);

  void MaybeNewRelationTarget(Node* node, AXObject* obj);

  void HandleActiveDescendantChangedWithCleanLayout(Node*);
  void HandleRoleChangeWithCleanLayout(Node*);
  void HandleRoleChangeIfNotEditableWithCleanLayout(Node*);
  void HandleAriaExpandedChangeWithCleanLayout(Node*);
  void HandleAriaSelectedChangedWithCleanLayout(Node*);
  void HandleNodeLostFocusWithCleanLayout(Node*);
  void HandleNodeGainedFocusWithCleanLayout(Node*);

  bool InlineTextBoxAccessibilityEnabled();

  void RemoveAXID(AXObject*);

  AXID GenerateAXID() const;

  // Counts the number of times the document has been modified. Some attribute
  // values are cached as long as the modification count hasn't changed.
  int ModificationCount() const { return modification_count_; }

  void PostNotification(const LayoutObject*, ax::mojom::blink::Event);
  void PostNotification(Node*, ax::mojom::Event);
  void PostNotification(AXObject*, ax::mojom::blink::Event);
  void MarkAXObjectDirty(AXObject*, bool subtree);
  void MarkElementDirty(const Element*, bool subtree);

  //
  // Aria-owns support.
  //

  // Returns true if the given object's position in the tree was due to
  // aria-owns.
  bool IsAriaOwned(const AXObject*) const;

  // Returns the parent of the given object due to aria-owns.
  AXObject* GetAriaOwnedParent(const AXObject*) const;

  // Given an object that has an aria-owns attributes, and a vector of ids from
  // the value of that attribute, updates the internal state to reflect the new
  // set of children owned by this object, returning the result in
  // |ownedChildren|. The result is validated - illegal, duplicate, or cyclical
  // references have been removed.
  //
  // If one or more ids aren't found, they're added to a lookup table so that if
  // an element with that id appears later, it can be added when you call
  // updateTreeIfElementIdIsAriaOwned.
  void UpdateAriaOwns(const AXObject* owner,
                      const Vector<String>& id_vector,
                      HeapVector<Member<AXObject>>& owned_children);

  // Given an object that has explicitly set elements for aria-owns, update the
  // internal state to reflect the new set of children owned by this object.
  // Note that |owned_children| will be the AXObjects corresponding to the
  // elements in |attr_associated_elements|. These elements are validated -
  // exist in the DOM, and are a descendant of a shadow including ancestor.
  void UpdateAriaOwnsFromAttrAssociatedElements(
      const AXObject* owner,
      const HeapVector<Member<Element>>& attr_associated_elements,
      HeapVector<Member<AXObject>>& owned_children);

  // Adds |object| to |fixed_or_sticky_node_ids_| if it has a fixed or sticky
  // position.
  void AddToFixedOrStickyNodeList(const AXObject* object);

  bool MayHaveHTMLLabel(const HTMLElement& elem);

  // Synchronously returns whether or not we currently have permission to
  // call AOM event listeners.
  bool CanCallAOMEventListeners() const;

  // This is called when an accessibility event is triggered and there are
  // AOM event listeners registered that would have been called.
  // Asynchronously requests permission from the user. If permission is
  // granted, it only applies to the next event received.
  void RequestAOMEventListenerPermission();

  // For built-in HTML form validation messages.
  AXObject* ValidationMessageObjectIfInvalid();

  WebAXAutofillState GetAutofillState(AXID id) const;
  void SetAutofillState(AXID id, WebAXAutofillState state);

  ax::mojom::blink::EventFrom active_event_from() const {
    return active_event_from_;
  }
  void set_active_event_from(const ax::mojom::blink::EventFrom event_from) {
    active_event_from_ = event_from;
  }

  AXObject* GetActiveAriaModalDialog() const;

  bool UseAXMenuList() { return use_ax_menu_list_; }

  // Retrieves a vector of all AXObjects whose bounding boxes may have changed
  // since the last query. Clears the vector so that the next time it's
  // called, it will only retrieve objects that have changed since now.
  HeapVector<Member<AXObject>> GetAllObjectsWithChangedBounds();

 protected:
  void PostPlatformNotification(
      AXObject* obj,
      ax::mojom::blink::Event event_type,
      ax::mojom::blink::EventFrom event_from =
          ax::mojom::blink::EventFrom::kNone,
      const BlinkAXEventIntentsSet& event_intents = BlinkAXEventIntentsSet());
  void LabelChangedWithCleanLayout(Element*);

  // Returns a reference to the set of currently active event intents.
  BlinkAXEventIntentsSet& ActiveEventIntents() override {
    return active_event_intents_;
  }

  AXObject* CreateFromRenderer(LayoutObject*);
  AXObject* CreateFromNode(Node*);
  AXObject* CreateFromInlineTextBox(AbstractInlineTextBox*);

 private:
  struct AXEventParams final : public GarbageCollected<AXEventParams> {
    AXEventParams(AXObject* target,
                  ax::mojom::blink::Event event_type,
                  ax::mojom::blink::EventFrom event_from,
                  const BlinkAXEventIntentsSet& intents)
        : target(target), event_type(event_type), event_from(event_from) {
      for (const auto& intent : intents) {
        event_intents.insert(intent.key, intent.value);
      }
    }
    Member<AXObject> target;
    ax::mojom::blink::Event event_type;
    ax::mojom::blink::EventFrom event_from;
    BlinkAXEventIntentsSet event_intents;

    void Trace(Visitor* visitor) const { visitor->Trace(target); }
  };

  struct TreeUpdateParams final : public GarbageCollected<TreeUpdateParams> {
    TreeUpdateParams(Node* node,
                     AXID axid,
                     ax::mojom::blink::EventFrom event_from,
                     const BlinkAXEventIntentsSet& intents,
                     base::OnceClosure callback)
        : node(node),
          axid(axid),
          event_from(event_from),
          callback(std::move(callback)) {
      for (const auto& intent : intents) {
        event_intents.insert(intent.key, intent.value);
      }
    }
    WeakMember<Node> node;
    AXID axid;
    ax::mojom::blink::EventFrom event_from;
    BlinkAXEventIntentsSet event_intents;
    base::OnceClosure callback;

    void Trace(Visitor* visitor) const { visitor->Trace(node); }
  };

  ax::mojom::blink::EventFrom ComputeEventFrom();

  Member<Document> document_;
  HeapHashMap<AXID, Member<AXObject>> objects_;
  // LayoutObject and AbstractInlineTextBox are not on the Oilpan heap so we
  // do not use HeapHashMap for those mappings.
  HeapHashMap<Member<AccessibleNode>, AXID> accessible_node_mapping_;
  HashMap<const LayoutObject*, AXID> layout_object_mapping_;
  HeapHashMap<Member<const Node>, AXID> node_object_mapping_;
  HashMap<AbstractInlineTextBox*, AXID> inline_text_box_object_mapping_;
  int modification_count_;

  HashSet<AXID> ids_in_use_;

  // Used for a mock AXObject representing the message displayed in the
  // validation message bubble.
  // There can be only one of these per document with invalid form controls,
  // and it will always be related to the currently focused control.
  AXID validation_message_axid_;

  // The currently active aria-modal dialog element, if one has been computed,
  // null if otherwise. This is only ever computed on platforms that have the
  // AriaModalPrunesAXTree setting enabled, such as Mac.
  WeakMember<AXObject> active_aria_modal_dialog_;

  std::unique_ptr<AXRelationCache> relation_cache_;

#if DCHECK_IS_ON()
  // Verified when finalizing.
  bool has_been_disposed_ = false;
#endif

  HeapVector<Member<AXEventParams>> notifications_to_post_;

  void ProcessUpdates(Document&);
  void PostNotifications(Document&);

  // Get the currently focused Node element.
  Node* FocusedElement();

  // GetOrCreate the focusable AXObject for a specific Node.
  AXObject* GetOrCreateFocusedObjectFromNode(Node*);

  AXObject* FocusedImageMapUIElement(HTMLAreaElement*);

  AXID GetOrCreateAXID(AXObject*);

  void TextChanged(Node*);
  bool NodeIsTextControl(const Node*);
  AXObject* NearestExistingAncestor(Node*);

  Settings* GetSettings();

  // Start listenening for updates to the AOM accessibility event permission.
  void AddPermissionStatusListener();

  // mojom::blink::PermissionObserver implementation.
  // Called when we get an updated AOM event listener permission value from
  // the browser.
  void OnPermissionStatusChange(mojom::PermissionStatus) override;

  // When a <tr> or <td> is inserted or removed, the containing table may have
  // gained or lost rows or columns.
  void ContainingTableRowsOrColsMaybeChanged(Node*);

  // Must be called an entire subtree of accessible objects are no longer valid.
  void InvalidateTableSubtree(AXObject* subtree);

  // Object for HTML validation alerts. Created at most once per object cache.
  AXObject* GetOrCreateValidationMessageObject();
  void RemoveValidationMessageObject();

  // Enqueue a callback to the given method to be run after layout is
  // complete.
  void DeferTreeUpdate(void (AXObjectCacheImpl::*method)(Node*), Node* node);
  void DeferTreeUpdate(void (AXObjectCacheImpl::*method)(const QualifiedName&,
                                                         Element* element),
                       const QualifiedName& attr_name,
                       Element* element);
  // Provide either a DOM node or AXObject. If both are provided, then they must
  // match, meaning that the AXObject's DOM node must equal the provided node.
  void DeferTreeUpdate(void (AXObjectCacheImpl::*method)(Node*, AXObject*),
                       Node* node,
                       AXObject* obj);

  void DeferTreeUpdateInternal(base::OnceClosure callback, Node* node);

  void DeferTreeUpdateInternal(base::OnceClosure callback, AXObject* obj);

  void SelectionChangedWithCleanLayout(Node* node);
  void TextChangedWithCleanLayout(Node* node);
  void ChildrenChangedWithCleanLayout(Node* node);
  void HandleAttributeChangedWithCleanLayout(const QualifiedName& attr_name,
                                             Element* element);

  //
  // aria-modal support
  //

  // This function is only ever called on platforms where the
  // AriaModalPrunesAXTree setting is enabled, and the accessibility tree must
  // be manually pruned to remove background content.
  void UpdateActiveAriaModalDialog(Node* element);

  // This will return null on platforms without the AriaModalPrunesAXTree
  // setting enabled, or where there is no active ancestral aria-modal dialog.
  AXObject* AncestorAriaModalDialog(Node* node);

  void ScheduleVisualUpdate();
  void FireTreeUpdatedEventImmediately(
      Document& document,
      ax::mojom::blink::EventFrom event_from,
      const BlinkAXEventIntentsSet& event_intents,
      base::OnceClosure callback);
  void FireAXEventImmediately(AXObject* obj,
                              ax::mojom::blink::Event event_type,
                              ax::mojom::blink::EventFrom event_from,
                              const BlinkAXEventIntentsSet& event_intents);

  void SetMaxPendingUpdatesForTesting(wtf_size_t max_pending_updates) {
    max_pending_updates_ = max_pending_updates;
  }

  void UpdateNumTreeUpdatesQueuedBeforeLayoutHistogram();

  // Invalidates the bounding boxes of fixed or sticky positioned objects which
  // should be updated when the scroll offset is changed. Like
  // InvalidateBoundingBox, it can be later retrieved by
  // GetAllObjectsWithChangedBounds.
  void InvalidateBoundingBoxForFixedOrStickyPosition();

  // Whether the user has granted permission for the user to install event
  // listeners for accessibility events using the AOM.
  mojom::PermissionStatus accessibility_event_permission_;
  // The permission service, enabling us to check for event listener
  // permission.
  HeapMojoRemote<mojom::blink::PermissionService> permission_service_;
  HeapMojoReceiver<mojom::blink::PermissionObserver, AXObjectCacheImpl>
      permission_observer_receiver_;

  // The main document, plus any page popups.
  HeapHashSet<WeakMember<Document>> documents_;

  // Queued callbacks.
  typedef HeapVector<Member<TreeUpdateParams>> TreeUpdateCallbackQueue;
  TreeUpdateCallbackQueue tree_update_callback_queue_;
  HeapHashSet<WeakMember<Node>> nodes_with_pending_children_changed_;

  // If tree_update_callback_queue_ gets improbably large, stop
  // enqueueing updates and fire a single ChildrenChanged event on the
  // document once layout occurs.
  wtf_size_t max_pending_updates_ = 1UL << 16;
  bool tree_updates_paused_ = false;

  // Maps ids to their object's autofill state.
  HashMap<AXID, WebAXAutofillState> autofill_state_map_;

  // The set of node IDs whose bounds has changed since the last time
  // GetAllObjectsWithChangedBounds was called.
  HashSet<AXID> changed_bounds_ids_;

  // The list of node IDs whose position is fixed or sticky.
  HashSet<AXID> fixed_or_sticky_node_ids_;

  // The source of the event that is currently being handled.
  ax::mojom::blink::EventFrom active_event_from_ =
      ax::mojom::blink::EventFrom::kNone;

  // A set of currently active event intents.
  BlinkAXEventIntentsSet active_event_intents_;

  // If false, exposes the internal accessibility tree of a select pop-up
  // instead.
  bool use_ax_menu_list_ = true;

  DISALLOW_COPY_AND_ASSIGN(AXObjectCacheImpl);

  FRIEND_TEST_ALL_PREFIXES(AccessibilityTest, PauseUpdatesAfterMaxNumberQueued);
};

// This is the only subclass of AXObjectCache.
template <>
struct DowncastTraits<AXObjectCacheImpl> {
  static bool AllowFrom(const AXObjectCache& cache) { return true; }
};

// This will let you know if aria-hidden was explicitly set to false.
bool IsNodeAriaVisible(Node*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_OBJECT_CACHE_IMPL_H_
