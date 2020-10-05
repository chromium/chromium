/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_NODE_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_NODE_OBJECT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class AXObjectCacheImpl;
class AXSVGRoot;
class Element;
class HTMLLabelElement;
class Node;

class MODULES_EXPORT AXNodeObject : public AXObject {
 public:
  AXNodeObject(Node*, AXObjectCacheImpl&);
  ~AXNodeObject() override;
  void Trace(Visitor*) const override;

 protected:
  bool children_dirty_;
#if DCHECK_IS_ON()
  bool initialized_ = false;
#endif
  // The accessibility role, not taking ARIA into account.
  ax::mojom::blink::Role native_role_;

  static base::Optional<String> GetCSSAltText(Node*);
  AXObjectInclusion ShouldIncludeBasedOnSemantics(
      IgnoredReasons* = nullptr) const;
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;
  const AXObject* InheritsPresentationalRoleFrom() const override;
  ax::mojom::blink::Role DetermineTableSectionRole() const;
  ax::mojom::blink::Role DetermineTableCellRole() const;
  ax::mojom::blink::Role DetermineTableRowRole() const;
  ax::mojom::blink::Role DetermineAccessibilityRole() override;
  virtual ax::mojom::blink::Role NativeRoleIgnoringAria() const;
  void AlterSliderOrSpinButtonValue(bool increase);
  AXObject* ActiveDescendant() override;
  String AriaAccessibilityDescription() const;
  String AutoComplete() const override;
  void AccessibilityChildrenFromAOMProperty(AOMRelationListProperty,
                                            AXObject::AXObjectVector&) const;

  bool HasContentEditableAttributeSet() const;
  bool IsTextControl() const override;
  Element* MenuItemElementForMenu() const;
  Element* MouseButtonListener() const;
  bool IsNativeCheckboxOrRadio() const;
  void SetNode(Node*);
  AXObject* CorrespondingControlAXObjectForLabelElement() const;
  AXObject* CorrespondingLabelAXObject() const;
  HTMLLabelElement* LabelElementContainer() const;

  //
  // Overridden from AXObject.
  //

  void Init() override;
  void Detach() override;
  bool IsDetached() const override;
  bool IsAXNodeObject() const final;

  // Check object role or purpose.
  bool IsControllingVideoElement() const;
  bool IsDefault() const final;
  bool IsMultiline() const override;
  bool IsEditable() const override { return IsNativeTextControl(); }
  bool ComputeIsEditableRoot() const override;
  bool IsFieldset() const final;
  bool IsHovered() const final;
  bool IsImageButton() const;
  bool IsInputImage() const final;
  bool IsInPageLinkTarget() const override;
  bool IsLoaded() const override;
  bool IsMultiSelectable() const override;
  bool IsNativeImage() const final;
  bool IsNativeTextControl() const final;
  bool IsNonNativeTextControl() const final;
  bool IsOffScreen() const override;
  bool IsPasswordField() const final;
  bool IsProgressIndicator() const override;
  bool IsRichlyEditable() const override;
  bool IsSlider() const override;
  bool IsSpinButton() const override;
  bool IsNativeSlider() const override;
  bool IsNativeSpinButton() const override;

  // Check object state.
  bool IsClickable() const final;
  AccessibilityExpanded IsExpanded() const override;
  bool IsModal() const final;
  bool IsRequired() const final;
  bool IsControl() const override;
  AXRestriction Restriction() const override;

  // Properties of static elements.
  const AtomicString& AccessKey() const override;
  RGBA32 ColorValue() const final;
  RGBA32 ComputeBackgroundColor() const final;
  RGBA32 GetColor() const final;
  String FontFamily() const final;
  // Font size is in pixels.
  float FontSize() const final;
  float FontWeight() const final;
  bool CanvasHasFallbackContent() const final;
  int HeadingLevel() const final;
  unsigned HierarchicalLevel() const final;
  void GetDocumentMarkers(Vector<DocumentMarker::MarkerType>* marker_types,
                          Vector<AXRange>* marker_ranges) const override;
  AXObject* InPageLinkTarget() const override;
  AccessibilityOrientation Orientation() const override;
  AXObjectVector RadioButtonsInGroup() const override;
  static HeapVector<Member<HTMLInputElement>> FindAllRadioButtonsWithSameName(
      HTMLInputElement* radio_button);
  String GetText() const override;
  String ImageDataUrl(const IntSize& max_size) const final;
  int TextLength() const override;
  int TextOffsetInFormattingContext(int offset) const override;

  // Object attributes.
  ax::mojom::blink::TextAlign GetTextAlign() const final;
  float GetTextIndent() const final;

  // Properties of interactive elements.
  ax::mojom::blink::AriaCurrentState GetAriaCurrentState() const final;
  ax::mojom::blink::InvalidState GetInvalidState() const final;
  // Only used when invalidState() returns InvalidStateOther.
  String AriaInvalidValue() const final;
  String ValueDescription() const override;
  bool ValueForRange(float* out_value) const override;
  bool MaxValueForRange(float* out_value) const override;
  bool MinValueForRange(float* out_value) const override;
  bool StepValueForRange(float* out_value) const override;
  KURL Url() const override;
  AXObject* ChooserPopup() const override;
  String StringValue() const override;
  String TextFromDescendants(AXObjectSet& visited,
                             bool recursive) const override;

  // ARIA attributes.
  ax::mojom::blink::Role AriaRoleAttribute() const final;
  bool HasAriaAttribute() const override;
  void AriaDescribedbyElements(AXObjectVector&) const override;
  void AriaOwnsElements(AXObjectVector&) const override;
  bool SupportsARIAOwns() const override;
  bool SupportsARIADragging() const override;
  void Dropeffects(
      Vector<ax::mojom::blink::Dropeffect>& dropeffects) const override;

  // ARIA live-region features.
  const AtomicString& LiveRegionStatus() const override;
  const AtomicString& LiveRegionRelevant() const override;

  ax::mojom::blink::HasPopup HasPopup() const override;

  // AX name calculation.
  String GetName(ax::mojom::blink::NameFrom&,
                 AXObjectVector* name_objects) const override;
  String TextAlternative(bool recursive,
                         bool in_aria_labelled_by_traversal,
                         AXObjectSet& visited,
                         ax::mojom::blink::NameFrom&,
                         AXRelatedObjectVector*,
                         NameSources*) const override;
  String Description(ax::mojom::blink::NameFrom,
                     ax::mojom::blink::DescriptionFrom&,
                     AXObjectVector* description_objects) const override;
  String Description(ax::mojom::blink::NameFrom,
                     ax::mojom::blink::DescriptionFrom&,
                     DescriptionSources*,
                     AXRelatedObjectVector*) const override;
  String Placeholder(ax::mojom::blink::NameFrom) const override;
  String Title(ax::mojom::blink::NameFrom) const override;
  bool NameFromLabelElement() const override;

  // Location
  void GetRelativeBounds(AXObject** out_container,
                         FloatRect& out_bounds_in_container,
                         SkMatrix44& out_container_transform,
                         bool* clips_children = nullptr) const override;

  // High-level accessibility tree access.
  AXObject* ComputeParent() const override;
  AXObject* ComputeParentIfExists() const override;

  // Low-level accessibility tree exploration.
  AXObject* RawFirstChild() const override;
  AXObject* RawNextSibling() const override;
  void AddChildren() override;

  bool CanHaveChildren() const override;
  void AddChild(AXObject*);
  void InsertChild(AXObject*, unsigned index);
  void ClearChildren() override;
  bool NeedsToUpdateChildren() const override { return children_dirty_; }
  void SetNeedsToUpdateChildren() override { children_dirty_ = true; }
  void UpdateChildrenIfNecessary() override;
  void SelectedOptions(AXObjectVector&) const override;

  // Properties of the object's owning document or page.
  double EstimatedLoadingProgress() const override;

  // DOM and Render tree access.
  Element* ActionElement() const override;
  Element* AnchorElement() const override;
  Document* GetDocument() const override;
  Node* GetNode() const override { return node_; }

  // DOM and layout tree access.
  AtomicString Language() const override;

  // Modify or take an action on an object.
  bool OnNativeFocusAction() final;
  bool OnNativeIncrementAction() final;
  bool OnNativeDecrementAction() final;
  bool OnNativeSetSequentialFocusNavigationStartingPointAction() final;

  // Notifications that this object may have changed.
  void ChildrenChanged() override;
  void SelectionChanged() final;

  // The aria-errormessage object or native object from a validationMessage
  // alert.
  AXObject* ErrorMessage() const override;

  // Position in set and Size of set
  int PosInSet() const override;
  int SetSize() const override;

  // Aria-owns.
  void ComputeAriaOwnsChildren(
      HeapVector<Member<AXObject>>& owned_children) const;

  // Inline text boxes.
  void LoadInlineTextBoxes() override;

  // SVG.
  bool IsSVGImage() const { return RemoteSVGRootElement(); }
  AXSVGRoot* RemoteSVGRootElement() const;

  virtual LayoutBoxModelObject* GetLayoutBoxModelObject() const {
    return nullptr;
  }

  //
  // Layout object specific methods.
  //

  // If we can't determine a useful role from the DOM node, attempt to determine
  // a role from the layout object.
  virtual ax::mojom::blink::Role RoleFromLayoutObject(
      ax::mojom::blink::Role dom_role) const {
    return dom_role;
  }

  FRIEND_TEST_ALL_PREFIXES(AccessibilityTest, SetNeedsToUpdateChildren);
  FRIEND_TEST_ALL_PREFIXES(AccessibilityTest, UpdateChildrenIfNecessary);

 private:
  Member<Node> node_;

  bool IsNativeCheckboxInMixedState() const;
  String NativeTextAlternative(AXObjectSet& visited,
                               ax::mojom::blink::NameFrom&,
                               AXRelatedObjectVector*,
                               NameSources*,
                               bool* found_text_alternative) const;
  bool IsDescendantOfElementType(HashSet<QualifiedName>& tag_names) const;
  String PlaceholderFromNativeAttribute() const;

  void AddInlineTextBoxChildren(bool force);
  void AddImageMapChildren();
  void AddHiddenChildren();
  void AddPopupChildren();
  void AddRemoteSVGChildren();
  void AddTableChildren();
  void AddValidationMessageChild();
  // For some nodes, only LayoutBuilderTraversal visits the necessary children.
  bool ShouldUseLayoutBuilderTraversal() const;
  ax::mojom::blink::Dropeffect ParseDropeffect(String& dropeffect) const;

  DISALLOW_COPY_AND_ASSIGN(AXNodeObject);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_NODE_OBJECT_H_
