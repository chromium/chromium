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

#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class AXObjectCacheImpl;
class Element;
class HTMLElement;
class HTMLLabelElement;
class Node;

class MODULES_EXPORT AXNodeObject : public AXObject {
 public:
  AXNodeObject(Node*, AXObjectCacheImpl&);

  AXNodeObject(const AXNodeObject&) = delete;
  AXNodeObject& operator=(const AXNodeObject&) = delete;

  ~AXNodeObject() override;

  static absl::optional<String> GetCSSAltText(const Node*);

  void Trace(Visitor*) const override;

 protected:
#if DCHECK_IS_ON()
  bool initialized_ = false;
  mutable bool getting_bounds_ = false;
#endif

  // The accessibility role, not taking the ARIA role into account.
  ax::mojom::blink::Role native_role_;

  // The ARIA role, not taking the native role into account.
  ax::mojom::blink::Role aria_role_;

  AXObjectInclusion ShouldIncludeBasedOnSemantics(
      IgnoredReasons* = nullptr) const;
  bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;
  ax::mojom::blink::Role DetermineTableSectionRole() const;
  ax::mojom::blink::Role DetermineTableCellRole() const;
  ax::mojom::blink::Role DetermineTableRowRole() const;
  ax::mojom::blink::Role DetermineAccessibilityRole() override;
  ax::mojom::blink::Role NativeRoleIgnoringAria() const override;
  void AlterSliderOrSpinButtonValue(bool increase);
  AXObject* ActiveDescendant() override;
  String AriaAccessibilityDescription() const;
  String AutoComplete() const override;
  void AccessibilityChildrenFromAOMProperty(AOMRelationListProperty,
                                            AXObject::AXObjectVector&) const;

  Element* MenuItemElementForMenu() const;
  HTMLElement* CorrespondingControlForLabelElement() const;

  //
  // Overridden from AXObject.
  //

  void Init(AXObject* parent) override;
  void Detach() override;
  bool IsAXNodeObject() const final;

  // Check object role or purpose.
  bool IsAutofillAvailable() const override;
  bool IsDefault() const final;
  bool IsFieldset() const final;
  bool IsHovered() const final;
  bool IsImageButton() const;
  bool IsInputImage() const final;
  bool IsLineBreakingObject() const override;
  bool IsLoaded() const override;
  bool IsMultiSelectable() const override;
  bool IsNativeImage() const final;
  bool IsOffScreen() const override;
  bool IsProgressIndicator() const override;
  bool IsSlider() const override;
  bool IsSpinButton() const override;
  bool IsNativeSlider() const override;
  bool IsNativeSpinButton() const override;
  bool IsChildTreeOwner() const override;

  // Check object state.
  bool IsClickable() const final;
  bool IsFocused() const override;
  // aria-grabbed is deprecated in WAI-ARIA 1.1.
  AccessibilityGrabbedState IsGrabbed() const override;
  AccessibilityExpanded IsExpanded() const override;
  AccessibilitySelectedState IsSelected() const override;
  bool IsSelectedFromFocusSupported() const override;
  bool IsSelectedFromFocus() const override;
  bool IsRequired() const final;
  bool IsControl() const override;
  AXRestriction Restriction() const override;

  // Properties of static elements.
  const AtomicString& AccessKey() const override;
  RGBA32 ColorValue() const final;
  RGBA32 GetColor() const final;
  RGBA32 BackgroundColor() const override;
  const AtomicString& ComputedFontFamily() const final;
  String FontFamilyForSerialization() const final;
  // Font size is in pixels.
  float FontSize() const final;
  float FontWeight() const final;
  bool CanvasHasFallbackContent() const final;
  int HeadingLevel() const final;
  unsigned HierarchicalLevel() const final;
  void SerializeMarkerAttributes(ui::AXNodeData* node_data) const override;
  AXObject* InPageLinkTarget() const override;
  AccessibilityOrientation Orientation() const override;

  AXObject* GetChildFigcaption() const override;
  bool IsDescendantOfLandmarkDisallowedElement() const override;

  // Used to compute kRadioGroupIds, which is only used on Mac.
  // TODO(accessibility) Consider computing on browser side and removing here.
  AXObjectVector RadioButtonsInGroup() const override;
  static HeapVector<Member<HTMLInputElement>> FindAllRadioButtonsWithSameName(
      HTMLInputElement* radio_button);

  ax::mojom::blink::WritingDirection GetTextDirection() const final;
  ax::mojom::blink::TextPosition GetTextPosition() const final;
  void GetTextStyleAndTextDecorationStyle(
      int32_t* text_style,
      ax::mojom::blink::TextDecorationStyle* text_overline_style,
      ax::mojom::blink::TextDecorationStyle* text_strikethrough_style,
      ax::mojom::blink::TextDecorationStyle* text_underline_style) const final;

  String ImageDataUrl(const gfx::Size& max_size) const final;
  int TextOffsetInFormattingContext(int offset) const override;

  // Object attributes.
  ax::mojom::blink::TextAlign GetTextAlign() const final;
  float GetTextIndent() const final;

  // Properties of interactive elements.
  ax::mojom::blink::AriaCurrentState GetAriaCurrentState() const final;
  ax::mojom::blink::InvalidState GetInvalidState() const final;
  bool IsValidFormControl(ListedElement* form_control) const;
  bool ValueForRange(float* out_value) const override;
  bool MaxValueForRange(float* out_value) const override;
  bool MinValueForRange(float* out_value) const override;
  bool StepValueForRange(float* out_value) const override;
  KURL Url() const override;
  AXObject* ChooserPopup() const override;
  String GetValueForControl() const override;
  String SlowGetValueForControlIncludingContentEditable() const override;
  String TextFromDescendants(AXObjectSet& visited,
                             const AXObject* aria_label_or_description_root,
                             bool recursive) const override;

  // ARIA attributes.
  ax::mojom::blink::Role AriaRoleAttribute() const final;
  void AriaDescribedbyElements(AXObjectVector&) const override;
  void AriaOwnsElements(AXObjectVector&) const override;
  bool SupportsARIADragging() const override;
  void Dropeffects(
      Vector<ax::mojom::blink::Dropeffect>& dropeffects) const override;

  ax::mojom::blink::HasPopup HasPopup() const override;
  ax::mojom::blink::IsPopup IsPopup() const override;
  bool IsEditableRoot() const override;
  bool HasContentEditableAttributeSet() const override;

  // Modify or take an action on an object.
  bool OnNativeSetValueAction(const String&) override;

  // AX name calculation.
  String GetName(ax::mojom::blink::NameFrom&,
                 AXObjectVector* name_objects) const override;
  String TextAlternative(bool recursive,
                         const AXObject* aria_label_or_description_root,
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
  String SVGDescription(ax::mojom::blink::NameFrom,
                        ax::mojom::blink::DescriptionFrom&,
                        DescriptionSources*,
                        AXRelatedObjectVector*) const;
  String Placeholder(ax::mojom::blink::NameFrom) const override;
  String Title(ax::mojom::blink::NameFrom) const override;

  // Location
  void GetRelativeBounds(AXObject** out_container,
                         gfx::RectF& out_bounds_in_container,
                         gfx::Transform& out_container_transform,
                         bool* clips_children = nullptr) const override;

  void AddChildren() override;
  bool CanHaveChildren() const override;
  // Set is_from_aria_owns to true if the child is being added because it was
  // pointed to from aria-owns.
  void AddChild(AXObject*, bool is_from_aria_owns = false);
  // Add a child that must be included in tree, enforced via DCHECK.
  void AddChildAndCheckIncluded(AXObject*, bool is_from_aria_owns = false);
  // If node is non-null, GetOrCreate an AXObject for it and add as a child.
  void AddNodeChild(Node*);
  // Set is_from_aria_owns to true if the child is being insert because it was
  // pointed to from aria-owns.
  void InsertChild(AXObject*, unsigned index, bool is_from_aria_owns = false);
  void SelectedOptions(AXObjectVector&) const override;

  // Properties of the object's owning document or page.
  double EstimatedLoadingProgress() const override;

  // DOM and Render tree access.
  Element* ActionElement() const override;
  Element* AnchorElement() const override;
  Document* GetDocument() const override;
  Node* GetNode() const final;

  // DOM and layout tree access.
  AtomicString Language() const override;
  bool HasAttribute(const QualifiedName&) const override;
  const AtomicString& GetAttribute(const QualifiedName&) const override;

  // Modify or take an action on an object.
  bool OnNativeFocusAction() final;
  bool OnNativeIncrementAction() final;
  bool OnNativeDecrementAction() final;
  bool OnNativeSetSequentialFocusNavigationStartingPointAction() final;

  // Notifications that this object may have changed.
  void HandleAriaExpandedChanged() override;
  void HandleActiveDescendantChanged() override;

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
  void ForceAddInlineTextBoxChildren() override;

  //
  // Layout object specific methods.
  //

  // If we can't determine a useful role from the DOM node, attempt to determine
  // a role from the layout object.
  virtual ax::mojom::blink::Role RoleFromLayoutObjectOrNode() const;

 private:
  bool HasInternalsAttribute(Element&, const QualifiedName&) const;
  const AtomicString& GetInternalsAttribute(Element&,
                                            const QualifiedName&) const;

  bool IsNativeCheckboxInMixedState() const;
  String TextAlternativeFromTitleAttribute(
      const AtomicString& title,
      ax::mojom::blink::NameFrom& name_from,
      NameSources* name_sources,
      bool* found_text_alternative) const;
  String NativeTextAlternative(AXObjectSet& visited,
                               ax::mojom::blink::NameFrom&,
                               AXRelatedObjectVector*,
                               NameSources*,
                               bool* found_text_alternative) const;
  String MaybeAppendFileDescriptionToName(const String& name) const;
  String PlaceholderFromNativeAttribute() const;
  String GetValueContributionToName(AXObjectSet& visited) const;
  bool UseNameFromSelectedOption() const;
  virtual bool IsTabItemSelected() const;

  void AddChildrenImpl();
  void AddNodeChildren();
  void AddPseudoElementChildrenFromLayoutTree();
  bool CanAddLayoutChild(LayoutObject& child);
  // Add inline textbox children, if either force == true or
  // AXObjectCache().InlineTextBoxAccessibilityEnabled().
  void AddInlineTextBoxChildren(bool force = false);
  void AddImageMapChildren();
  void AddPopupChildren();
  bool HasValidHTMLTableStructureAndLayout() const;
  void AddTableChildren();
  void AddValidationMessageChild();
  void AddAccessibleNodeChildren();
  void AddOwnedChildren();
#if DCHECK_IS_ON()
  void CheckValidChild(AXObject* child);
#endif

  ax::mojom::blink::TextPosition GetTextPositionFromRole() const;
  ax::mojom::blink::Dropeffect ParseDropeffect(String& dropeffect) const;

  static bool IsNameFromLabelElement(HTMLElement* control);
  static bool IsRedundantLabel(HTMLLabelElement* label);

  Member<Node> node_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_NODE_OBJECT_H_
