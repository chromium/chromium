// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_BASE_H_

#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"
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

enum class ElementFlags;

class ElementRareDataBase : public GarbageCollectedMixin {
 public:
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

  virtual void SetDidAttachInternals() = 0;
  virtual bool DidAttachInternals() const = 0;

  virtual void SetStyleShouldForceLegacyLayout(bool force) = 0;
  virtual bool StyleShouldForceLegacyLayout() const = 0;
  virtual void SetShouldForceLegacyLayoutForChild(bool force) = 0;
  virtual bool ShouldForceLegacyLayoutForChild() const = 0;
  virtual bool HasUndoStack() const = 0;
  virtual void SetHasUndoStack(bool value) = 0;
  virtual bool ScrollbarPseudoElementStylesDependOnFontMetrics() const = 0;
  virtual void SetScrollbarPseudoElementStylesDependOnFontMetrics(
      bool value) = 0;

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

  virtual FocusgroupFlags GetFocusgroupFlags() const = 0;
  virtual void SetFocusgroupFlags(FocusgroupFlags flags) = 0;
  virtual void ClearFocusgroupFlags() = 0;

  virtual bool AffectedBySubjectHas() const = 0;
  virtual void SetAffectedBySubjectHas() = 0;
  virtual bool AffectedByNonSubjectHas() const = 0;
  virtual void SetAffectedByNonSubjectHas() = 0;
  virtual bool AncestorsOrAncestorSiblingsAffectedByHas() const = 0;
  virtual void SetAncestorsOrAncestorSiblingsAffectedByHas() = 0;
  virtual unsigned GetSiblingsAffectedByHasFlags() const = 0;
  virtual bool HasSiblingsAffectedByHasFlags(unsigned flags) const = 0;
  virtual void SetSiblingsAffectedByHasFlags(unsigned flags) = 0;
  virtual bool AffectedByPseudoInHas() const = 0;
  virtual void SetAffectedByPseudoInHas() = 0;
  virtual bool AncestorsOrSiblingsAffectedByHoverInHas() const = 0;
  virtual void SetAncestorsOrSiblingsAffectedByHoverInHas() = 0;
  virtual bool AncestorsOrSiblingsAffectedByActiveInHas() const = 0;
  virtual void SetAncestorsOrSiblingsAffectedByActiveInHas() = 0;
  virtual bool AncestorsOrSiblingsAffectedByFocusInHas() const = 0;
  virtual void SetAncestorsOrSiblingsAffectedByFocusInHas() = 0;
  virtual bool AncestorsOrSiblingsAffectedByFocusVisibleInHas() const = 0;
  virtual void SetAncestorsOrSiblingsAffectedByFocusVisibleInHas() = 0;
  virtual bool AffectedByLogicalCombinationsInHas() const = 0;
  virtual void SetAffectedByLogicalCombinationsInHas() = 0;
  virtual bool AffectedByMultipleHas() const = 0;
  virtual void SetAffectedByMultipleHas() = 0;

  virtual void SetTabIndexExplicitly() = 0;
  virtual void ClearTabIndexExplicitly() = 0;

  virtual AnchorScrollData* GetAnchorScrollData() const = 0;
  virtual void RemoveAnchorScrollData() = 0;
  virtual AnchorScrollData& EnsureAnchorScrollData(Element*) = 0;

  // from NodeRareData
  virtual bool HasElementFlag(ElementFlags mask) const = 0;
  virtual void SetElementFlag(ElementFlags mask, bool value) = 0;
  virtual void ClearElementFlag(ElementFlags mask) = 0;
  virtual bool HasRestyleFlags() const = 0;
  virtual void ClearRestyleFlags() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_BASE_H_
