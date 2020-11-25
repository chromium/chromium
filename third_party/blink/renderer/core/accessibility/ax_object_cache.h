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

#include <memory>

#include "third_party/blink/renderer/core/accessibility/axid.h"
#include "third_party/blink/renderer/core/accessibility/blink_ax_event_intent.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/wtf/hash_counted_set.h"

namespace blink {

class AbstractInlineTextBox;
class AccessibleNode;
class HTMLCanvasElement;
class HTMLOptionElement;
class HTMLFrameOwnerElement;
class HTMLSelectElement;
class IntPoint;
class LayoutRect;
class LineLayoutItem;
class LocalFrameView;

class CORE_EXPORT AXObjectCache : public GarbageCollected<AXObjectCache> {
 public:
  using BlinkAXEventIntentsSet = HashCountedSet<BlinkAXEventIntent,
                                                BlinkAXEventIntentHash,
                                                BlinkAXEventIntentHashTraits>;

  static AXObjectCache* Create(Document&);

  AXObjectCache(const AXObjectCache&) = delete;
  AXObjectCache& operator=(const AXObjectCache&) = delete;
  virtual ~AXObjectCache() = default;
  virtual void Trace(Visitor*) const {}

  virtual void Dispose() = 0;

  // A Freeze() occurs during a serialization run.
  // Used here as a hint for DCHECKS to enforce the following behavior:
  // objects in the ax hierarchy should not be destroyed during serialization.
  virtual void Freeze() = 0;
  virtual void Thaw() = 0;

  // Register/remove popups
  virtual void InitializePopup(Document* document) = 0;
  virtual void DisposePopup(Document* document) = 0;

  virtual void SelectionChanged(Node*) = 0;
  virtual void ChildrenChanged(Node*) = 0;
  virtual void ChildrenChanged(const LayoutObject*) = 0;
  virtual void ChildrenChanged(AccessibleNode*) = 0;
  virtual void CheckedStateChanged(Node*) = 0;
  virtual void ListboxOptionStateChanged(HTMLOptionElement*) = 0;
  virtual void ListboxSelectedChildrenChanged(HTMLSelectElement*) = 0;
  virtual void ListboxActiveIndexChanged(HTMLSelectElement*) = 0;
  virtual void LocationChanged(const LayoutObject*) = 0;
  virtual void ImageLoaded(const LayoutObject*) = 0;

  virtual void Remove(AccessibleNode*) = 0;
  virtual void Remove(LayoutObject*) = 0;
  virtual void Remove(Node*) = 0;
  virtual void Remove(AbstractInlineTextBox*) = 0;

  virtual const Element* RootAXEditableElement(const Node*) = 0;

  // Called when aspects of the style (e.g. color, alignment) change.
  virtual void StyleChanged(const LayoutObject*) = 0;

  // Called by a node when text or a text equivalent (e.g. alt) attribute is
  // changed.
  virtual void TextChanged(const LayoutObject*) = 0;
  virtual void DocumentTitleChanged() = 0;
  // Called when a node has just been attached, so we can make sure we have the
  // right subclass of AXObject.
  virtual void UpdateCacheAfterNodeIsAttached(Node*) = 0;
  virtual void DidInsertChildrenOfNode(Node*) = 0;

  // Returns true if the AXObjectCache cares about this attribute
  virtual void HandleAttributeChanged(const QualifiedName& attr_name,
                                      Element*) = 0;
  virtual void HandleFocusedUIElementChanged(Element* old_focused_node,
                                             Element* new_focused_node) = 0;
  virtual void HandleInitialFocus() = 0;
  virtual void HandleEditableTextContentChanged(Node*) = 0;
  virtual void HandleScaleAndLocationChanged(Document*) = 0;
  virtual void HandleTextMarkerDataAdded(Node* start, Node* end) = 0;
  virtual void HandleTextFormControlChanged(Node*) = 0;
  virtual void HandleValueChanged(Node*) = 0;
  virtual void HandleUpdateActiveMenuOption(LayoutObject*,
                                            int option_index) = 0;
  virtual void DidShowMenuListPopup(LayoutObject*) = 0;
  virtual void DidHideMenuListPopup(LayoutObject*) = 0;
  virtual void HandleLoadComplete(Document*) = 0;
  virtual void HandleLayoutComplete(Document*) = 0;
  virtual void HandleClicked(Node*) = 0;
  virtual void HandleValidationMessageVisibilityChanged(
      const Node* form_control) = 0;
  virtual void HandleEventListenerAdded(const Node& node,
                                        const AtomicString& event_type) = 0;
  virtual void HandleEventListenerRemoved(const Node& node,
                                          const AtomicString& event_type) = 0;

  // Handle any notifications which arrived while layout was dirty.
  virtual void ProcessDeferredAccessibilityEvents(Document&) = 0;

  // Changes to virtual Accessibility Object Model nodes.
  virtual void HandleAttributeChanged(const QualifiedName& attr_name,
                                      AccessibleNode*) = 0;

  // Called when a HTMLFrameOwnerElement (such as an iframe element) changes the
  // embedding token of its child frame.
  virtual void EmbeddingTokenChanged(HTMLFrameOwnerElement*) = 0;

  virtual void SetCanvasObjectBounds(HTMLCanvasElement*,
                                     Element*,
                                     const LayoutRect&) = 0;

  virtual void InlineTextBoxesUpdated(LineLayoutItem) = 0;

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

  virtual void OnTouchAccessibilityHover(const IntPoint&) = 0;

  virtual AXID GetAXID(Node*) = 0;
  virtual Element* GetElementFromAXID(AXID) = 0;

  typedef AXObjectCache* (*AXObjectCacheCreateFunction)(Document&);
  static void Init(AXObjectCacheCreateFunction);

  // Static helper functions.
  static bool IsInsideFocusableElementOrARIAWidget(const Node&);

  // Returns true if there are any pending updates that need processing.
  virtual bool IsDirty() const = 0;

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

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ACCESSIBILITY_AX_OBJECT_CACHE_H_
