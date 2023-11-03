/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_LAYOUT_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_LAYOUT_OBJECT_H_

#include "third_party/blink/renderer/modules/accessibility/ax_node_object.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace gfx {
class Point;
}

namespace blink {

class AXObjectCacheImpl;
class HTMLAreaElement;
class Node;

class MODULES_EXPORT AXLayoutObject : public AXNodeObject {
 public:
  AXLayoutObject(LayoutObject*, AXObjectCacheImpl&);

  AXLayoutObject(const AXLayoutObject&) = delete;
  AXLayoutObject& operator=(const AXLayoutObject&) = delete;

  ~AXLayoutObject() override;
  void Trace(Visitor*) const override;

  // AXObject overrides:
  LayoutObject* GetLayoutObject() const final;
  ScrollableArea* GetScrollableAreaIfScrollable() const final;

  // If this is an anonymous node, returns the node of its containing layout
  // block, otherwise returns the node of this layout object.
  Node* GetNodeOrContainingBlockNode() const;

  // DOM and layout tree access.
  Document* GetDocument() const override;

 protected:
  Member<LayoutObject> layout_object_;

  //
  // Overridden from AXObject.
  //

  void Detach() override;
  bool IsAXLayoutObject() const final;

  // Check object role or purpose.
  bool IsLinked() const override;
  bool IsOffScreen() const override;
  bool IsVisited() const override;

  // Check object state.
  bool IsNotUserSelectable() const override;

  // Whether objects are ignored, i.e. not included in the tree.
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;

  // Properties of static elements.
  ax::mojom::blink::ListStyle GetListStyle() const final;
  // Inline text boxes.
  AXObject* NextOnLine() const override;
  AXObject* PreviousOnLine() const override;

  // AX name calc.
  String TextAlternative(bool recursive,
                         const AXObject* aria_label_or_description_root,
                         AXObjectSet& visited,
                         ax::mojom::blink::NameFrom&,
                         AXRelatedObjectVector*,
                         NameSources*) const override;

  // Hit testing.
  AXObject* AccessibilityHitTest(const gfx::Point&) const override;

  // Called when autofill/autocomplete suggestion availability changes on a form
  // control.
  void HandleAutofillSuggestionAvailabilityChanged(
      WebAXAutofillSuggestionAvailability suggestion_availability) override;

  // For a table.
  unsigned ColumnCount() const override;
  unsigned RowCount() const override;
  void ColumnHeaders(AXObjectVector&) const override;
  void RowHeaders(AXObjectVector&) const override;
  AXObject* CellForColumnAndRow(unsigned column, unsigned row) const override;

  // For a table cell.
  unsigned ColumnIndex() const override;
  unsigned RowIndex() const override;  // Also for a table row.
  unsigned ColumnSpan() const override;
  unsigned RowSpan() const override;
  ax::mojom::blink::SortDirection GetSortDirection() const override;

  // For a table row or column.
  AXObject* HeaderObject() const override;

  // For a list marker.
  void GetWordBoundaries(Vector<int>& word_starts,
                         Vector<int>& word_ends) const override;

  //
  // Layout object specific methods.
  //
  // These methods may eventually migrate over to AXNodeObject.
  //

  // If we can't determine a useful role from the DOM node, attempt to determine
  // a role from the layout object.
  ax::mojom::blink::Role RoleFromLayoutObjectOrNode() const override;

 private:
  AXObject* AccessibilityImageMapHitTest(HTMLAreaElement*,
                                         const gfx::Point&) const;
  bool FindAllTableCellsWithRole(ax::mojom::blink::Role, AXObjectVector&) const;

  bool IsPlaceholder() const;
};

template <>
struct DowncastTraits<AXLayoutObject> {
  static bool AllowFrom(const AXObject& object) {
    return object.IsAXLayoutObject();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_LAYOUT_OBJECT_H_
