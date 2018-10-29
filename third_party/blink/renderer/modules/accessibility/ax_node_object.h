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
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class AXObjectCacheImpl;
class Element;
class HTMLLabelElement;
class Node;

class MODULES_EXPORT AXNodeObject : public AXObject {
 protected:
  AXNodeObject(Node*, AXObjectCacheImpl&);

 public:
  static AXNodeObject* Create(Node*, AXObjectCacheImpl&);
  ~AXNodeObject() override;
  void Trace(blink::Visitor*) override;

 protected:
  bool children_dirty_;
#if DCHECK_IS_ON()
  bool initialized_ = false;
#endif
  // The accessibility role, not taking ARIA into account.
  ax::mojom::Role native_role_;

  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;
  const AXObject* InheritsPresentationalRoleFrom() const override;
  ax::mojom::Role DetermineAccessibilityRole() override;
  virtual ax::mojom::Role NativeRoleIgnoringAria() const;
  void AlterSliderOrSpinButtonValue(bool increase);
  AXObject* ActiveDescendant() override;
  String AriaAccessibilityDescription() const;
  String AriaAutoComplete() const override;
  void AccessibilityChildrenFromAOMProperty(AOMRelationListProperty,
                                            AXObject::AXObjectVector&) const;

  bool HasContentEditableAttributeSet() const;
  bool IsTextControl() const override;
  AXObject* MenuButtonForMenu() const;
  AXObject* MenuButtonForMenuIfExists() const;
  Element* MenuItemElementForMenu() const;
  Element* MouseButtonListener() const;
  bool IsNativeCheckboxOrRadio() const;
  void SetNode(Node*);
  AXObject* CorrespondingControlForLabelElement() const;
  HTMLLabelElement* LabelElementContainer() const;

  //
  // Overridden from AXObject.
  //

  void Init() override;
  void Detach() override;
  bool IsDetached() const override { return !node_; }
  bool IsAXNodeObject() const final { return true; }

  // Check object role or purpose.
  bool IsAnchor() const final;
  bool IsControllingVideoElement() const;
  bool IsMultiline() const override;
  bool IsEditable() const override { return IsNativeTextControl(); }
  bool ComputeIsEditableRoot() const override;
  bool IsEmbeddedObject() const final;
  bool IsFieldset() const final;
  bool IsHeading() const final;
  bool IsHovered() const final;
  bool IsImage() const final;
  bool IsImageButton() const;
  bool IsInputImage() const final;
  bool IsLink() const override;
  bool IsInPageLinkTarget() const override;
  bool IsMenu() const final;
  bool IsMenuButton() const final;
  bool IsMeter() const final;
  bool IsMultiSelectable() const override;
  bool IsNativeImage() const;
  bool IsNativeTextControl() const final;
  bool IsNonNativeTextControl() const final;
  bool IsPasswordField() const final;
  bool IsProgressIndicator() const override;
  bool IsRichlyEditable() const override;
  bool IsSlider() const override;
  bool IsSpinButton() const override;
  bool IsNativeSlider() const override;
  bool IsNativeSpinButton() const override;
  bool IsMoveableSplitter() const override;

  // Check object state.
  bool IsClickable() const final;
  AccessibilityExpanded IsExpanded() const override;
  bool IsModal() const final;
  bool IsRequired() const final;
  bool IsControl() const override;
  AXRestriction Restriction() const override;

  // Properties of static elements.
  RGBA32 ColorValue() const final;
  bool CanvasHasFallbackContent() const final;
  int HeadingLevel() const final;
  unsigned HierarchicalLevel() const final;
  void Markers(Vector<DocumentMarker::MarkerType>&,
               Vector<AXRange>&) const override;
  AXObject* InPageLinkTarget() const override;
  AccessibilityOrientation Orientation() const override;
  AXObjectVector RadioButtonsInGroup() const override;
  static HeapVector<Member<HTMLInputElement>> FindAllRadioButtonsWithSameName(
      HTMLInputElement* radio_button);
  String GetText() const override;

  // Properties of interactive elements.
  ax::mojom::AriaCurrentState GetAriaCurrentState() const final;
  ax::mojom::InvalidState GetInvalidState() const final;
  // Only used when invalidState() returns InvalidStateOther.
  String AriaInvalidValue() const final;
  String ValueDescription() const override;
  bool ValueForRange(float* out_value) const override;
  bool MaxValueForRange(float* out_value) const override;
  bool MinValueForRange(float* out_value) const override;
  bool StepValueForRange(float* out_value) const override;
  String StringValue() const override;

  // ARIA attributes.
  ax::mojom::Role AriaRoleAttribute() const final;

  // AX name calculation.
  String GetName(ax::mojom::NameFrom&,
                 AXObjectVector* name_objects) const override;
  String TextAlternative(bool recursive,
                         bool in_aria_labelled_by_traversal,
                         AXObjectSet& visited,
                         ax::mojom::NameFrom&,
                         AXRelatedObjectVector*,
                         NameSources*) const override;
  String Description(ax::mojom::NameFrom,
                     ax::mojom::DescriptionFrom&,
                     AXObjectVector* description_objects) const override;
  String Description(ax::mojom::NameFrom,
                     ax::mojom::DescriptionFrom&,
                     DescriptionSources*,
                     AXRelatedObjectVector*) const override;
  String Placeholder(ax::mojom::NameFrom) const override;
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

  // DOM and Render tree access.
  Element* ActionElement() const override;
  Element* AnchorElement() const override;
  Document* GetDocument() const override;
  Node* GetNode() const override { return node_; }

  // Modify or take an action on an object.
  bool OnNativeFocusAction() final;
  bool OnNativeIncrementAction() final;
  bool OnNativeDecrementAction() final;
  bool OnNativeSetSequentialFocusNavigationStartingPointAction() final;

  // Notifications that this object may have changed.
  void ChildrenChanged() override;
  void SelectionChanged() final;
  void TextChanged() override;

  // Position in set and Size of set
  int PosInSet() const override;
  int SetSize() const override;
  // Compute the number of siblings that have the same role before |this|,
  // following rules for counting the number of items in a set.
  int AutoPosInSet() const;
  // Compute the number of unignored siblings with the same role as |this|.
  int AutoSetSize() const;

  // Aria-owns.
  void ComputeAriaOwnsChildren(
      HeapVector<Member<AXObject>>& owned_children) const;

 private:
  Member<Node> node_;

  bool IsNativeCheckboxInMixedState() const;
  String TextFromDescendants(AXObjectSet& visited,
                             bool recursive) const override;
  String NativeTextAlternative(AXObjectSet& visited,
                               ax::mojom::NameFrom&,
                               AXRelatedObjectVector*,
                               NameSources*,
                               bool* found_text_alternative) const;
  bool IsDescendantOfElementType(HashSet<QualifiedName>& tag_names) const;
  String PlaceholderFromNativeAttribute() const;

  DISALLOW_COPY_AND_ASSIGN(AXNodeObject);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_NODE_OBJECT_H_
