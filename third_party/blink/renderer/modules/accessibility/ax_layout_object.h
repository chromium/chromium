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

#include "base/macros.h"
#include "third_party/blink/renderer/modules/accessibility/ax_node_object.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"

namespace blink {

class AXObjectCacheImpl;
class AXSVGRoot;
class Element;
class HTMLAreaElement;
class IntPoint;
class LocalFrameView;
class Node;

class MODULES_EXPORT AXLayoutObject : public AXNodeObject {
 public:
  AXLayoutObject(LayoutObject*, AXObjectCacheImpl&);
  ~AXLayoutObject() override;

  // Public, overridden from AXObject.
  LayoutObject* GetLayoutObject() const final { return layout_object_; }
  LayoutBoxModelObject* GetLayoutBoxModelObject() const;
  ScrollableArea* GetScrollableAreaIfScrollable() const final;
  ax::mojom::Role DetermineAccessibilityRole() override;
  ax::mojom::Role NativeRoleIgnoringAria() const override;

  // If this is an anonymous node, returns the node of its containing layout
  // block, otherwise returns the node of this layout object.
  Node* GetNodeOrContainingBlockNode() const;

 protected:
  LayoutObject* layout_object_;

  LayoutObject* LayoutObjectForRelativeBounds() const override {
    return layout_object_;
  }

  //
  // Overridden from AXObject.
  //

  void Init() override;
  void Detach() override;
  bool IsDetached() const override { return !layout_object_; }
  bool IsAXLayoutObject() const override { return true; }

  // Check object role or purpose.
  bool IsAutofillAvailable() const override;
  bool IsDefault() const override;
  bool IsEditable() const override;
  bool IsRichlyEditable() const override;
  bool IsLineBreakingObject() const override;
  bool IsLinked() const override;
  bool IsLoaded() const override;
  bool IsOffScreen() const override;
  bool IsVisited() const override;

  // Check object state.
  bool IsFocused() const override;
  // aria-grabbed is deprecated in WAI-ARIA 1.1.
  AccessibilityGrabbedState IsGrabbed() const override;
  AccessibilitySelectedState IsSelected() const override;
  bool IsSelectedFromFocus() const override;

  // Whether objects are ignored, i.e. not included in the tree.
  AXObjectInclusion DefaultObjectInclusion(
      IgnoredReasons* = nullptr) const override;
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;

  // Properties of static elements.
  const AtomicString& AccessKey() const override;
  RGBA32 ComputeBackgroundColor() const final;
  RGBA32 GetColor() const final;
  String FontFamily() const final;
  // Font size is in pixels.
  float FontSize() const final;
  float FontWeight() const final;
  String ImageDataUrl(const IntSize& max_size) const final;
  ax::mojom::ListStyle GetListStyle() const final;
  String GetText() const override;
  ax::mojom::TextDirection GetTextDirection() const final;
  ax::mojom::TextPosition GetTextPosition() const final;
  int TextLength() const override;
  void GetTextStyleAndTextDecorationStyle(
      int32_t* text_style,
      ax::mojom::TextDecorationStyle* text_overline_style,
      ax::mojom::TextDecorationStyle* text_strikethrough_style,
      ax::mojom::TextDecorationStyle* text_underline_style) const final;

  // Inline text boxes.
  void LoadInlineTextBoxes() override;
  AXObject* NextOnLine() const override;
  AXObject* PreviousOnLine() const override;

  // Properties of interactive elements.
  String StringValue() const override;

  // ARIA attributes.
  void AriaDescribedbyElements(AXObjectVector&) const override;
  void AriaOwnsElements(AXObjectVector&) const override;

  ax::mojom::HasPopup HasPopup() const override;
  bool SupportsARIADragging() const override;
  void Dropeffects(Vector<ax::mojom::Dropeffect>& dropeffects) const override;
  bool SupportsARIAFlowTo() const override;
  bool SupportsARIAOwns() const override;

  // ARIA live-region features.
  const AtomicString& LiveRegionStatus() const override;
  const AtomicString& LiveRegionRelevant() const override;

  // AX name calc.
  String TextAlternative(bool recursive,
                         bool in_aria_labelled_by_traversal,
                         AXObjectSet& visited,
                         ax::mojom::NameFrom&,
                         AXRelatedObjectVector*,
                         NameSources*) const override;

  // Modify or take an action on an object.
  bool OnNativeSetValueAction(const String&) override;

  // Hit testing.
  AXObject* AccessibilityHitTest(const IntPoint&) const override;
  AXObject* ElementAccessibilityHitTest(const IntPoint&) const override;

  // High-level accessibility tree access. Other modules should only use these
  // functions.
  AXObject* ComputeParent() const override;
  AXObject* ComputeParentIfExists() const override;

  // Low-level accessibility tree exploration, only for use within the
  // accessibility module.
  AXObject* RawFirstChild() const override;
  AXObject* RawNextSibling() const override;
  void AddChildren() override;
  void AddListMarker() override;
  void AddInlineTextBoxChildren(bool force) override;
  void AddImageMapChildren() override;
  void AddHiddenChildren() override;
  void AddPopupChildren() override;
  bool CanHaveChildren() const override;

  // Properties of the object's owning document or page.
  double EstimatedLoadingProgress() const override;

  // DOM and layout tree access.
  Node* GetNode() const override;
  Document* GetDocument() const override;
  LocalFrameView* DocumentFrameView() const override;
  Element* AnchorElement() const override;
  AtomicString Language() const override;

  // Notifications that this object may have changed.
  void HandleActiveDescendantChanged() override;
  void HandleAriaExpandedChanged() override;
  // Called when autofill/autocomplete state changes on a form control.
  void HandleAutofillStateChanged(WebAXAutofillState state) override;
  void TextChanged() override;

  // For a table.
  bool IsDataTable() const override;
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
  ax::mojom::SortDirection GetSortDirection() const override;

  // For a table row or column.
  AXObject* HeaderObject() const override;

  // The aria-errormessage object or native object from a validationMessage
  // alert.
  AXObject* ErrorMessage() const override;

 private:
  bool IsTabItemSelected() const;
  AXObject* AccessibilityImageMapHitTest(HTMLAreaElement*,
                                         const IntPoint&) const;
  bool IsSVGImage() const;
  void DetachRemoteSVGRoot();
  AXSVGRoot* RemoteSVGRootElement() const;
  AXObject* RemoteSVGElementHitTest(const IntPoint&) const;
  void OffsetBoundingBoxForRemoteSVGElement(LayoutRect&) const;
  void AddRemoteSVGChildren();
  void AddTableChildren();
  void AddValidationMessageChild();
  bool FindAllTableCellsWithRole(ax::mojom::Role, AXObjectVector&) const;

  LayoutRect ComputeElementRect() const;
  bool CanIgnoreTextAsEmpty() const;
  bool CanIgnoreSpaceNextTo(LayoutObject*, bool is_after) const;
  bool HasAriaCellRole(Element*) const;
  bool IsPlaceholder() const;
  ax::mojom::Dropeffect ParseDropeffect(String& dropeffect) const;

  static ax::mojom::TextDecorationStyle
  TextDecorationStyleToAXTextDecorationStyle(
      const ETextDecorationStyle text_decoration_style);

  DISALLOW_COPY_AND_ASSIGN(AXLayoutObject);
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXLayoutObject, IsAXLayoutObject());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_LAYOUT_OBJECT_H_
