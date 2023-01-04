// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_BASE_H_

#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"
#include "third_party/blink/renderer/core/dom/node_rare_data.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/pseudo_element_data.h"
#include "third_party/blink/renderer/platform/region_capture_crop_id.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CSSStyleDeclaration;
class ShadowRoot;
class NamedNodeMap;
class DOMTokenList;
class DatasetDOMStringMap;
class ElementAnimations;
class Attr;
typedef HeapVector<Member<Attr>> AttrNodeList;
class ElementIntersectionObserverData;
class ContainerQueryEvaluator;
class EditContext;
class InlineStylePropertyMap;
class ElementInternals;
class AccessibleNode;
class DisplayLockContext;
class ContainerQueryData;
class ResizeObserver;
class ResizeObservation;
class CustomElementDefinition;
class ResizeObserverSize;
class PopoverData;
class CSSToggleMap;
class HTMLElement;

enum class ElementFlags;

class ElementRareDataBase : public NodeRareData {
 public:
  explicit ElementRareDataBase(NodeData* node_layout_data)
      : NodeRareData(ClassType::kElementRareData,
                     std::move(*node_layout_data)) {}

  virtual void SetPseudoElement(
      PseudoId,
      PseudoElement*,
      const AtomicString& document_transition_tag = g_null_atom) = 0;
  virtual PseudoElement* GetPseudoElement(
      PseudoId,
      const AtomicString& document_transition_tag = g_null_atom) const = 0;
  virtual PseudoElementData::PseudoElementVector GetPseudoElements() const = 0;

  virtual CSSStyleDeclaration& EnsureInlineCSSStyleDeclaration(
      Element* owner_element) = 0;

  virtual ShadowRoot* GetShadowRoot() const = 0;
  virtual void SetShadowRoot(ShadowRoot& shadow_root) = 0;

  virtual NamedNodeMap* AttributeMap() const = 0;
  virtual void SetAttributeMap(NamedNodeMap* attribute_map) = 0;

  virtual DOMTokenList* GetClassList() const = 0;
  virtual void SetClassList(DOMTokenList* class_list) = 0;

  virtual DatasetDOMStringMap* Dataset() const = 0;
  virtual void SetDataset(DatasetDOMStringMap* dataset) = 0;

  virtual ScrollOffset SavedLayerScrollOffset() const = 0;
  virtual void SetSavedLayerScrollOffset(ScrollOffset offset) = 0;

  virtual ElementAnimations* GetElementAnimations() = 0;
  virtual void SetElementAnimations(ElementAnimations* element_animations) = 0;

  virtual bool HasPseudoElements() const = 0;
  virtual void ClearPseudoElements() = 0;

  virtual AttrNodeList& EnsureAttrNodeList() = 0;
  virtual AttrNodeList* GetAttrNodeList() = 0;
  virtual void RemoveAttrNodeList() = 0;
  virtual void AddAttr(Attr* attr) = 0;

  virtual ElementIntersectionObserverData* IntersectionObserverData() const = 0;
  virtual ElementIntersectionObserverData& EnsureIntersectionObserverData() = 0;

  virtual ContainerQueryEvaluator* GetContainerQueryEvaluator() const = 0;
  virtual void SetContainerQueryEvaluator(
      ContainerQueryEvaluator* evaluator) = 0;

  virtual const AtomicString& GetNonce() const = 0;
  virtual void SetNonce(const AtomicString& nonce) = 0;

  virtual EditContext* GetEditContext() const = 0;
  virtual void SetEditContext(EditContext* edit_context) = 0;

  virtual void SetPart(DOMTokenList* part) = 0;
  virtual DOMTokenList* GetPart() const = 0;

  virtual void SetPartNamesMap(const AtomicString part_names) = 0;
  virtual const NamesMap* PartNamesMap() const = 0;

  virtual InlineStylePropertyMap& EnsureInlineStylePropertyMap(
      Element* owner_element) = 0;
  virtual InlineStylePropertyMap* GetInlineStylePropertyMap() = 0;

  virtual ElementInternals& EnsureElementInternals(HTMLElement& target) = 0;
  virtual const ElementInternals* GetElementInternals() const = 0;

  virtual AccessibleNode* GetAccessibleNode() const = 0;
  virtual AccessibleNode* EnsureAccessibleNode(Element* owner_element) = 0;
  virtual void ClearAccessibleNode() = 0;

  virtual DisplayLockContext* EnsureDisplayLockContext(Element* element) = 0;
  virtual DisplayLockContext* GetDisplayLockContext() const = 0;

  virtual ContainerQueryData& EnsureContainerQueryData() = 0;
  virtual ContainerQueryData* GetContainerQueryData() const = 0;
  virtual void ClearContainerQueryData() = 0;

  // Returns the crop-ID if one was set, or nullptr otherwise.
  virtual const RegionCaptureCropId* GetRegionCaptureCropId() const = 0;

  // Sets a crop-ID on the item. Must be called at most once. Cannot be used
  // to unset a previously set crop-ID.
  virtual void SetRegionCaptureCropId(
      std::unique_ptr<RegionCaptureCropId> crop_id) = 0;

  using ResizeObserverDataMap =
      HeapHashMap<Member<ResizeObserver>, Member<ResizeObservation>>;

  virtual ResizeObserverDataMap* ResizeObserverData() const = 0;
  virtual ResizeObserverDataMap& EnsureResizeObserverData() = 0;

  virtual void SetCustomElementDefinition(
      CustomElementDefinition* definition) = 0;
  virtual CustomElementDefinition* GetCustomElementDefinition() const = 0;

  virtual void SetIsValue(const AtomicString& is_value) = 0;
  virtual const AtomicString& IsValue() const = 0;

  virtual void SaveLastIntrinsicSize(ResizeObserverSize* size) = 0;
  virtual const ResizeObserverSize* LastIntrinsicSize() const = 0;

  virtual PopoverData* GetPopoverData() const = 0;
  virtual PopoverData& EnsurePopoverData() = 0;
  virtual void RemovePopoverData() = 0;

  virtual CSSToggleMap* GetToggleMap() const = 0;
  virtual CSSToggleMap& EnsureToggleMap(Element* owner_element) = 0;

  FocusgroupFlags GetFocusgroupFlags() const { return focusgroup_flags_; }
  void SetFocusgroupFlags(FocusgroupFlags flags) { focusgroup_flags_ = flags; }
  void ClearFocusgroupFlags() { focusgroup_flags_ = FocusgroupFlags::kNone; }

  bool AffectedBySubjectHas() const {
    return has_invalidation_flags_.affected_by_subject_has;
  }
  void SetAffectedBySubjectHas() {
    has_invalidation_flags_.affected_by_subject_has = true;
  }
  bool AffectedByNonSubjectHas() const {
    return has_invalidation_flags_.affected_by_non_subject_has;
  }
  void SetAffectedByNonSubjectHas() {
    has_invalidation_flags_.affected_by_non_subject_has = true;
  }
  bool AncestorsOrAncestorSiblingsAffectedByHas() const {
    return has_invalidation_flags_
        .ancestors_or_ancestor_siblings_affected_by_has;
  }
  void SetAncestorsOrAncestorSiblingsAffectedByHas() {
    has_invalidation_flags_.ancestors_or_ancestor_siblings_affected_by_has =
        true;
  }
  unsigned GetSiblingsAffectedByHasFlags() const {
    return has_invalidation_flags_.siblings_affected_by_has;
  }
  bool HasSiblingsAffectedByHasFlags(unsigned flags) const {
    return has_invalidation_flags_.siblings_affected_by_has & flags;
  }
  void SetSiblingsAffectedByHasFlags(unsigned flags) {
    has_invalidation_flags_.siblings_affected_by_has |= flags;
  }
  bool AffectedByPseudoInHas() const {
    return has_invalidation_flags_.affected_by_pseudos_in_has;
  }
  void SetAffectedByPseudoInHas() {
    has_invalidation_flags_.affected_by_pseudos_in_has = true;
  }
  bool AncestorsOrSiblingsAffectedByHoverInHas() const {
    return has_invalidation_flags_
        .ancestors_or_siblings_affected_by_hover_in_has;
  }
  void SetAncestorsOrSiblingsAffectedByHoverInHas() {
    has_invalidation_flags_.ancestors_or_siblings_affected_by_hover_in_has =
        true;
  }
  bool AncestorsOrSiblingsAffectedByActiveInHas() const {
    return has_invalidation_flags_
        .ancestors_or_siblings_affected_by_active_in_has;
  }
  void SetAncestorsOrSiblingsAffectedByActiveInHas() {
    has_invalidation_flags_.ancestors_or_siblings_affected_by_active_in_has =
        true;
  }
  bool AncestorsOrSiblingsAffectedByFocusInHas() const {
    return has_invalidation_flags_
        .ancestors_or_siblings_affected_by_focus_in_has;
  }
  void SetAncestorsOrSiblingsAffectedByFocusInHas() {
    has_invalidation_flags_.ancestors_or_siblings_affected_by_focus_in_has =
        true;
  }
  bool AncestorsOrSiblingsAffectedByFocusVisibleInHas() const {
    return has_invalidation_flags_
        .ancestors_or_siblings_affected_by_focus_visible_in_has;
  }
  void SetAncestorsOrSiblingsAffectedByFocusVisibleInHas() {
    has_invalidation_flags_
        .ancestors_or_siblings_affected_by_focus_visible_in_has = true;
  }
  bool AffectedByLogicalCombinationsInHas() const {
    return has_invalidation_flags_.affected_by_logical_combinations_in_has;
  }
  void SetAffectedByLogicalCombinationsInHas() {
    has_invalidation_flags_.affected_by_logical_combinations_in_has = true;
  }
  bool AffectedByMultipleHas() const {
    return has_invalidation_flags_.affected_by_multiple_has;
  }
  void SetAffectedByMultipleHas() {
    has_invalidation_flags_.affected_by_multiple_has = true;
  }

  virtual void SetTabIndexExplicitly() = 0;
  virtual void ClearTabIndexExplicitly() = 0;

  virtual AnchorScrollData* GetAnchorScrollData() const = 0;
  virtual void RemoveAnchorScrollData() = 0;
  virtual AnchorScrollData& EnsureAnchorScrollData(Element*) = 0;

  virtual void IncrementAnchoredPopoverCount() = 0;
  virtual void DecrementAnchoredPopoverCount() = 0;
  virtual bool HasAnchoredPopover() const = 0;

  // from NodeRareData
  virtual bool HasElementFlag(ElementFlags mask) const = 0;
  virtual void SetElementFlag(ElementFlags mask, bool value) = 0;
  virtual void ClearElementFlag(ElementFlags mask) = 0;
  virtual bool HasRestyleFlags() const = 0;
  virtual void ClearRestyleFlags() = 0;

  void SetDidAttachInternals() { did_attach_internals_ = true; }
  bool DidAttachInternals() const { return did_attach_internals_; }
  void SetStyleShouldForceLegacyLayout(bool force) {
    style_should_force_legacy_layout_ = force;
  }
  bool StyleShouldForceLegacyLayout() const {
    return style_should_force_legacy_layout_;
  }
  void SetShouldForceLegacyLayoutForChild(bool force) {
    should_force_legacy_layout_for_child_ = force;
  }
  bool ShouldForceLegacyLayoutForChild() const {
    return should_force_legacy_layout_for_child_;
  }
  bool HasUndoStack() const { return has_undo_stack_; }
  void SetHasUndoStack(bool value) { has_undo_stack_ = value; }
  bool ScrollbarPseudoElementStylesDependOnFontMetrics() const {
    return scrollbar_pseudo_element_styles_depend_on_font_metrics_;
  }
  void SetScrollbarPseudoElementStylesDependOnFontMetrics(bool value) {
    scrollbar_pseudo_element_styles_depend_on_font_metrics_ = value;
  }

 private:
  unsigned did_attach_internals_ : 1;
  unsigned should_force_legacy_layout_for_child_ : 1;
  unsigned style_should_force_legacy_layout_ : 1;
  unsigned has_undo_stack_ : 1;
  unsigned scrollbar_pseudo_element_styles_depend_on_font_metrics_ : 1;
  HasInvalidationFlags has_invalidation_flags_;
  FocusgroupFlags focusgroup_flags_ = FocusgroupFlags::kNone;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_BASE_H_
