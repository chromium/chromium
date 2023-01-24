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
#include "third_party/blink/renderer/platform/wtf/gc_plugin.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/accessibility/ax_enums.mojom-blink-forward.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_tree_serializer.h"

namespace blink {

class AbstractInlineTextBox;
class AXRelationCache;
class HTMLAreaElement;
class LocalFrameView;
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

  void Freeze() override {
    is_frozen_ = true;
    ax_tree_source_->Freeze();
  }
  void Thaw() override {
    is_frozen_ = false;
    ax_tree_source_->Thaw();
  }
  bool IsFrozen() { return is_frozen_; }

  //
  // Iterators.
  //

  void SelectionChanged(Node*) override;
  // Update reverse relation cache when aria-labelledby or aria-describedby
  // point to the relation_source.
  void UpdateReverseTextRelations(const AXObject* relation_source,
                                  const Vector<String>& target_ids);
  void ChildrenChanged(AXObject*);
  void ChildrenChangedWithCleanLayout(AXObject*);
  void ChildrenChanged(Node*) override;
  void ChildrenChanged(const LayoutObject*) override;
  void ChildrenChanged(AccessibleNode*) override;
  void SlotAssignmentWillChange(Node*) override;
  void CheckedStateChanged(Node*) override;
  void ListboxOptionStateChanged(HTMLOptionElement*) override;
  void ListboxSelectedChildrenChanged(HTMLSelectElement*) override;
  void ListboxActiveIndexChanged(HTMLSelectElement*) override;
  void LocationChanged(const LayoutObject*) override;
  void ImageLoaded(const LayoutObject*) override;

  void Remove(AccessibleNode*) override;
  void Remove(LayoutObject*) override;
  void Remove(Node*) override;
  void Remove(Document*) override;
  void Remove(AbstractInlineTextBox*) override;
  void Remove(AXObject*);  // Calls more specific Remove methods as necessary.

  // For any ancestor that could contain the passed-in AXObject* in their cached
  // children, clear their children and set needs to update children on them.
  // In addition, ChildrenChanged() on an included ancestor that might contain
  // this child, if one exists.
  void ChildrenChangedOnAncestorOf(AXObject*);

  const Element* RootAXEditableElement(const Node*) override;

  // Called when aspects of the style (e.g. color, alignment) change.
  void StyleChanged(const LayoutObject*) override;

  // Called by a node when text or a text equivalent (e.g. alt) attribute is
  // changed.
  void TextChanged(const LayoutObject*) override;
  void TextChangedWithCleanLayout(Node* optional_node, AXObject*);
  void FocusableChangedWithCleanLayout(Element* element);
  void DocumentTitleChanged() override;
  // Called when a layout tree for a node has just been attached, so we can make
  // sure we have the right subclass of AXObject.
  void UpdateCacheAfterNodeIsAttached(Node*) override;
  // A DOM node was inserted , but does not necessarily have a layout tree.
  void DidInsertChildrenOfNode(Node*) override;

  void HandleAttributeChanged(const QualifiedName& attr_name,
                              Element*) override;
  void HandleValidationMessageVisibilityChanged(
      const Node* form_control) override;
  void HandleEventListenerAdded(const Node& node,
                                const AtomicString& event_type) override;
  void HandleEventListenerRemoved(const Node& node,
                                  const AtomicString& event_type) override;
  void HandleFocusedUIElementChanged(Element* old_focused_element,
                                     Element* new_focused_element) override;
  void HandleInitialFocus() override;
  void HandleTextFormControlChanged(Node*) override;
  void HandleEditableTextContentChanged(Node*) override;
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
                             const LayoutRect&) override;

  void InlineTextBoxesUpdated(LayoutObject*) override;
  // Called during the accessibility lifecycle to refresh the AX tree.
  void ProcessDeferredAccessibilityEvents(Document&) override;
  // Is there work to be done when layout becomes clean?
  bool IsDirty() const override;

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

  AXObject* GetOrCreate(AccessibleNode*, AXObject* parent);
  AXObject* GetOrCreate(LayoutObject*, AXObject* parent_if_known) override;
  AXObject* GetOrCreate(LayoutObject* layout_object);
  AXObject* GetOrCreate(const Node*, AXObject* parent_if_known);
  AXObject* GetOrCreate(Node*, AXObject* parent_if_known);
  AXObject* GetOrCreate(Node*);
  AXObject* GetOrCreate(const Node*);
  AXObject* GetOrCreate(AbstractInlineTextBox*, AXObject* parent_if_known);

  AXID GetAXID(Node*) override;

  AXID GetExistingAXID(Node*) override;

  // Return an AXObject for the AccessibleNode. If the AccessibleNode is
  // attached to an element, will return the AXObject for that element instead.
  AXObject* Get(AccessibleNode*);
  AXObject* Get(AbstractInlineTextBox*);

  // Get an AXObject* backed by the passed-in DOM node or the node's layout
  // object, whichever is available.
  // If it no longer the correct type of AXObject (AXNodeObject/AXLayoutObject),
  // will Invalidate() the AXObject so that it is refreshed with a new object
  // when safe to do so.
  AXObject* Get(const Node*);
  AXObject* Get(const LayoutObject*);

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
  void SectionOrRegionRoleMaybeChanged(Element* element);
  void HandleRoleChangeWithCleanLayout(Node*);
  void HandleAriaHiddenChangedWithCleanLayout(Node*);
  void HandleAriaExpandedChangeWithCleanLayout(Node*);
  void HandleAriaSelectedChangedWithCleanLayout(Node*);
  void HandleAriaPressedChangedWithCleanLayout(Element*);
  void HandleNodeLostFocusWithCleanLayout(Node*);
  void HandleNodeGainedFocusWithCleanLayout(Node*);
  void UpdateCacheAfterNodeIsAttachedWithCleanLayout(Node*);
  void DidShowMenuListPopupWithCleanLayout(Node*);
  void DidHideMenuListPopupWithCleanLayout(Node*);
  void HandleScrollPositionChangedWithCleanLayout(Node*);
  void HandleValidationMessageVisibilityChangedWithCleanLayout(const Node*);
  void HandleUpdateActiveMenuOptionWithCleanLayout(Node*);
  void HandleEditableTextContentChangedWithCleanLayout(Node*);

  bool InlineTextBoxAccessibilityEnabled();

  void RemoveAXID(AXObject*);

  AXID GenerateAXID() const override;

  void AddAriaNotification(Node*,
                           const String,
                           const AriaNotificationOptions*) override;

  // Counts the number of times the document has been modified. Some attribute
  // values are cached as long as the modification count hasn't changed.
  int ModificationCount() const { return modification_count_; }
  void IncrementModificationCount() { ++modification_count_; }

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

  AXObject* GetActiveAriaModalDialog() const;

  static bool UseAXMenuList() { return use_ax_menu_list_; }
  static bool ShouldCreateAXMenuListFor(LayoutObject* layout_object);
  static bool ShouldCreateAXMenuListOptionFor(const Node*);
  static bool IsRelevantPseudoElement(const Node& node);
  static bool IsRelevantPseudoElementDescendant(
      const LayoutObject& layout_object);
  static bool IsRelevantSlotElement(const HTMLSlotElement& slot);

  bool HasBeenDisposed() { return has_been_disposed_; }

  // Retrieves a vector of all AXObjects whose bounding boxes may have changed
  // since the last query. Sends the resulting vector over mojo to the browser
  // process. Clears the vector so that the next time it's
  // called, it will only retrieve objects that have changed since now.
  void SerializeLocationChanges() override;

  // Searches the accessibility tree for plugin's root object and returns it.
  // Returns an empty WebAXObject if no root object is present.
  AXObject* GetPluginRoot() override {
    return ax_tree_source_->GetPluginRoot();
  }

  bool SerializeEntireTree(bool exclude_offscreen,
                           size_t max_node_count,
                           base::TimeDelta timeout,
                           ui::AXTreeUpdate*) override;

  void MarkAllImageAXObjectsDirty() override {
    return Root()->MarkAllImageAXObjectsDirty();
  }

  void ResetSerializer() override { ax_tree_serializer_->Reset(); }

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

  void ClearDirtyObjectsAndPendingEvents() override {
    dirty_objects_.clear();
    pending_events_.clear();
  }

  bool HasDirtyObjects() const override { return !dirty_objects_.empty(); }

  bool AddPendingEvent(const ui::AXEvent& event,
                       bool insert_at_beginning) override;

  void InvalidateSerializerSubtree(AXObject& obj) {
    ax_tree_serializer_->InvalidateSubtree(&obj);
  }

  bool SerializeChanges(AXObject& obj, ui::AXTreeUpdate* update) {
    return ax_tree_serializer_->SerializeChanges(&obj, update);
  }

  bool IsInClientTree(AXObject& obj) {
    return ax_tree_serializer_->IsInClientTree(&obj);
  }

  void OnLoadInlineTextBoxes(AXObject& obj) {
    ax_tree_source_->OnLoadInlineTextBoxes(obj);
  }

  bool ShouldLoadInlineTextBoxes(AXObject& obj) {
    return ax_tree_source_->ShouldLoadInlineTextBoxes(&obj);
  }

  void GetChildren(AXObject& parent, std::vector<AXObject*>* out_children) {
    return ax_tree_source_->GetChildren(&parent, out_children);
  }

  void SetImageAsDataNodeId(int id, const gfx::Size& max_size) {
    ax_tree_source_->set_image_data_node_id(id, max_size);
  }

  int image_data_node_id() { return ax_tree_source_->image_data_node_id(); }

  static constexpr int kDataTableHeuristicMinRows = 20;

  void UpdateAXForAllDocuments() override;

 protected:
  void PostPlatformNotification(
      AXObject* obj,
      ax::mojom::blink::Event event_type,
      ax::mojom::blink::EventFrom event_from =
          ax::mojom::blink::EventFrom::kNone,
      ax::mojom::blink::Action event_from_action =
          ax::mojom::blink::Action::kNone,
      const BlinkAXEventIntentsSet& event_intents = BlinkAXEventIntentsSet());
  void LabelChangedWithCleanLayout(Element*);

  // Returns a reference to the set of currently active event intents.
  BlinkAXEventIntentsSet& ActiveEventIntents() override {
    return active_event_intents_;
  }

  // Mark object as invalid and needing to be refreshed when layout is clean.
  // Will result in a new object with the same AXID, and will also call
  // ChildrenChanged() on the parent of invalidated objects. Automatically
  // de-dupes extra object refreshes and ChildrenChanged() calls.
  void Invalidate(Document&, AXID);

  void Remove(AXID);

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

  // Create an AXObject, and do not check if a previous one exists.
  // Also, initialize the object and add it to maps for later retrieval.
  AXObject* CreateAndInit(Node*,
                          LayoutObject*,
                          AXObject* parent_if_known,
                          AXID use_axid = 0);
  // Helpers for CreateAndInitIfRelevant() methods..
  AXObject* CreateFromRenderer(LayoutObject*);
  AXObject* CreateFromNode(Node*);

  AXObject* CreateFromInlineTextBox(AbstractInlineTextBox*);

  mojo::Remote<mojom::blink::RenderAccessibilityHost>&
  GetOrCreateRemoteRenderAccessibilityHost();
  WebLocalFrameClient* GetWebLocalFrameClient() const;
  void ProcessDeferredAccessibilityEventsImpl(Document&);
  void UpdateLifecycleIfNeeded(Document& document);

  bool IsMainDocumentDirty() const;
  bool IsPopupDocumentDirty() const;

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

  struct TreeUpdateParams final : public GarbageCollected<TreeUpdateParams> {
    TreeUpdateParams(const Node* node,
                     AXID axid,
                     ax::mojom::blink::EventFrom event_from,
                     ax::mojom::blink::Action event_from_action,
                     const BlinkAXEventIntentsSet& intents,
                     base::OnceClosure callback)
        : node(node),
          axid(axid),
          event_from(event_from),
          event_from_action(event_from_action),
          callback(std::move(callback)) {
      for (const auto& intent : intents) {
        event_intents.insert(intent.key, intent.value);
      }
    }
    WeakMember<const Node> node;
    AXID axid;
    ax::mojom::blink::EventFrom event_from;
    ax::mojom::blink::Action event_from_action;
    BlinkAXEventIntentsSet event_intents;
    base::OnceClosure callback;

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
  void MarkElementDirty(const Node*);
  void MarkElementDirtyWithCleanLayout(const Node*);

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
  // LayoutObject and AbstractInlineTextBox are not on the Oilpan heap so we
  // do not use HeapHashMap for those mappings.
  HeapHashMap<Member<AccessibleNode>, AXID> accessible_node_mapping_;
  HeapHashMap<Member<const LayoutObject>, AXID> layout_object_mapping_;
  HeapHashMap<Member<const Node>, AXID> node_object_mapping_;
  HashMap<AbstractInlineTextBox*, AXID> inline_text_box_object_mapping_;
  int modification_count_;

  // Used for a mock AXObject representing the message displayed in the
  // validation message bubble.
  // There can be only one of these per document with invalid form controls,
  // and it will always be related to the currently focused control.
  AXID validation_message_axid_;

  // The currently active aria-modal dialog element, if one has been computed,
  // null if otherwise. This is only ever computed on platforms that have the
  // AriaModalPrunesAXTree setting enabled, such as Mac.
  WeakMember<AXObject> active_aria_modal_dialog_;

  // If non-null, this is the node that the current aria-activedescendant caused
  // to have the selected state.
  WeakMember<Node> last_selected_from_active_descendant_;

  std::unique_ptr<AXRelationCache> relation_cache_;

  bool processing_deferred_events_ = false;
#if DCHECK_IS_ON()
  bool updating_layout_and_ax_ = false;
#endif

  // Verified when finalizing.
  bool has_been_disposed_ = false;

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

  // Get the currently focused Node element.
  Node* FocusedElement();

  // GetOrCreate the focusable AXObject for a specific Node.
  AXObject* GetOrCreateFocusedObjectFromNode(Node*);

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

  // Must be called an entire subtree of accessible objects are no longer valid.
  void RemoveAXObjectsInLayoutSubtree(AXObject* subtree, int depth);

  // Object for HTML validation alerts. Created at most once per object cache.
  AXObject* GetOrCreateValidationMessageObject();
  void RemoveValidationMessageObjectWithCleanLayout(Node* document);

  // Enqueue a callback to the given method to be run after layout is
  // complete.
  void DeferTreeUpdate(void (AXObjectCacheImpl::*method)(const Node*),
                       const Node* node);
  void DeferTreeUpdate(void (AXObjectCacheImpl::*method)(Node*), Node* node);
  void DeferTreeUpdate(
      void (AXObjectCacheImpl::*method)(Node* node,
                                        ax::mojom::blink::Event event),
      Node* node,
      ax::mojom::blink::Event event);
  void DeferTreeUpdate(void (AXObjectCacheImpl::*method)(const QualifiedName&,
                                                         Element* element),
                       const QualifiedName& attr_name,
                       Element* element);
  // Provide either a DOM node or AXObject. If both are provided, then they must
  // match, meaning that the AXObject's DOM node must equal the provided node.
  void DeferTreeUpdate(void (AXObjectCacheImpl::*method)(Node*, AXObject*),
                       AXObject* obj);

  void DeferTreeUpdateInternal(base::OnceClosure callback, const Node* node);
  void DeferTreeUpdateInternal(base::OnceClosure callback, AXObject* obj);

  void TextChangedWithCleanLayout(Node* node);
  void ChildrenChangedWithCleanLayout(Node* node);

  // If the presence of document markers changed for the given text node, then
  // call children changed.
  void HandleTextMarkerDataAddedWithCleanLayout(Node*);
  void HandleAttributeChangedWithCleanLayout(const QualifiedName& attr_name,
                                             Element* element);
  void HandleUseMapAttributeChangedWithCleanLayout(Element*);
  void HandleNameAttributeChangedWithCleanLayout(Element*);

  bool DoesEventListenerImpactIgnoredState(
      const AtomicString& event_type) const;
  void HandleEventSubscriptionChanged(const Node& node,
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
  AXObject* AncestorAriaModalDialog(Node* node);

  void FireTreeUpdatedEventImmediately(
      Document& document,
      ax::mojom::blink::EventFrom event_from,
      ax::mojom::blink::Action event_from_action,
      const BlinkAXEventIntentsSet& event_intents,
      base::OnceClosure callback);
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

  bool is_frozen_ = false;  // Used with Freeze(), Thaw() and IsFrozen() above.

  // Set of ID's of current AXObjects that need to be destroyed and recreated.
  HashSet<AXID> invalidated_ids_main_;
  HashSet<AXID> invalidated_ids_popup_;

  // If false, exposes the internal accessibility tree of a select pop-up
  // instead.
  static bool use_ax_menu_list_;

  GC_PLUGIN_IGNORE("https://crbug.com/1381979")
  mojo::Remote<mojom::blink::RenderAccessibilityHost>
      render_accessibility_host_;

  Member<BlinkAXTreeSource> ax_tree_source_;
  std::unique_ptr<ui::AXTreeSerializer<AXObject*>> ax_tree_serializer_;

  HeapDeque<Member<AXDirtyObject>> dirty_objects_;

  Deque<ui::AXEvent> pending_events_;

  FRIEND_TEST_ALL_PREFIXES(AccessibilityTest, PauseUpdatesAfterMaxNumberQueued);
  FRIEND_TEST_ALL_PREFIXES(AccessibilityTest, RemoveAXID);
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
