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

#include "base/dcheck_is_on.h"
#include "base/gtest_prod_util.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink.h"
#include "third_party/blink/public/mojom/render_accessibility.mojom-blink.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_aria_notification_options.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache_base.h"
#include "third_party/blink/renderer/core/accessibility/blink_ax_event_intent.h"
#include "third_party/blink/renderer/core/editing/commands/selection_for_undo_step.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/modules/accessibility/aria_notification.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/blink_ax_tree_source.h"
#include "third_party/blink/renderer/modules/accessibility/inspector_accessibility_agent.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/accessibility/ax_enums.mojom-blink-forward.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_tree_serializer.h"

namespace blink {

class AXRelationCache;
class HTMLAreaElement;
class LocalFrameView;
class NGAbstractInlineTextBox;
class WebLocalFrameClient;

// Describes a decicion on whether to create an AXNodeObject, an AXLayoutObject,
// or nothing (which will cause the AX subtree to be pruned at that point).
// Currently this also mirrors the decision on whether to back the object by a
// node or a layout object. When the AXObject is backed by a node, it's
// AXID can be looked up in node_object_mapping_, and when the AXObject is
// backed by layout, it's AXID can be looked up in layout_object_mapping_.
// TODO(accessibility) Split the decision of what to use for backing from what
// type of object to create, and use a node whenever possible, in order to
// enable more stable IDs for most objects.
enum AXObjectType { kPruneSubtree = 0, kAXNodeObject, kAXLayoutObject };

struct TextChangedOperation {
  TextChangedOperation()
      : start(0),
        end(0),
        start_anchor_id(0),
        end_anchor_id(0),
        op(ax::mojom::blink::Command::kNone) {}
  TextChangedOperation(int start_in,
                       int end_in,
                       AXID start_id_in,
                       AXID end_id_in,
                       ax::mojom::blink::Command op_in)
      : start(start_in),
        end(end_in),
        start_anchor_id(start_id_in),
        end_anchor_id(end_id_in),
        op(op_in) {}
  int start;
  int end;
  AXID start_anchor_id;
  AXID end_anchor_id;
  ax::mojom::blink::Command op;
};

// This class should only be used from inside the accessibility directory.
class MODULES_EXPORT AXObjectCacheImpl
    : public AXObjectCacheBase,
      public mojom::blink::PermissionObserver {
 public:
  static AXObjectCache* Create(Document&, const ui::AXMode&);

  AXObjectCacheImpl(Document&, const ui::AXMode&);

  AXObjectCacheImpl(const AXObjectCacheImpl&) = delete;
  AXObjectCacheImpl& operator=(const AXObjectCacheImpl&) = delete;

  ~AXObjectCacheImpl() override;
  void Trace(Visitor*) const override;

  // The main document.
  Document& GetDocument() const { return *document_; }
  // The popup document, if showing, otherwise null.
  Document* GetPopupDocumentIfShowing() const { return popup_document_; }

  AXObject* FocusedObject();

  const ui::AXMode& GetAXMode() override;
  void SetAXMode(const ui::AXMode&) override;

  // When the accessibility tree view is open in DevTools, we listen for changes
  // to the tree by registering an InspectorAccessibilityAgent here and notify
  // the agent when AXEvents are fired or nodes are marked dirty.
  void AddInspectorAgent(InspectorAccessibilityAgent*);
  void RemoveInspectorAgent(InspectorAccessibilityAgent*);

  // Ensure that a full document lifecycle will occur, which in turn ensures
  // that a call to ProcessDeferredAccessibilityEvents() will occur soon.
  void ScheduleAXUpdate() const override;

  void Dispose() override;

  // Freeze that AXObject tree and do not allow changes until Thaw() is called.
  // Prefer ScopedFreezeAXCache where possible.
  void Freeze() override {
    // TODO(crbug.com/1477047, fuchsia:132924): Remove Fuchsia case once post
    // lifecycle serialization is enabled for it.
#if BUILDFLAG(IS_FUCHSIA)
    if (GetDocument().Lifecycle().GetState() <
        DocumentLifecycle::kPrePaintClean) {
      pause_tree_updates_until_more_loaded_content_ = false;
      UpdateAXForAllDocuments();
    }
#endif

    ax_tree_source_->Freeze();
    is_frozen_ = true;
  }
  void Thaw() override {
    is_frozen_ = false;
    ax_tree_source_->Thaw();
  }
  bool IsFrozen() const override { return is_frozen_; }

  //
  // Iterators.
  //

  void SelectionChanged(Node*) override;

  // Effects a ChildrenChanged() on the passed-in object, if unignored,
  // otherwise, uses the first unignored ancestor. Returns the object that the
  // children changed occurs on.
  AXObject* ChildrenChanged(AXObject*);
  void ChildrenChangedWithCleanLayout(AXObject*);
  void ChildrenChanged(Node*) override;
  void ChildrenChanged(AccessibleNode*) override;
  void ChildrenChanged(const LayoutObject*) override;
  void SlotAssignmentWillChange(Node*) override;
  void CheckedStateChanged(Node*) override;
  void ListboxOptionStateChanged(HTMLOptionElement*) override;
  void ListboxSelectedChildrenChanged(HTMLSelectElement*) override;
  void ListboxActiveIndexChanged(HTMLSelectElement*) override;
  void SetMenuListOptionsBounds(HTMLSelectElement*,
                                const WTF::Vector<gfx::Rect>&) override;
  void LocationChanged(const LayoutObject*) override;
  void ImageLoaded(const LayoutObject*) override;

  // Removes AXObject backed by passed-in object, if there is one.
  // It will also notify the parent that its children have changed, so that the
  // parent will recompute its children and be reserialized.
  void Remove(AccessibleNode*) override;
  void Remove(LayoutObject*) override;
  void Remove(Node*) override;
  void RemovePopup(Document*) override;
  void Remove(NGAbstractInlineTextBox*) override;
  // Remove an AXObject or its subtree, and if |notify_parent| is true,
  // recompute the parent's children and reserialize the parent.
  void Remove(AXObject*, bool notify_parent);
  void Remove(Node*, bool notify_parent);

  // This will remove all AXObjects in the subtree, whether they or not they are
  // marked as included for serialization. This can only be called while flat
  // tree traversal is safe and there are no slot assignments pending.
  // To remove only included nodes, use RemoveIncludedSubtree(), which can be
  // called at any time.
  // If |remove_root|, remove the root of the subtree, otherwise only
  // descendants are removed. If |notify_parent|, call ChildrenChanged() on the
  // parent.
  void RemoveSubtreeWithFlatTraversal(const Node*,
                                      bool remove_root = true,
                                      bool notify_parent = true);
  void RemoveSubtreeWhenSafe(Node*, bool remove_root = true) override;

  // Remove the cached subtree of included AXObjects. If |remove_root| is false,
  // then only descendants will be removed. To remove unincluded AXObjects as
  // well, call RemoveSubtreeWithFlatTraversal() or RemoveSubtreeWhenSafe().
  // If |remove_root|, remove the root of the subtree, otherwise only
  // descendants are removed.
  void RemoveIncludedSubtree(AXObject* object, bool remove_root);

  // For any ancestor that could contain the passed-in AXObject* in their cached
  // children, clear their children and set needs to update children on them.
  // In addition, ChildrenChanged() on an included ancestor that might contain
  // this child, if one exists.
  void ChildrenChangedOnAncestorOf(AXObject*);

  const Element* RootAXEditableElement(const Node*) override;

  // Called when aspects of the style (e.g. color, alignment) change.
  void StyleChanged(const LayoutObject*,
                    bool visibility_or_inertness_changed) override;

  // Called by a node when text or a text equivalent (e.g. alt) attribute is
  // changed.
  void TextChanged(const LayoutObject*) override;
  void TextChangedWithCleanLayout(Node* optional_node, AXObject*);

  void FocusableChangedWithCleanLayout(Node* node);
  void DocumentTitleChanged() override;
  // Called when a node is connected to the document.
  void NodeIsConnected(Node*) override;
  // Called when a node is attached to the layout tree.
  void NodeIsAttached(Node*) override;

  void HandleAttributeChanged(const QualifiedName& attr_name,
                              Element*) override;
  void FinishedParsingTable(HTMLTableElement*) override;
  void HandleValidationMessageVisibilityChanged(Node* form_control) override;
  void HandleEventListenerAdded(Node& node,
                                const AtomicString& event_type) override;
  void HandleEventListenerRemoved(Node& node,
                                  const AtomicString& event_type) override;
  void HandleFocusedUIElementChanged(Element* old_focused_element,
                                     Element* new_focused_element) override;
  void HandleInitialFocus() override;
  void HandleTextFormControlChanged(Node*) override;
  void HandleEditableTextContentChanged(Node*) override;
  void HandleDeletionOrInsertionInTextField(
      const SelectionInDOMTree& changed_selection,
      bool is_deletion) override;
  void HandleTextMarkerDataAdded(Node* start, Node* end) override;
  void HandleValueChanged(Node*) override;
  void HandleUpdateActiveMenuOption(Node*) override;
  void DidShowMenuListPopup(LayoutObject*) override;
  void DidHideMenuListPopup(LayoutObject*) override;
  void HandleLoadStart(Document*) override;
  void HandleLoadComplete(Document*) override;
  void HandleLayoutComplete(Document*) override;
  void HandleClicked(Node*) override;
  void HandleAttributeChanged(const QualifiedName& attr_name,
                              AccessibleNode*) override;

  void SetCanvasObjectBounds(HTMLCanvasElement*,
                             Element*,
                             const PhysicalRect&) override;

  void InlineTextBoxesUpdated(LayoutObject*) override;
  // Called during the accessibility lifecycle to refresh the AX tree.
  void ProcessDeferredAccessibilityEvents(Document&) override;
  // Remove AXObject subtrees (once flat tree traversal is safe).
  void ProcessSubtreeRemovals() override;
  // Is there work to be done when layout becomes clean?
  bool IsDirty() override;

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
  // SerializeLocationChanges.
  void InvalidateBoundingBox(const LayoutObject*) override;

  void SetCachedBoundingBox(AXID id, const ui::AXRelativeBounds& bounds);

  void SerializerClearedNode(AXID id);

  const AtomicString& ComputedRoleForNode(Node*) override;
  String ComputedNameForNode(Node*) override;

  void OnTouchAccessibilityHover(const gfx::Point&) override;

  AXObject* ObjectFromAXID(AXID id) const override;
  AXObject* Root();

  // Used for objects without backing DOM nodes, layout objects, etc.
  AXObject* CreateAndInit(ax::mojom::blink::Role, AXObject* parent);

  // Note that these functions do NOT guarantee that an AXObject will
  // be created. For instance, not all HTMLElements can have an AXObject,
  // such as <head> or <script> tags.
  AXObject* GetOrCreate(AccessibleNode*, AXObject* parent);
  AXObject* GetOrCreate(LayoutObject*, AXObject* parent_if_known) override;
  AXObject* GetOrCreate(LayoutObject* layout_object);
  AXObject* GetOrCreate(const Node*, AXObject* parent_if_known);
  AXObject* GetOrCreate(Node*, AXObject* parent_if_known);
  AXObject* GetOrCreate(Node*);
  AXObject* GetOrCreate(const Node*);
  AXObject* GetOrCreate(NGAbstractInlineTextBox*, AXObject* parent_if_known);

  AXID GetAXID(Node*) override;

  AXID GetExistingAXID(Node*) override;

  // Return an AXObject for the AccessibleNode. If the AccessibleNode is
  // attached to an element, will return the AXObject for that element instead.
  AXObject* Get(AccessibleNode*);
  AXObject* Get(NGAbstractInlineTextBox*);

  // Get an AXObject* backed by the passed-in DOM node or the node's layout
  // object, whichever is available.
  // If it no longer the correct type of AXObject (AXNodeObject/AXLayoutObject),
  // will Invalidate() the AXObject so that it is refreshed with a new object
  // when safe to do so.
  AXObject* Get(const Node*);
  // Get an AXObject* backed by the passed-in LayoutObject, or the
  // LayoutObject's DOM node, whiever is available.
  // If |parent_for_repair| is provided, and the object had been detached from
  // its parent, it will be set as the new parent.
  AXObject* Get(const LayoutObject*, AXObject* parent_for_repair = nullptr);

  // Get an AXObject* in a way that is safe for the current calling context:
  // - No calls into layout during an unclean layout phase
  // - Does not walk the flat tree during slot reassignment.
  // - Will not do invalidations from display locking changes, unless the
  //   caller passes in true for allow_display_locking_invalidation.
  //   This is generally safe to do, but may not be desirable e.g. when
  //   simply writing a DCHECK, where a pure get is optimal so as to avoid
  //   changing behavior.
  AXObject* SafeGet(const Node* node,
                    bool allow_display_locking_invalidation = false,
                    bool allow_layout_object_relevance_check = false);

  // Return true if the object is still part of the tree, meaning that ancestors
  // exist or can be repaired all the way to the root.
  bool IsStillInTree(AXObject*);

  void ChildrenChangedWithCleanLayout(Node* optional_node_for_relation_update,
                                      AXObject*);

  // Mark an object or subtree dirty, aka its properties have changed and it
  // needs to be reserialized. Use the |*WithCleanLayout| versions when layout
  // is already known to be clean.
  void MarkAXObjectDirty(AXObject*);

  void MarkAXObjectDirtyWithCleanLayout(AXObject*);
  void MarkAXObjectDirtyWithCleanLayoutAndEvent(
      AXObject*,
      ax::mojom::blink::EventFrom event_from,
      ax::mojom::blink::Action event_from_action);

  void MarkAXSubtreeDirtyWithCleanLayout(AXObject*);

  // Set the parent of |child|. If no parent is possible, this means the child
  // can no longer be in the AXTree, so remove the child.
  AXObject* RestoreParentOrPrune(AXObject* child);

  // When an object is created or its id changes, this must be called so that
  // the relation cache is updated.
  void MaybeNewRelationTarget(Node& node, AXObject* obj);

  void HandleActiveDescendantChangedWithCleanLayout(Node*);
  void SectionOrRegionRoleMaybeChangedWithCleanLayout(Node*);
  void TableCellRoleMaybeChanged(Node* node);
  void HandleRoleMaybeChangedWithCleanLayout(Node*);
  void HandleRoleChangeWithCleanLayout(Node*);
  void HandleAriaExpandedChangeWithCleanLayout(Node*);
  void HandleAriaSelectedChangedWithCleanLayout(Node*);
  void HandleAriaPressedChangedWithCleanLayout(Node*);
  void HandleNodeLostFocusWithCleanLayout(Node*);
  void HandleNodeGainedFocusWithCleanLayout(Node*);
  void NodeIsAttachedWithCleanLayout(Node*);
  void DidShowMenuListPopupWithCleanLayout(Node*);
  void DidHideMenuListPopupWithCleanLayout(Node*);
  void HandleScrollPositionChangedWithCleanLayout(Node*);
  void HandleValidationMessageVisibilityChangedWithCleanLayout(const Node*);
  void HandleUpdateActiveMenuOptionWithCleanLayout(Node*);
  void HandleEditableTextContentChangedWithCleanLayout(Node*);
  void UpdateTableRoleWithCleanLayout(Node*);

  bool InlineTextBoxAccessibilityEnabled();

  AXID GenerateAXID() const override;

  void AddAriaNotification(Node*,
                           const String,
                           const AriaNotificationOptions*) override;

  void PostNotification(const LayoutObject*, ax::mojom::blink::Event);
  // Creates object if necessary.
  void EnsurePostNotification(Node*, ax::mojom::blink::Event);
  void EnsureMarkDirtyWithCleanLayout(Node*);
  // Does not create object.
  // TODO(accessibility) Find out if we can merge with EnsurePostNotification().
  void PostNotification(Node*, ax::mojom::blink::Event);
  void PostNotification(AXObject*, ax::mojom::blink::Event);

  //
  // Aria-owns support.
  //

  // Returns true if the given object's position in the tree was due to
  // aria-owns.
  bool IsAriaOwned(const AXObject*) const;

  // Returns the parent of the given object due to aria-owns, if valid.
  AXObject* ValidatedAriaOwner(const AXObject*) const;

  // Given an object that has an aria-owns attribute, return the validated
  // set of aria-owned children.
  void ValidatedAriaOwnedChildren(const AXObject* owner,
                                  HeapVector<Member<AXObject>>& owned_children);

  // Given a <map> element, get the image currently associated with it, if any.
  AXObject* GetAXImageForMap(HTMLMapElement& map);

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

  // For built-in HTML form validation messages. Set notify_children_changed to
  // true if not already processing changed children.
  AXObject* ValidationMessageObjectIfInvalid(bool notify_children_changed);

  WebAXAutofillState GetAutofillState(AXID id) const;
  void SetAutofillState(AXID id, WebAXAutofillState state);

  std::pair<ax::mojom::blink::EventFrom, ax::mojom::blink::Action>
  active_event_from_data() const {
    return std::make_pair(active_event_from_, active_event_from_action_);
  }

  void set_active_event_from_data(
      const ax::mojom::blink::EventFrom event_from,
      const ax::mojom::blink::Action event_from_action) {
    active_event_from_ = event_from;
    active_event_from_action_ = event_from_action;
  }

  Element* GetActiveAriaModalDialog() const;

  static bool UseAXMenuList() { return use_ax_menu_list_; }
  static bool ShouldCreateAXMenuListFor(LayoutObject* layout_object);
  static bool ShouldCreateAXMenuListOptionFor(const Node*);
  static bool IsRelevantPseudoElement(const Node& node);
  static bool IsRelevantPseudoElementDescendant(
      const LayoutObject& layout_object);
  static bool IsRelevantSlotElement(const HTMLSlotElement& slot);

  // Retrieves a vector of all AXObjects whose bounding boxes may have changed
  // since the last query. Sends the resulting vector over mojo to the browser
  // process. Clears the vector so that the next time it's
  // called, it will only retrieve objects that have changed since now.
  void SerializeLocationChanges(uint32_t reset_token) override;

  // Searches the accessibility tree for plugin's root object and returns it.
  // Returns an empty WebAXObject if no root object is present.
  AXObject* GetPluginRoot() override {
    ax_tree_source_->Freeze();
    AXObject* result = ax_tree_source_->GetPluginRoot();
    ax_tree_source_->Thaw();
    return result;
  }

  bool SerializeEntireTree(size_t max_node_count,
                           base::TimeDelta timeout,
                           ui::AXTreeUpdate*) override;

  void MarkAXObjectDirtyWithDetails(
      AXObject* obj,
      bool subtree,
      ax::mojom::blink::EventFrom event_from,
      ax::mojom::blink::Action event_from_action,
      const std::vector<ui::AXEventIntent>& event_intents) override;

  void SerializeDirtyObjectsAndEvents(
      bool has_plugin_tree_source,
      std::vector<ui::AXTreeUpdate>& updates,
      std::vector<ui::AXEvent>& events,
      bool& had_end_of_test_event,
      bool& had_load_complete_messages,
      bool& need_to_send_location_changes) override;

  void GetImagesToAnnotate(ui::AXTreeUpdate& updates,
                           std::vector<ui::AXNodeData*>& nodes) override;

  bool HasDirtyObjects() const override { return !dirty_objects_.empty(); }

  bool AddPendingEvent(const ui::AXEvent& event,
                       bool insert_at_beginning) override;

  void MarkSerializerSubtreeDirty(AXObject& obj) {
    ax_tree_serializer_->MarkSubtreeDirty(&obj);
  }

  bool IsDirty(AXObject& obj) { return ax_tree_serializer_->IsDirty(&obj); }

  void SetImageAsDataNodeId(int id, const gfx::Size& max_size) {
    ax_tree_source_->set_image_data_node_id(id, max_size);
  }

  int image_data_node_id() { return ax_tree_source_->image_data_node_id(); }

  static constexpr int kDataTableHeuristicMinRows = 20;

  void UpdateAXForAllDocuments() override;
  void MarkDocumentDirty() override;
  void ResetSerializer() override;
  void MarkElementDirty(const Node*) override;
  void MarkElementDirtyWithCleanLayout(const Node*);

  // TODO(accessibility) Create an a11y lifecycle that encompasses these.
  // Layout is clean and the cache is processing callbacks.
  bool IsProcessingDeferredEvents() const {
    return processing_deferred_events_;
  }
  bool EntireDocumentIsDirty() const { return mark_all_dirty_; }
  // Returns true if UpdateTreeIfNeeded has been called and has not finished.
  bool UpdatingTree() { return updating_tree_; }
  // The document/cache are in the tear-down phase.
  bool HasBeenDisposed() const { return has_been_disposed_; }
  // Assert that tree is completely up-to-date.
  void CheckTreeIsUpdated() const;

  // Returns the `TextChangedOperation` associated with the `id` from the
  // `text_operation_in_node_ids_` map, if `id` is in the map.
  WTF::Vector<TextChangedOperation>* GetFromTextOperationInNodeIdMap(AXID id);

  // Clears the map after each call, should be called after each serialization.
  void ClearTextOperationInNodeIdMap();

  // TODO(accessibility) Convert methods consuming this into members so that we
  // can remove this accessor method.
  HashMap<DOMNodeId, bool>& whitespace_ignored_map() {
    return whitespace_ignored_map_;
  }

 protected:
  void PostPlatformNotification(
      AXObject* obj,
      ax::mojom::blink::Event event_type,
      ax::mojom::blink::EventFrom event_from =
          ax::mojom::blink::EventFrom::kNone,
      ax::mojom::blink::Action event_from_action =
          ax::mojom::blink::Action::kNone,
      const BlinkAXEventIntentsSet& event_intents = BlinkAXEventIntentsSet());
  void IdChangedWithCleanLayout(Node*);
  void AriaOwnsChangedWithCleanLayout(Node*);

  // Returns a reference to the set of currently active event intents.
  BlinkAXEventIntentsSet& ActiveEventIntents() override {
    return active_event_intents_;
  }

  // Mark object as invalid and needing to be refreshed when layout is clean.
  // Will result in a new object with the same AXID, and will also call
  // ChildrenChanged() on the parent of invalidated objects. Automatically
  // de-dupes extra object refreshes and ChildrenChanged() calls.
  void Invalidate(Document&, AXID);

 private:
  struct AXDirtyObject : public GarbageCollected<AXDirtyObject> {
    AXDirtyObject(AXObject* obj_arg,
                  ax::mojom::blink::EventFrom event_from_arg,
                  ax::mojom::blink::Action event_from_action_arg,
                  std::vector<ui::AXEventIntent> event_intents_arg)
        : obj(obj_arg),
          event_from(event_from_arg),
          event_from_action(event_from_action_arg),
          event_intents(event_intents_arg) {}

    static AXDirtyObject* Create(AXObject* obj,
                                 ax::mojom::blink::EventFrom event_from,
                                 ax::mojom::blink::Action event_from_action,
                                 std::vector<ui::AXEventIntent> event_intents) {
      return MakeGarbageCollected<AXDirtyObject>(
          obj, event_from, event_from_action, event_intents);
    }

    void Trace(Visitor* visitor) const { visitor->Trace(obj); }

    Member<AXObject> obj;
    ax::mojom::blink::EventFrom event_from;
    ax::mojom::blink::Action event_from_action;
    std::vector<ui::AXEventIntent> event_intents ALLOW_DISCOURAGED_TYPE(
        "Avoids conversion when passed from/to ui::AXTreeUpdate or "
        "blink::WebAXObject");
  };

  // Updates the AX tree by walking from the root, calling AXObject::
  // UpdateChildrenIfNecessary on each AXObject for which NeedsUpdate is true.
  // This method is part of a11y-during-render, and in particular transitioning
  // to an eager (as opposed to lazy) AX tree update pattern. See
  // https://bugs.chromium.org/p/chromium/issues/detail?id=1342801#c12 for more
  // details.
  void UpdateTreeIfNeeded();

  // Make sure a relation cache exists and is initialized. Mst be called with
  // clean layout.
  void EnsureRelationCache();

  // Create an AXObject, and do not check if a previous one exists.
  // Also, initialize the object and add it to maps for later retrieval.
  AXObject* CreateAndInit(Node*,
                          LayoutObject*,
                          AXObject* parent_if_known,
                          AXID use_axid = 0);
  // Helpers for CreateAndInit().
  AXObject* CreateFromRenderer(LayoutObject*);
  AXObject* CreateFromNode(Node*);

  AXObject* CreateFromInlineTextBox(NGAbstractInlineTextBox*);

  // Removes AXObject backed by passed-in object, if there is one.
  // It will also notify the parent that its children have changed, so that the
  // parent will recompute its children and be reserialized, unless
  // |notify_parent| is passed in as false.
  void Remove(AccessibleNode*, bool notify_parent);
  void Remove(LayoutObject*, bool notify_parent);
  void Remove(NGAbstractInlineTextBox*, bool notify_parent);

  // Helper to remove the object from the cache.
  // Most callers should be using Remove(AXObject) instead.
  void Remove(AXID, bool notify_parent);
  // Helper to clean up any references to the AXObject's AXID.
  void RemoveReferencesToAXID(AXID);

  HeapMojoRemote<mojom::blink::RenderAccessibilityHost>&
  GetOrCreateRemoteRenderAccessibilityHost();
  WebLocalFrameClient* GetWebLocalFrameClient() const;
  void ProcessDeferredAccessibilityEventsImpl(Document&);
  void UpdateLifecycleIfNeeded(Document& document);

  // Is the main document currently parsing content, as opposed to being blocked
  // by script execution or being load complete state.
  bool IsParsingMainDocument() const;

  bool IsMainDocumentDirty() const;
  bool IsPopupDocumentDirty() const;

  void ProcessSubtreeRemoval(Node*, bool remove_root);

  HeapHashSet<WeakMember<InspectorAccessibilityAgent>> agents_;

  struct AXEventParams final : public GarbageCollected<AXEventParams> {
    AXEventParams(AXObject* target,
                  ax::mojom::blink::Event event_type,
                  ax::mojom::blink::EventFrom event_from,
                  ax::mojom::blink::Action event_from_action,
                  const BlinkAXEventIntentsSet& intents)
        : target(target),
          event_type(event_type),
          event_from(event_from),
          event_from_action(event_from_action) {
      for (const auto& intent : intents) {
        event_intents.insert(intent.key, intent.value);
      }
    }
    Member<AXObject> target;
    ax::mojom::blink::Event event_type;
    ax::mojom::blink::EventFrom event_from;
    ax::mojom::blink::Action event_from_action;
    BlinkAXEventIntentsSet event_intents;

    void Trace(Visitor* visitor) const { visitor->Trace(target); }
  };

  // The following represent functions that could be used as callbacks for
  // DeferTreeUpdate. Every enum value represents a function that would be
  // called after a tree update is complete.
  // Please don't reuse these enums in multiple callers to DeferTreeUpdate().
  // Instead, add an enum where the suffix describes where it's being called
  // from (this helps when debugging an issue apparent in clean layout, by
  // helping clarify the code paths).
  enum class TreeUpdateReason : uint8_t {
    // These updates are always associated with a DOM Node:
    kActiveDescendantChanged = 1,
    kAriaExpandedChanged = 2,
    kAriaOwnsChanged = 3,
    kAriaPressedChanged = 4,
    kAriaSelectedChanged = 5,
    kDidHideMenuListPopup = 6,
    kDidShowMenuListPopup = 7,
    kEditableTextContentChanged = 8,
    kFocusableChanged = 9,
    kIdChanged = 10,
    kMarkDirtyFromHandleLayout = 11,
    kMarkDirtyFromHandleScroll = 12,
    kMarkDirtyFromRemove = 13,
    kNameAttributeChanged = 14,
    kNodeGainedFocus = 15,
    kNodeLostFocus = 16,
    kPostNotificationFromHandleLoadComplete = 17,
    kPostNotificationFromHandleLoadStart = 18,
    kPostNotificationFromHandleScrolledToAnchor = 19,
    kRemoveValidationMessageObjectFromFocusedUIElement = 20,
    kRemoveValidationMessageObjectFromValidationMessageObject = 21,
    kRoleChangeFromAriaHasPopup = 22,
    kRoleChangeFromRoleOrType = 23,
    kRoleMaybeChangedFromEventListener = 24,
    kRoleMaybeChangedFromHref = 25,
    kSectionOrRegionRoleMaybeChangedFromLabel = 26,
    kSectionOrRegionRoleMaybeChangedFromLabelledBy = 27,
    kSectionOrRegionRoleMaybeChangedFromTitle = 28,
    kTextChangedFromTextChangedNode = 29,
    kTextMarkerDataAdded = 30,
    kUpdateActiveMenuOption = 31,
    kNodeIsAttached = 32,
    kUpdateTableRole = 33,
    kUseMapAttributeChanged = 34,
    kValidationMessageVisibilityChanged = 35,

    // These updates are associated with an AXID:
    kChildrenChanged = 100,
    kMarkAXObjectDirty = 101,
    kMarkAXSubtreeDirty = 102,
    kTextChangedFromTextChangedAXObject = 103
  };

  struct TreeUpdateParams final : public GarbageCollected<TreeUpdateParams> {
    TreeUpdateParams(
        Node* node_arg,
        AXID axid_arg,
        ax::mojom::blink::EventFrom event_from_arg,
        ax::mojom::blink::Action event_from_action_arg,
        const BlinkAXEventIntentsSet& intents_arg,
        TreeUpdateReason update_reason_arg,
        ax::mojom::blink::Event event_arg = ax::mojom::blink::Event::kNone)
        : node(node_arg),
          axid(axid_arg),
          event(event_arg),
          event_from(event_from_arg),
          update_reason(update_reason_arg),
          event_from_action(event_from_action_arg) {
      for (const auto& intent : intents_arg) {
        DCHECK(node || axid) << "Either a DOM Node or AXID is required.";
        DCHECK(!node || !axid) << "Provide a DOM Node *or* AXID, not both.";
        event_intents.insert(intent.key, intent.value);
      }
    }

    // Only either node or AXID will be filled at a time. Some events use Node
    // while others use AXObject.
    WeakMember<Node> node;
    AXID axid;

    ax::mojom::blink::Event event;
    ax::mojom::blink::EventFrom event_from;
    TreeUpdateReason update_reason;
    ax::mojom::blink::Action event_from_action;
    BlinkAXEventIntentsSet event_intents;

    void Trace(Visitor* visitor) const { visitor->Trace(node); }
  };

  typedef HeapVector<Member<TreeUpdateParams>> TreeUpdateCallbackQueue;

  ax::mojom::blink::EventFrom ComputeEventFrom();

  void MarkAXObjectDirtyWithCleanLayoutHelper(
      AXObject* obj,
      bool subtree,
      ax::mojom::blink::EventFrom event_from,
      ax::mojom::blink::Action event_from_action);
  void MarkAXSubtreeDirty(AXObject*);
  void MarkDocumentDirtyWithCleanLayout();

  // Given an object to mark dirty or fire an event on, return an object
  // included in the tree that can be used with the serializer, or null if there
  // is no relevant object to use. Objects that are not included in the tree,
  // and have no ancestor object included in the tree, are pruned from the tree,
  // in which case there is nothing to be serialized.
  AXObject* GetSerializationTarget(AXObject* obj);

  // Helper that clears children up to the first included ancestor and returns
  // the ancestor if a children changed notification should be fired on it.
  AXObject* InvalidateChildren(AXObject* obj);

  Member<Document> document_;
  Member<Document> popup_document_;

  ui::AXMode ax_mode_;
  HeapHashMap<AXID, Member<AXObject>> objects_;
  HeapHashMap<Member<AccessibleNode>, AXID> accessible_node_mapping_;
  HeapHashMap<Member<const LayoutObject>, AXID> layout_object_mapping_;
  HeapHashMap<Member<const Node>, AXID> node_object_mapping_;
  HeapHashMap<Member<NGAbstractInlineTextBox>, AXID>
      inline_text_box_object_mapping_;

  // Used for a mock AXObject representing the message displayed in the
  // validation message bubble.
  // There can be only one of these per document with invalid form controls,
  // and it will always be related to the currently focused control.
  AXID validation_message_axid_;

  // The currently active aria-modal dialog element, if one has been computed,
  // null if otherwise. This is only ever computed on platforms that have the
  // AriaModalPrunesAXTree setting enabled, such as Mac.
  WeakMember<Element> active_aria_modal_dialog_;

  // If non-null, this is the node that the current aria-activedescendant caused
  // to have the selected state.
  WeakMember<Node> last_selected_from_active_descendant_;

  std::unique_ptr<AXRelationCache> relation_cache_;

  // Stages of cache/tree.
  // If all of these are false, the cache can collect updates to-be-processed
  // via callbacks from DOM/layout.
  // TODO(accessibility) Replace these with something like a document lifecycle.
  // Tree is being updated.
  bool processing_deferred_events_ = false;
  // Tree is frozen and beign serialized.
  bool is_frozen_ = false;  // Used with Freeze(), Thaw() and IsFrozen() above.
  // Tree and cache are being destroyed.
  bool has_been_disposed_ = false;

#if DCHECK_IS_ON()
  bool updating_layout_and_ax_ = false;
#endif

  // If true, do not do work to process a11y or build the a11y in
  // ProcessDeferredAccessibilityEvents(). Will be set to false when more
  // content is loaded or the load is completed.
  bool pause_tree_updates_until_more_loaded_content_ = false;

  HeapVector<Member<AXEventParams>> notifications_to_post_main_;
  HeapVector<Member<AXEventParams>> notifications_to_post_popup_;

  // Call the queued callback methods that do processing which must occur when
  // layout is clean. These callbacks are stored in tree_update_callback_queue_,
  // and have names like FooBarredWithCleanLayout().
  void ProcessCleanLayoutCallbacks(Document&);

  // Destroy and recreate any objects which are no longer valid, for example
  // they used to be an AXNodeObject and now must be an AXLayoutObject, or
  // vice-versa. Also fires children changed on the parent of these nodes.
  void ProcessInvalidatedObjects(Document&);

  // Send events to RenderAccessibilityImpl, which serializes them and then
  // sends the serialized events and dirty objects to the browser process.
  void PostNotifications(Document&);

  // Get the currently focused Node (an element or a document).
  Node* FocusedNode();

  AXObject* FocusedImageMapUIElement(HTMLAreaElement*);

  // Associate an AXObject with an AXID. Generate one if none is supplied.
  AXID AssociateAXID(AXObject*, AXID use_axid = 0);

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

  // Object for HTML validation alerts. Created at most once per object cache.
  AXObject* GetOrCreateValidationMessageObject();
  void RemoveValidationMessageObjectWithCleanLayout(Node* document);

  // To be called inside DeferTreeUpdate to check the queue status before
  // adding.
  bool CanDeferTreeUpdate(Document* tree_update_document);

  // Checks the update queue, then pauses and rebuilds it if full. Returns true
  // of the queue was paused.
  bool PauseTreeUpdatesIfQueueFull();

  // Enqueue a callback to the given method to be run after layout is
  // complete.
  void DeferTreeUpdate(
      AXObjectCacheImpl::TreeUpdateReason update_reason,
      Node* node,
      ax::mojom::blink::Event event = ax::mojom::blink::Event::kNone);

  // Provide either a DOM node or AXObject. If both are provided, then they must
  // match, meaning that the AXObject's DOM node must equal the provided node.
  void DeferTreeUpdate(
      AXObjectCacheImpl::TreeUpdateReason update_reason,
      AXObject* obj,
      ax::mojom::blink::Event event = ax::mojom::blink::Event::kNone);

  void TextChangedWithCleanLayout(Node* node);
  void ChildrenChangedWithCleanLayout(Node* node);

  // If the presence of document markers changed for the given text node, then
  // call children changed.
  void HandleTextMarkerDataAddedWithCleanLayout(Node*);
  void HandleUseMapAttributeChangedWithCleanLayout(Node*);
  void HandleNameAttributeChangedWithCleanLayout(Node*);

  bool DoesEventListenerImpactIgnoredState(const AtomicString& event_type,
                                           const Node& node) const;
  void HandleEventSubscriptionChanged(Node& node,
                                      const AtomicString& event_type);

  //
  // aria-modal support
  //

  // This function is only ever called on platforms where the
  // AriaModalPrunesAXTree setting is enabled, and the accessibility tree must
  // be manually pruned to remove background content.
  void UpdateActiveAriaModalDialog(Node* element);

  // This will return null on platforms without the AriaModalPrunesAXTree
  // setting enabled, or where there is no active ancestral aria-modal dialog.
  Element* AncestorAriaModalDialog(Node* node);

  // Return the AXObject for the update if it is relevant (its backing data has
  // not been destroyed and it is attached to the expected tree document).
  AXObject* TreeUpdateObjectIfRelevant(Document& document,
                                       TreeUpdateParams* tree_update);

  void FireTreeUpdatedEventImmediately(TreeUpdateParams* tree_update,
                                       AXObject* ax_object);

  void FireAXEventImmediately(AXObject* obj,
                              ax::mojom::blink::Event event_type,
                              ax::mojom::blink::EventFrom event_from,
                              ax::mojom::blink::Action event_from_action,
                              const BlinkAXEventIntentsSet& event_intents);

  void SetMaxPendingUpdatesForTesting(wtf_size_t max_pending_updates) {
    max_pending_updates_ = max_pending_updates;
  }

  void UpdateNumTreeUpdatesQueuedBeforeLayoutHistogram();

  // Invalidates the bounding boxes of fixed or sticky positioned objects which
  // should be updated when the scroll offset is changed. Like
  // InvalidateBoundingBox, it can be later retrieved by
  // SerializeLocationChanges.
  void InvalidateBoundingBoxForFixedOrStickyPosition();

  // Return true if this is the popup document. There can only be one popup
  // document at a time. If it is not the popup document, it's the main
  // document stored in |document_|.
  bool IsPopup(Document& document) const;

  // Get the invalidated objects for the passed-in document.
  HashSet<AXID>& GetInvalidatedIds(Document& document);

  // Get the queued tree update callbacks for the passed-in document
  TreeUpdateCallbackQueue& GetTreeUpdateCallbackQueue(Document& document);

  // Get the event notifications to post for the passed-in document.
  HeapVector<Member<AXEventParams>>& GetNotificationsToPost(Document& document);

  // Whether the user has granted permission for the user to install event
  // listeners for accessibility events using the AOM.
  mojom::PermissionStatus accessibility_event_permission_;
  // The permission service, enabling us to check for event listener
  // permission.
  HeapMojoRemote<mojom::blink::PermissionService> permission_service_;
  HeapMojoReceiver<mojom::blink::PermissionObserver, AXObjectCacheImpl>
      permission_observer_receiver_;

  // Queued callbacks.
  TreeUpdateCallbackQueue tree_update_callback_queue_main_;
  TreeUpdateCallbackQueue tree_update_callback_queue_popup_;

  // Help de-dupe processing of repetitive events.
  HeapHashSet<WeakMember<Node>> nodes_with_pending_children_changed_;
  HashSet<AXID> nodes_with_pending_location_changed_;

  // Nodes with document markers that have received accessibility updates.
  HeapHashSet<WeakMember<Node>> nodes_with_spelling_or_grammar_markers_;

  // Nodes renoved from flat tree.
  HeapVector<std::pair<Member<Node>, bool>> nodes_for_subtree_removal_;

  // True when layout has changed, and changed locations must be serialized.
  bool need_to_send_location_changes_ = false;

  AXID last_value_change_node_ = ui::AXNodeData::kInvalidAXID;

  // If tree_update_callback_queue_ gets improbably large, stop
  // enqueueing updates and fire a single ChildrenChanged event on the
  // document once layout occurs.
  wtf_size_t max_pending_updates_ = 1UL << 16;
  bool tree_updates_paused_ = false;

  // Maps ids to their object's autofill state.
  HashMap<AXID, WebAXAutofillState> autofill_state_map_;

  // The set of node IDs whose bounds has changed since the last time
  // SerializeLocationChanges was called.
  HashSet<AXID> changed_bounds_ids_;

  // Known locations and sizes of bounding boxes that are known to have been
  // serialized.
  HashMap<AXID, ui::AXRelativeBounds> cached_bounding_boxes_;

  // The list of node IDs whose position is fixed or sticky.
  HashSet<AXID> fixed_or_sticky_node_ids_;

  // Map of node IDs where there was an operation done, could be deletion or
  // insertion. The items in the vector are in the order that the operations
  // were made in.
  HashMap<AXID, WTF::Vector<TextChangedOperation>> text_operation_in_node_ids_;

  // The source of the event that is currently being handled.
  ax::mojom::blink::EventFrom active_event_from_ =
      ax::mojom::blink::EventFrom::kNone;

  // The accessibility action that caused the event. Will only be valid if
  // active_event_from_ is set to kAction.
  ax::mojom::blink::Action active_event_from_action_ =
      ax::mojom::blink::Action::kNone;

  // A set of currently active event intents.
  BlinkAXEventIntentsSet active_event_intents_;

  // A set of aria notifications that have yet to be added to ax_tree_data.
  HeapVector<Member<AriaNotification>> aria_notifications_;

  // Set of ID's of current AXObjects that need to be destroyed and recreated.
  HashSet<AXID> invalidated_ids_main_;
  HashSet<AXID> invalidated_ids_popup_;

  // If false, exposes the internal accessibility tree of a select pop-up
  // instead.
  static bool use_ax_menu_list_;

  HeapMojoRemote<mojom::blink::RenderAccessibilityHost>
      render_accessibility_host_;

  Member<BlinkAXTreeSource> ax_tree_source_;
  std::unique_ptr<ui::AXTreeSerializer<AXObject*, HeapVector<Member<AXObject>>>>
      ax_tree_serializer_;

  HeapDeque<Member<AXDirtyObject>> dirty_objects_;

  Deque<ui::AXEvent> pending_events_;

  HashMap<DOMNodeId, bool> whitespace_ignored_map_;

  bool updating_tree_ = false;
  // Make sure the next serialization sends everything.
  bool mark_all_dirty_ = false;

  FRIEND_TEST_ALL_PREFIXES(AccessibilityTest, PauseUpdatesAfterMaxNumberQueued);
  FRIEND_TEST_ALL_PREFIXES(AccessibilityTest, RemoveReferencesToAXID);
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
