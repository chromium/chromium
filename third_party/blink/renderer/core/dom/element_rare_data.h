/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith <catfish.man@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_H_

#include <memory>

#include "base/token.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/css/container_query_data.h"
#include "third_party/blink/renderer/core/css/cssom/inline_style_property_map.h"
#include "third_party/blink/renderer/core/css/inline_css_style_declaration.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/css_toggle_map.h"
#include "third_party/blink/renderer/core/dom/dataset_dom_string_map.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"
#include "third_party/blink/renderer/core/dom/has_invalidation_flags.h"
#include "third_party/blink/renderer/core/dom/named_node_map.h"
#include "third_party/blink/renderer/core/dom/names_map.h"
#include "third_party/blink/renderer/core/dom/node_rare_data.h"
#include "third_party/blink/renderer/core/dom/popup_data.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/pseudo_element_data.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/core/intersection_observer/element_intersection_observer_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/region_capture_crop_id.h"

namespace blink {

class ContainerQueryData;
class Element;
class HTMLElement;
class ResizeObservation;
class ResizeObserver;

// This class contains rare data which is significantly more rare than the data
// in ElementRareData. They are split up to improve memory usage.
class ElementSuperRareData : public GarbageCollected<ElementSuperRareData> {
 public:
  const AtomicString& GetNonce() const { return nonce_; }
  void SetNonce(const AtomicString& nonce) { nonce_ = nonce; }

  EditContext* GetEditContext() const { return edit_context_.Get(); }
  void SetEditContext(EditContext* edit_context) {
    edit_context_ = edit_context;
  }

  void SetPart(DOMTokenList* part) { part_ = part; }
  DOMTokenList* GetPart() const { return part_.Get(); }

  void SetPartNamesMap(const AtomicString part_names) {
    if (!part_names_map_) {
      part_names_map_ = std::make_unique<NamesMap>();
    }
    part_names_map_->Set(part_names);
  }
  const NamesMap* PartNamesMap() const { return part_names_map_.get(); }

  InlineStylePropertyMap& EnsureInlineStylePropertyMap(Element* owner_element);
  InlineStylePropertyMap* GetInlineStylePropertyMap() {
    return cssom_map_wrapper_.Get();
  }

  ElementInternals& EnsureElementInternals(HTMLElement& target);
  const ElementInternals* GetElementInternals() const {
    return element_internals_;
  }

  AccessibleNode* GetAccessibleNode() const { return accessible_node_.Get(); }
  AccessibleNode* EnsureAccessibleNode(Element* owner_element) {
    if (!accessible_node_) {
      accessible_node_ = MakeGarbageCollected<AccessibleNode>(owner_element);
    }
    return accessible_node_;
  }
  void ClearAccessibleNode() { accessible_node_.Clear(); }

  DisplayLockContext* EnsureDisplayLockContext(Element* element) {
    if (!display_lock_context_) {
      display_lock_context_ = MakeGarbageCollected<DisplayLockContext>(element);
    }
    return display_lock_context_.Get();
  }
  DisplayLockContext* GetDisplayLockContext() const {
    return display_lock_context_;
  }

  ContainerQueryData& EnsureContainerQueryData() {
    DCHECK(RuntimeEnabledFeatures::CSSContainerQueriesEnabled());
    if (!container_query_data_)
      container_query_data_ = MakeGarbageCollected<ContainerQueryData>();
    return *container_query_data_;
  }
  ContainerQueryData* GetContainerQueryData() const {
    return container_query_data_;
  }
  void ClearContainerQueryData() { container_query_data_ = nullptr; }

  // Returns the crop-ID if one was set, or nullptr otherwise.
  const RegionCaptureCropId* GetRegionCaptureCropId() const {
    return region_capture_crop_id_.get();
  }

  // Sets a crop-ID on the item. Must be called at most once. Cannot be used
  // to unset a previously set crop-ID.
  void SetRegionCaptureCropId(std::unique_ptr<RegionCaptureCropId> crop_id) {
    DCHECK(!GetRegionCaptureCropId());
    DCHECK(crop_id);
    DCHECK(!crop_id->value().is_zero());
    region_capture_crop_id_ = std::move(crop_id);
  }

  using ResizeObserverDataMap =
      HeapHashMap<Member<ResizeObserver>, Member<ResizeObservation>>;

  ResizeObserverDataMap* ResizeObserverData() const {
    return resize_observer_data_;
  }
  ResizeObserverDataMap& EnsureResizeObserverData();

  void SetCustomElementDefinition(CustomElementDefinition* definition) {
    custom_element_definition_ = definition;
  }
  CustomElementDefinition* GetCustomElementDefinition() const {
    return custom_element_definition_.Get();
  }

  void SetIsValue(const AtomicString& is_value) { is_value_ = is_value; }
  const AtomicString& IsValue() const { return is_value_; }

  void SaveLastIntrinsicSize(ResizeObserverSize* size) {
    last_intrinsic_size_ = size;
  }
  const ResizeObserverSize* LastIntrinsicSize() const {
    return last_intrinsic_size_;
  }

  PopupData* GetPopupData() const { return popup_data_; }
  PopupData& EnsurePopupData();
  void RemovePopupData();

  CSSToggleMap* GetToggleMap() const { return toggle_map_.Get(); }
  CSSToggleMap& EnsureToggleMap(Element* owner_element);

  FocusgroupFlags GetFocusgroupFlags() const { return focusgroup_flags_; }

  void SetFocusgroupFlags(FocusgroupFlags flags) { focusgroup_flags_ = flags; }
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

  void Trace(blink::Visitor*) const;

 private:
  AtomicString nonce_;
  Member<EditContext> edit_context_;
  Member<DOMTokenList> part_;
  std::unique_ptr<NamesMap> part_names_map_;
  Member<InlineStylePropertyMap> cssom_map_wrapper_;
  Member<ElementInternals> element_internals_;
  Member<AccessibleNode> accessible_node_;
  Member<DisplayLockContext> display_lock_context_;
  Member<ContainerQueryData> container_query_data_;
  std::unique_ptr<RegionCaptureCropId> region_capture_crop_id_;
  Member<ResizeObserverDataMap> resize_observer_data_;
  Member<CustomElementDefinition> custom_element_definition_;
  AtomicString is_value_;
  Member<ResizeObserverSize> last_intrinsic_size_;
  Member<PopupData> popup_data_;
  Member<CSSToggleMap> toggle_map_;
  FocusgroupFlags focusgroup_flags_ = FocusgroupFlags::kNone;
  HasInvalidationFlags has_invalidation_flags_;
};

class ElementRareData final : public NodeRareData {
 public:
  explicit ElementRareData(NodeRenderingData*);
  ~ElementRareData();

  void SetPseudoElement(
      PseudoId,
      PseudoElement*,
      const AtomicString& document_transition_tag = g_null_atom);
  PseudoElement* GetPseudoElement(
      PseudoId,
      const AtomicString& document_transition_tag = g_null_atom) const;
  PseudoElementData::PseudoElementVector GetPseudoElements() const;

  void SetTabIndexExplicitly() {
    SetElementFlag(ElementFlags::kTabIndexWasSetExplicitly, true);
  }

  void ClearTabIndexExplicitly() {
    ClearElementFlag(ElementFlags::kTabIndexWasSetExplicitly);
  }

  FocusgroupFlags GetFocusgroupFlags() const {
    if (super_rare_data_)
      return super_rare_data_->GetFocusgroupFlags();
    return FocusgroupFlags::kNone;
  }

  void SetFocusgroupFlags(FocusgroupFlags flags) {
    EnsureSuperRareData().SetFocusgroupFlags(flags);
  }

  void ClearFocusgroupFlags() {
    if (!super_rare_data_)
      return;
    SetFocusgroupFlags(FocusgroupFlags::kNone);
  }

  CSSStyleDeclaration& EnsureInlineCSSStyleDeclaration(Element* owner_element);
  InlineStylePropertyMap& EnsureInlineStylePropertyMap(Element* owner_element);

  InlineStylePropertyMap* GetInlineStylePropertyMap() {
    if (super_rare_data_)
      return super_rare_data_->GetInlineStylePropertyMap();
    return nullptr;
  }

  ShadowRoot* GetShadowRoot() const { return shadow_root_.Get(); }
  void SetShadowRoot(ShadowRoot& shadow_root) {
    DCHECK(!shadow_root_);
    shadow_root_ = &shadow_root;
  }

  EditContext* GetEditContext() const {
    if (super_rare_data_)
      return super_rare_data_->GetEditContext();
    return nullptr;
  }
  void SetEditContext(EditContext* edit_context) {
    EnsureSuperRareData().SetEditContext(edit_context);
  }

  NamedNodeMap* AttributeMap() const { return attribute_map_.Get(); }
  void SetAttributeMap(NamedNodeMap* attribute_map) {
    attribute_map_ = attribute_map;
  }

  DOMTokenList* GetClassList() const { return class_list_.Get(); }
  void SetClassList(DOMTokenList* class_list) {
    class_list_ = class_list;
  }

  void SetPart(DOMTokenList* part) { EnsureSuperRareData().SetPart(part); }
  DOMTokenList* GetPart() const {
    if (super_rare_data_)
      return super_rare_data_->GetPart();
    return nullptr;
  }

  const NamesMap* PartNamesMap() const {
    if (super_rare_data_)
      return super_rare_data_->PartNamesMap();
    return nullptr;
  }
  void SetPartNamesMap(const AtomicString part_names) {
    EnsureSuperRareData().SetPartNamesMap(part_names);
  }

  DatasetDOMStringMap* Dataset() const { return dataset_.Get(); }
  void SetDataset(DatasetDOMStringMap* dataset) {
    dataset_ = dataset;
  }

  ScrollOffset SavedLayerScrollOffset() const {
    return saved_layer_scroll_offset_;
  }
  void SetSavedLayerScrollOffset(ScrollOffset offset) {
    saved_layer_scroll_offset_ = offset;
  }

  ElementAnimations* GetElementAnimations() {
    return element_animations_.Get();
  }
  void SetElementAnimations(ElementAnimations* element_animations) {
    element_animations_ = element_animations;
  }

  bool HasPseudoElements() const;
  void ClearPseudoElements();

  void SetCustomElementDefinition(CustomElementDefinition* definition) {
    EnsureSuperRareData().SetCustomElementDefinition(definition);
  }
  CustomElementDefinition* GetCustomElementDefinition() const {
    if (super_rare_data_)
      return super_rare_data_->GetCustomElementDefinition();
    return nullptr;
  }
  void SetIsValue(const AtomicString& is_value) {
    EnsureSuperRareData().SetIsValue(is_value);
  }
  const AtomicString& IsValue() const {
    if (super_rare_data_)
      return super_rare_data_->IsValue();
    return g_null_atom;
  }
  void SetDidAttachInternals() { did_attach_internals_ = true; }
  bool DidAttachInternals() const { return did_attach_internals_; }
  ElementInternals& EnsureElementInternals(HTMLElement& target) {
    return EnsureSuperRareData().EnsureElementInternals(target);
  }
  const ElementInternals* GetElementInternals() const {
    if (super_rare_data_)
      return super_rare_data_->GetElementInternals();
    return nullptr;
  }

  const RegionCaptureCropId* GetRegionCaptureCropId() const {
    if (super_rare_data_)
      return super_rare_data_->GetRegionCaptureCropId();
    return nullptr;
  }
  void SetRegionCaptureCropId(std::unique_ptr<RegionCaptureCropId> value) {
    EnsureSuperRareData().SetRegionCaptureCropId(std::move(value));
  }

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
  bool AffectedBySubjectHas() const {
    return super_rare_data_ ? super_rare_data_->AffectedBySubjectHas() : false;
  }
  void SetAffectedBySubjectHas() {
    EnsureSuperRareData().SetAffectedBySubjectHas();
  }
  bool AffectedByNonSubjectHas() const {
    return super_rare_data_ ? super_rare_data_->AffectedByNonSubjectHas()
                            : false;
  }
  void SetAffectedByNonSubjectHas() {
    EnsureSuperRareData().SetAffectedByNonSubjectHas();
  }
  bool AncestorsOrAncestorSiblingsAffectedByHas() const {
    return super_rare_data_
               ? super_rare_data_->AncestorsOrAncestorSiblingsAffectedByHas()
               : false;
  }
  void SetAncestorsOrAncestorSiblingsAffectedByHas() {
    EnsureSuperRareData().SetAncestorsOrAncestorSiblingsAffectedByHas();
  }
  unsigned GetSiblingsAffectedByHasFlags() const {
    return super_rare_data_ ? super_rare_data_->GetSiblingsAffectedByHasFlags()
                            : kNoSiblingsAffectedByHasFlags;
  }
  bool HasSiblingsAffectedByHasFlags(unsigned flags) const {
    return super_rare_data_
               ? super_rare_data_->HasSiblingsAffectedByHasFlags(flags)
               : false;
  }
  bool AffectedByPseudoInHas() const {
    return super_rare_data_ ? super_rare_data_->AffectedByPseudoInHas() : false;
  }
  void SetAffectedByPseudoInHas() {
    EnsureSuperRareData().SetAffectedByPseudoInHas();
  }
  void SetSiblingsAffectedByHasFlags(unsigned flags) {
    EnsureSuperRareData().SetSiblingsAffectedByHasFlags(flags);
  }
  bool AncestorsOrSiblingsAffectedByHoverInHas() const {
    return super_rare_data_
               ? super_rare_data_->AncestorsOrSiblingsAffectedByHoverInHas()
               : false;
  }
  void SetAncestorsOrSiblingsAffectedByHoverInHas() {
    EnsureSuperRareData().SetAncestorsOrSiblingsAffectedByHoverInHas();
  }
  bool AncestorsOrSiblingsAffectedByActiveInHas() const {
    return super_rare_data_
               ? super_rare_data_->AncestorsOrSiblingsAffectedByActiveInHas()
               : false;
  }
  void SetAncestorsOrSiblingsAffectedByActiveInHas() {
    EnsureSuperRareData().SetAncestorsOrSiblingsAffectedByActiveInHas();
  }
  bool AncestorsOrSiblingsAffectedByFocusInHas() const {
    return super_rare_data_
               ? super_rare_data_->AncestorsOrSiblingsAffectedByFocusInHas()
               : false;
  }
  void SetAncestorsOrSiblingsAffectedByFocusInHas() {
    EnsureSuperRareData().SetAncestorsOrSiblingsAffectedByFocusInHas();
  }
  bool AncestorsOrSiblingsAffectedByFocusVisibleInHas() const {
    return super_rare_data_
               ? super_rare_data_
                     ->AncestorsOrSiblingsAffectedByFocusVisibleInHas()
               : false;
  }
  void SetAncestorsOrSiblingsAffectedByFocusVisibleInHas() {
    EnsureSuperRareData().SetAncestorsOrSiblingsAffectedByFocusVisibleInHas();
  }
  bool AffectedByLogicalCombinationsInHas() const {
    return super_rare_data_
               ? super_rare_data_->AffectedByLogicalCombinationsInHas()
               : false;
  }
  void SetAffectedByLogicalCombinationsInHas() {
    EnsureSuperRareData().SetAffectedByLogicalCombinationsInHas();
  }
  bool AffectedByMultipleHas() const {
    return super_rare_data_ ? super_rare_data_->AffectedByMultipleHas() : false;
  }
  void SetAffectedByMultipleHas() {
    EnsureSuperRareData().SetAffectedByMultipleHas();
  }

  AccessibleNode* GetAccessibleNode() const {
    if (super_rare_data_)
      return super_rare_data_->GetAccessibleNode();
    return nullptr;
  }
  AccessibleNode* EnsureAccessibleNode(Element* owner_element) {
    return EnsureSuperRareData().EnsureAccessibleNode(owner_element);
  }
  void ClearAccessibleNode() {
    if (super_rare_data_)
      super_rare_data_->ClearAccessibleNode();
  }

  AttrNodeList& EnsureAttrNodeList();
  AttrNodeList* GetAttrNodeList() { return attr_node_list_.Get(); }
  void RemoveAttrNodeList() { attr_node_list_.Clear(); }
  void AddAttr(Attr* attr) {
    EnsureAttrNodeList().push_back(attr);
  }

  ElementIntersectionObserverData* IntersectionObserverData() const {
    return intersection_observer_data_.Get();
  }
  ElementIntersectionObserverData& EnsureIntersectionObserverData() {
    if (!intersection_observer_data_) {
      intersection_observer_data_ =
          MakeGarbageCollected<ElementIntersectionObserverData>();
    }
    return *intersection_observer_data_;
  }

  using ResizeObserverDataMap =
      HeapHashMap<Member<ResizeObserver>, Member<ResizeObservation>>;

  ResizeObserverDataMap* ResizeObserverData() const {
    if (super_rare_data_)
      return super_rare_data_->ResizeObserverData();
    return nullptr;
  }
  ResizeObserverDataMap& EnsureResizeObserverData();

  PopupData* GetPopupData() const {
    if (super_rare_data_)
      return super_rare_data_->GetPopupData();
    return nullptr;
  }
  PopupData& EnsurePopupData();
  void RemovePopupData();

  CSSToggleMap* GetToggleMap() const {
    if (super_rare_data_)
      return super_rare_data_->GetToggleMap();
    return nullptr;
  }
  CSSToggleMap& EnsureToggleMap(Element* owner_element);

  DisplayLockContext* EnsureDisplayLockContext(Element* element) {
    return EnsureSuperRareData().EnsureDisplayLockContext(element);
  }
  DisplayLockContext* GetDisplayLockContext() const {
    if (super_rare_data_)
      return super_rare_data_->GetDisplayLockContext();
    return nullptr;
  }

  ContainerQueryData& EnsureContainerQueryData() {
    return EnsureSuperRareData().EnsureContainerQueryData();
  }
  ContainerQueryData* GetContainerQueryData() const {
    if (super_rare_data_)
      return super_rare_data_->GetContainerQueryData();
    return nullptr;
  }
  void ClearContainerQueryData() {
    if (super_rare_data_)
      super_rare_data_->ClearContainerQueryData();
  }

  ContainerQueryEvaluator* GetContainerQueryEvaluator() const {
    ContainerQueryData* container_query_data = GetContainerQueryData();
    if (!container_query_data)
      return nullptr;
    return container_query_data->GetContainerQueryEvaluator();
  }
  void SetContainerQueryEvaluator(ContainerQueryEvaluator* evaluator) {
    ContainerQueryData* container_query_data = GetContainerQueryData();
    if (container_query_data)
      container_query_data->SetContainerQueryEvaluator(evaluator);
    else if (evaluator)
      EnsureContainerQueryData().SetContainerQueryEvaluator(evaluator);
  }

  const AtomicString& GetNonce() const {
    if (super_rare_data_)
      return super_rare_data_->GetNonce();
    return g_null_atom;
  }
  void SetNonce(const AtomicString& nonce) {
    EnsureSuperRareData().SetNonce(nonce);
  }

  void SaveLastIntrinsicSize(ResizeObserverSize* size) {
    EnsureSuperRareData().SaveLastIntrinsicSize(size);
  }
  const ResizeObserverSize* LastIntrinsicSize() const {
    if (super_rare_data_)
      return super_rare_data_->LastIntrinsicSize();
    return nullptr;
  }

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  ElementSuperRareData& EnsureSuperRareData() {
    if (!super_rare_data_)
      super_rare_data_ = MakeGarbageCollected<ElementSuperRareData>();
    return *super_rare_data_;
  }

  Member<ElementSuperRareData> super_rare_data_;

  ScrollOffset saved_layer_scroll_offset_;

  Member<DatasetDOMStringMap> dataset_;
  Member<ShadowRoot> shadow_root_;
  Member<DOMTokenList> class_list_;
  Member<NamedNodeMap> attribute_map_;
  Member<AttrNodeList> attr_node_list_;
  Member<InlineCSSStyleDeclaration> cssom_wrapper_;

  Member<ElementAnimations> element_animations_;
  Member<ElementIntersectionObserverData> intersection_observer_data_;

  Member<PseudoElementData> pseudo_element_data_;

  unsigned did_attach_internals_ : 1;
  unsigned should_force_legacy_layout_for_child_ : 1;
  unsigned style_should_force_legacy_layout_ : 1;
  unsigned has_undo_stack_ : 1;
  unsigned scrollbar_pseudo_element_styles_depend_on_font_metrics_ : 1;
};

inline LayoutSize DefaultMinimumSizeForResizing() {
  return LayoutSize(LayoutUnit::Max(), LayoutUnit::Max());
}

inline bool ElementRareData::HasPseudoElements() const {
  return (pseudo_element_data_ && pseudo_element_data_->HasPseudoElements());
}

inline void ElementRareData::ClearPseudoElements() {
  if (pseudo_element_data_) {
    pseudo_element_data_->ClearPseudoElements();
    pseudo_element_data_.Clear();
  }
}

inline void ElementRareData::SetPseudoElement(
    PseudoId pseudo_id,
    PseudoElement* element,
    const AtomicString& document_transition_tag) {
  if (!pseudo_element_data_) {
    if (!element)
      return;
    pseudo_element_data_ = MakeGarbageCollected<PseudoElementData>();
  }
  pseudo_element_data_->SetPseudoElement(pseudo_id, element,
                                         document_transition_tag);
}

inline PseudoElement* ElementRareData::GetPseudoElement(
    PseudoId pseudo_id,
    const AtomicString& document_transition_tag) const {
  if (!pseudo_element_data_)
    return nullptr;
  return pseudo_element_data_->GetPseudoElement(pseudo_id,
                                                document_transition_tag);
}

inline PseudoElementData::PseudoElementVector
ElementRareData::GetPseudoElements() const {
  if (!pseudo_element_data_)
    return {};
  return pseudo_element_data_->GetPseudoElements();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_H_
