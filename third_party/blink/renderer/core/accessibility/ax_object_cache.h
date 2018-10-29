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

#include "base/macros.h"
#include "third_party/blink/renderer/core/accessibility/axid.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

class AbstractInlineTextBox;
class AccessibleNode;
class HTMLCanvasElement;
class HTMLOptionElement;
class HTMLSelectElement;
class LayoutMenuList;
class LineLayoutItem;
class LocalFrameView;

class CORE_EXPORT AXObjectCache
    : public GarbageCollectedFinalized<AXObjectCache>,
      public ContextLifecycleObserver {
  USING_GARBAGE_COLLECTED_MIXIN(AXObjectCache);

 public:
  static AXObjectCache* Create(Document&);

  virtual ~AXObjectCache();
  void Trace(blink::Visitor*) override;

  virtual void Dispose() = 0;

  virtual void SelectionChanged(Node*) = 0;
  virtual void ChildrenChanged(Node*) = 0;
  virtual void ChildrenChanged(LayoutObject*) = 0;
  virtual void ChildrenChanged(AccessibleNode*) = 0;
  virtual void CheckedStateChanged(Node*) = 0;
  virtual void ListboxOptionStateChanged(HTMLOptionElement*) = 0;
  virtual void ListboxSelectedChildrenChanged(HTMLSelectElement*) = 0;
  virtual void ListboxActiveIndexChanged(HTMLSelectElement*) = 0;
  virtual void LocationChanged(LayoutObject*) = 0;
  virtual void RadiobuttonRemovedFromGroup(HTMLInputElement*) = 0;

  virtual void Remove(AccessibleNode*) = 0;
  virtual void Remove(LayoutObject*) = 0;
  virtual void Remove(Node*) = 0;
  virtual void Remove(AbstractInlineTextBox*) = 0;

  virtual const Element* RootAXEditableElement(const Node*) = 0;

  // Called by a node when text or a text equivalent (e.g. alt) attribute is
  // changed.
  virtual void TextChanged(LayoutObject*) = 0;
  virtual void DocumentTitleChanged() = 0;
  // Called when a node has just been attached, so we can make sure we have the
  // right subclass of AXObject.
  virtual void UpdateCacheAfterNodeIsAttached(Node*) = 0;
  virtual void DidInsertChildrenOfNode(Node*) = 0;

  virtual void HandleAttributeChanged(const QualifiedName& attr_name,
                                      Element*) = 0;
  virtual void HandleFocusedUIElementChanged(Node* old_focused_node,
                                             Node* new_focused_node) = 0;
  virtual void HandleInitialFocus() = 0;
  virtual void HandleEditableTextContentChanged(Node*) = 0;
  virtual void HandleScaleAndLocationChanged(Document*) = 0;
  virtual void HandleTextMarkerDataAdded(Node* start, Node* end) = 0;
  virtual void HandleTextFormControlChanged(Node*) = 0;
  virtual void HandleValueChanged(Node*) = 0;
  virtual void HandleUpdateActiveMenuOption(LayoutMenuList*,
                                            int option_index) = 0;
  virtual void DidShowMenuListPopup(LayoutMenuList*) = 0;
  virtual void DidHideMenuListPopup(LayoutMenuList*) = 0;
  virtual void HandleLoadComplete(Document*) = 0;
  virtual void HandleLayoutComplete(Document*) = 0;
  virtual void HandleClicked(Node*) = 0;
  virtual void HandleAutofillStateChanged(Element*, bool) = 0;

  // Handle any notifications which arrived while layout was dirty.
  virtual void ProcessUpdatesAfterLayout(Document&) = 0;

  // Changes to virtual Accessibility Object Model nodes.
  virtual void HandleAttributeChanged(const QualifiedName& attr_name,
                                      AccessibleNode*) = 0;

  virtual void SetCanvasObjectBounds(HTMLCanvasElement*,
                                     Element*,
                                     const LayoutRect&) = 0;

  virtual void InlineTextBoxesUpdated(LineLayoutItem) = 0;

  // Called when the scroll offset changes.
  virtual void HandleScrollPositionChanged(LocalFrameView*) = 0;
  virtual void HandleScrollPositionChanged(LayoutObject*) = 0;

  // Called when scroll bars are added / removed (as the view resizes).
  virtual void HandleLayoutComplete(LayoutObject*) = 0;
  virtual void HandleScrolledToAnchor(const Node* anchor_node) = 0;

  virtual const AtomicString& ComputedRoleForNode(Node*) = 0;
  virtual String ComputedNameForNode(Node*) = 0;

  virtual void OnTouchAccessibilityHover(const IntPoint&) = 0;

  virtual AXID GetAXID(Node*) = 0;
  virtual Element* GetElementFromAXID(AXID) = 0;

  typedef AXObjectCache* (*AXObjectCacheCreateFunction)(Document&);
  static void Init(AXObjectCacheCreateFunction);

  // Static helper functions.
  static bool IsInsideFocusableElementOrARIAWidget(const Node&);

 protected:
  AXObjectCache(Document&);

 private:
  static AXObjectCacheCreateFunction create_function_;
  DISALLOW_COPY_AND_ASSIGN(AXObjectCache);
};

}  // namespace blink

#endif
