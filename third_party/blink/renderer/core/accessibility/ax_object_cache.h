/*
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ACCESSIBILITY_AX_OBJECT_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ACCESSIBILITY_AX_OBJECT_CACHE_H_

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/core/accessibility/axid.h"
#include "third_party/blink/renderer/core/accessibility/blink_ax_event_intent.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/wtf/hash_counted_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace gfx {
class Point;
}

namespace ui {
class AXMode;
struct AXTreeUpdate;
}

namespace blink {

class AbstractInlineTextBox;
class AriaNotificationOptions;
class AXObject;
class AccessibleNode;
class HTMLCanvasElement;
class HTMLOptionElement;
class HTMLTableElement;
class HTMLFrameOwnerElement;
class HTMLSelectElement;
class LocalFrameView;
struct PhysicalRect;

class CORE_EXPORT AXObjectCache : public GarbageCollected<AXObjectCache> {
 public:
  using BlinkAXEventIntentsSet =
      HashCountedSet<BlinkAXEventIntent, BlinkAXEventIntentHashTraits>;

  static AXObjectCache* Create(Document&, const ui::AXMode&);

  AXObjectCache(const AXObjectCache&) = delete;
  AXObjectCache& operator=(const AXObjectCache&) = delete;
  virtual ~AXObjectCache() = default;
  virtual void Trace(Visitor*) const {}

  virtual void Dispose() = 0;

  virtual const ui::AXMode& GetAXMode() = 0;
  virtual void SetAXMode(const ui::AXMode&) = 0;

  // A Freeze() occurs during a serialization run.
  virtual void Freeze() = 0;
  virtual void Thaw() = 0;
  virtual bool IsFrozen() const = 0;

  // Ensure that accessibility is clean and up-to-date for both the main and
  // popup document. Ensures layout is clean as well.
  virtual void UpdateAXForAllDocuments() = 0;

  virtual void SelectionChanged(Node*) = 0;
  virtual void ChildrenChanged(Node*) = 0;
  virtual void ChildrenChanged(const LayoutObject*) = 0;
  virtual void ChildrenChanged(AccessibleNode*) = 0;
  virtual void SlotAssignmentWillChange(Node*) = 0;
  virtual void CheckedStateChanged(Node*) = 0;
  virtual void ListboxOptionStateChanged(HTMLOptionElement*) = 0;
  virtual void ListboxSelectedChildrenChanged(HTMLSelectElement*) = 0;
  virtual void ListboxActiveIndexChanged(HTMLSelectElement*) = 0;
  virtual void SetMenuListOptionsBounds(HTMLSelectElement*,
                                        const WTF::Vector<gfx::Rect>&) = 0;
  virtual void LocationChanged(const LayoutObject*) = 0;
  virtual void ImageLoaded(const LayoutObject*) = 0;

  // Removes AXObject backed by passed-in object, if there is one.
  // Will also notify the parent that its children have changed, so that the
  // parent will recompute its children and be reserialized.
  virtual void Remove(AccessibleNode*) = 0;
  virtual void Remove(LayoutObject*) = 0;
  virtual void Remove(Node*) = 0;
  virtual void RemoveSubtreeWhenSafe(Node*, bool remove_root = true) = 0;
  virtual void RemovePopup(Document*) = 0;
  virtual void Remove(AbstractInlineTextBox*) = 0;

  virtual const Element* RootAXEditableElement(const Node*) = 0;

  // Called when aspects of the style (e.g. color, alignment) change.
  virtual void StyleChanged(const LayoutObject*,
                            bool visibility_or_inertness_changed = false) = 0;

  // Called by a node when text or a text equivalent (e.g. alt) attribute is
  // changed.
  virtual void TextChanged(const LayoutObject*) = 0;
  virtual void DocumentTitleChanged() = 0;
  // Called when a node is connected to the document.
  virtual void NodeIsConnected(Node*) = 0;
  // Called when a node is attached to the layout tree.
  virtual void NodeIsAttached(Node*) = 0;
  // Called when a subtree is attached to the layout tree because of
  // content-visibility or previously display:none content gaining layout.
  virtual void SubtreeIsAttached(Node*) = 0;

  // Called to process queued subtree removals when flat tree traversal is safe.
  virtual void ProcessSubtreeRemovals() = 0;
  // Returns true if the AXObjectCache cares about this attribute
  virtual void HandleAttributeChanged(const QualifiedName& attr_name,
                                      Element*) = 0;
  virtual void HandleFocusedUIElementChanged(Element* old_focused_node,
                                             Element* new_focused_node) = 0;
  virtual void HandleInitialFocus() = 0;
  virtual void HandleEditableTextContentChanged(Node*) = 0;
  virtual void HandleDeletionOrInsertionInTextField(
      const SelectionInDOMTree& changed_selection,
      bool is_deletion) = 0;
  virtual void HandleTextMarkerDataAdded(Node* start, Node* end) = 0;
  virtual void HandleTextFormControlChanged(Node*) = 0;
  virtual void HandleValueChanged(Node*) = 0;
  virtual void HandleUpdateActiveMenuOption(Node*) = 0;
  virtual void DidShowMenuListPopup(LayoutObject*) = 0;
  virtual void DidHideMenuListPopup(LayoutObject*) = 0;
  virtual void HandleLoadStart(Document*) = 0;
  virtual void HandleLoadComplete(Document*) = 0;
  virtual void HandleLayoutComplete(Document*) = 0;
  virtual void HandleClicked(Node*) = 0;
  virtual void HandleValidationMessageVisibilityChanged(Node* form_control) = 0;
  virtual void HandleEventListenerAdded(Node& node,
                                        const AtomicString& event_type) = 0;
  virtual void HandleEventListenerRemoved(Node& node,
                                          const AtomicString& event_type) = 0;

  // Handle any notifications which arrived while layout was dirty.
  // If |force|, then process regardless of any active batching or pauses.
  virtual void ProcessDeferredAccessibilityEvents(Document&,
                                                  bool force = false) = 0;

  // Changes to virtual Accessibility Object Model nodes.
  virtual void HandleAttributeChanged(const QualifiedName& attr_name,
                                      AccessibleNode*) = 0;

  // Called when the DOM parser has reached the closing tag of a table element.
  virtual void FinishedParsingTable(HTMLTableElement*) = 0;

  // Called when a HTMLFrameOwnerElement (such as an iframe element) changes the
  // embedding token of its child frame.
  virtual void EmbeddingTokenChanged(HTMLFrameOwnerElement*) = 0;

  virtual void SetCanvasObjectBounds(HTMLCanvasElement*,
                                     Element*,
                                     const PhysicalRect&) = 0;

  virtual void InlineTextBoxesUpdated(LayoutObject*) = 0;

  // Called when the scroll offset changes.
  virtual void HandleScrollPositionChanged(LocalFrameView*) = 0;
  virtual void HandleScrollPositionChanged(LayoutObject*) = 0;

  virtual void HandleScrolledToAnchor(const Node* anchor_node) = 0;

  // Called when the frame rect changes, which can sometimes happen
  // without producing any layout or other notifications.
  virtual void HandleFrameRectsChanged(Document&) = 0;

  // Called when a layout object's bounding box may have changed.
  virtual void InvalidateBoundingBox(const LayoutObject*) = 0;

  virtual const AtomicString& ComputedRoleForNode(Node*) = 0;
  virtual String ComputedNameForNode(Node*) = 0;

  virtual void OnTouchAccessibilityHover(const gfx::Point&) = 0;

  // Gets the AXID of the given Node. If there is not yet an associated
  // AXObject, this method will create one (along with its parent chain) and
  // return the id.
  virtual AXID GetAXID(Node*) = 0;

  // Crash dumps have shown there are times where AXID's are queried
  // 'out-of-band' where we may not be in a state where creating AXObjects and
  // repairing parent chains is possible. This will look for the current
  // AXObject associated with the given node and return its id without creating
  // or recomputing any state.
  virtual AXID GetExistingAXID(Node*) = 0;

  virtual AXObject* ObjectFromAXID(AXID) const = 0;

  virtual AXObject* Root() = 0;

  virtual AXID GenerateAXID() const = 0;

  virtual void AddAriaNotification(Node*,
                                   const String,
                                   const AriaNotificationOptions*) = 0;

  typedef AXObjectCache* (*AXObjectCacheCreateFunction)(Document&,
                                                        const ui::AXMode&);
  static void Init(AXObjectCacheCreateFunction);

  // Static helper functions.
  static bool IsInsideFocusableElementOrARIAWidget(const Node&);

  // Returns true if there are any pending updates that need processing.
  virtual bool IsDirty() = 0;

  virtual void SerializeLocationChanges(uint32_t reset_token) = 0;

  virtual AXObject* GetPluginRoot() = 0;

  // Serialize entire tree, returning true if successful.
  virtual bool SerializeEntireTree(size_t max_node_count,
                                   base::TimeDelta timeout,
                                   ui::AXTreeUpdate*) = 0;

  // Recompute the entire tree and reserialize it.
  // This method is useful when something that potentially affects most of the
  // page occurs, such as an inertness change or a fullscreen toggle.
  // This keeps the existing nodes, but recomputes all of their properties and
  // reserializes everything.
  // Compared with ResetSerializer() and MarkAXObjectDirtyWithDetails() with
  // subtree = true, this does more work, because it recomputes the entire tree
  // structure and properties of each node.
  virtual void MarkDocumentDirty() = 0;

  // Compared with MarkDocumentDirty(), this does less work, because it assumes
  // the AXObjectCache's tree of objects and properties is correct, but needs to
  // be reserialized.
  virtual void ResetSerializer() = 0;

  virtual void MarkElementDirty(const Node*) = 0;

  // Notifies that an AXObject is dirty and its state needs
  // to be serialized again. If |subtree| is true, the entire subtree is
  // dirty.
  // |event_from| and |event_from_action| annotate this node change with info
  // about the event which caused the change. For example, an event from a user
  // or an event from a focus action.
  virtual void MarkAXObjectDirtyWithDetails(
      AXObject* obj,
      bool subtree,
      ax::mojom::blink::EventFrom event_from,
      ax::mojom::blink::Action event_from_action,
      const std::vector<ui::AXEventIntent>& event_intents) = 0;

  virtual void SerializeDirtyObjectsAndEvents(
      bool has_plugin_tree_source,
      std::vector<ui::AXTreeUpdate>& updates,
      std::vector<ui::AXEvent>& events,
      bool& had_end_of_test_event,
      bool& had_load_complete_messages,
      bool& need_to_send_location_changes) = 0;

  // Returns a vector of the images found in |updates|.
  virtual void GetImagesToAnnotate(ui::AXTreeUpdate& updates,
                                   std::vector<ui::AXNodeData*>&) = 0;

  // Note that any pending event also causes its corresponding object to
  // become dirty.
  virtual bool HasDirtyObjects() const = 0;

  // Adds the event to a list of pending events that is cleared out by
  // a subsequent call to  duplicates are not represented.. Returns false if
  // the event is already pending; duplicates are not represented.
  virtual bool AddPendingEvent(const ui::AXEvent& event,
                               bool insert_at_beginning) = 0;

  // Ensure that a call to ProcessDeferredAccessibilityEvents() will occur soon.
  virtual void ScheduleAXUpdate() const = 0;

 protected:
  friend class ScopedBlinkAXEventIntent;
  FRIEND_TEST_ALL_PREFIXES(ScopedBlinkAXEventIntentTest, SingleIntent);
  FRIEND_TEST_ALL_PREFIXES(ScopedBlinkAXEventIntentTest,
                           MultipleIdenticalIntents);
  FRIEND_TEST_ALL_PREFIXES(ScopedBlinkAXEventIntentTest,
                           NestedIndividualIntents);
  FRIEND_TEST_ALL_PREFIXES(ScopedBlinkAXEventIntentTest, NestedMultipleIntents);
  FRIEND_TEST_ALL_PREFIXES(ScopedBlinkAXEventIntentTest,
                           NestedIdenticalIntents);

  virtual BlinkAXEventIntentsSet& ActiveEventIntents() = 0;

 private:
  friend class AXObjectCacheBase;
  AXObjectCache() = default;

  static AXObjectCacheCreateFunction create_function_;
};

class ScopedFreezeAXCache : public GarbageCollected<ScopedFreezeAXCache> {
 public:
  explicit ScopedFreezeAXCache(AXObjectCache& cache) : cache_(&cache) {
    CHECK(!cache.IsFrozen());
    cache.Freeze();
  }

  ScopedFreezeAXCache(const ScopedFreezeAXCache&) = delete;
  ScopedFreezeAXCache& operator=(const ScopedFreezeAXCache&) = delete;

  ~ScopedFreezeAXCache() {
    CHECK(cache_);
    CHECK(cache_->IsFrozen());
    cache_->Thaw();
  }

  void Trace(Visitor* visitor) const { visitor->Trace(cache_); }

 private:
  WeakMember<AXObjectCache> cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ACCESSIBILITY_AX_OBJECT_CACHE_H_
