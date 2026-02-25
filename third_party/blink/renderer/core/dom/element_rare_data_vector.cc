// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/element_rare_data_vector.h"

#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/css/container_query_data.h"
#include "third_party/blink/renderer/core/css/cssom/inline_style_property_map.h"
#include "third_party/blink/renderer/core/css/inline_css_style_declaration.h"
#include "third_party/blink/renderer/core/css/out_of_flow_data.h"
#include "third_party/blink/renderer/core/css/style_scope_data.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/css_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/dataset_dom_string_map.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_vector.h"
#include "third_party/blink/renderer/core/dom/explicitly_set_attr_elements_map.h"
#include "third_party/blink/renderer/core/dom/flat_tree_node_data.h"
#include "third_party/blink/renderer/core/dom/has_invalidation_flags.h"
#include "third_party/blink/renderer/core/dom/interest_invoker_target_data.h"
#include "third_party/blink/renderer/core/dom/invoker_data.h"
#include "third_party/blink/renderer/core/dom/mutation_observer_registration.h"
#include "third_party/blink/renderer/core/dom/named_node_map.h"
#include "third_party/blink/renderer/core/dom/names_map.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_group_data.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/editing/ime/edit_context.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/display_ad_element_monitor.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/intersection_observer/element_intersection_observer_data.h"
#include "third_party/blink/renderer/core/layout/anchor_position_scroll_data.h"
#include "third_party/blink/renderer/core/layout/anchor_position_visibility_observer.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/overscroll/overscroll_area_tracker.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observation.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/content_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

ElementRareDataVector::~ElementRareDataVector() {
  DCHECK(!GetField(FieldId::kPseudoElementData));
}

ElementRareDataField* ElementRareDataVector::GetField(FieldId field_id) const {
  if (HasField(field_id)) {
    return ArraySlot(field_id);
  }
  return nullptr;
}

ElementRareDataVector* ElementRareDataVector::SetField(
    FieldId field_id,
    ElementRareDataField* field) {
  ElementRareDataVector* vec = this;
  if (!HasField(field_id)) {
    if (field == nullptr) {
      return vec;
    }
    size_t current_size = size();
    if (current_size >= kMinimumVectorSize &&
        (current_size & (current_size - 1)) == 0) {
      // We're at a power of two elements, so we're out of capacity and need to
      // reallocate.
      size_t new_size = std::max<size_t>(current_size * 2, 1);
      vec = MakeGarbageCollected<ElementRareDataVector>(
          AdditionalBytes(kSlotSizeBytes * new_size), PassKey(),
          std::move(*this));
    }

    // Update the bitfield first, so that if we're tracing in parallel,
    // we're not missing the last field. AdditionalBytes is guaranteed to
    // initially be zero, so tracing the newly visible member is safe.
    vec->fields_bitfield_ |= FieldIdMask(field_id);

    size_t idx = GetFieldIndex(field_id);
    UNSAFE_BUFFERS(
        VectorTypeOperations<Member<ElementRareDataField>, HeapAllocator>::
            MoveOverlapping(vec->ArrayBase() + idx,
                            vec->ArrayBase() + current_size,
                            vec->ArrayBase() + idx + 1,
                            VectorOperationOrigin::kRegularModification));
  }
  vec->ArraySlot(field_id) = field;
  return vec;
}

void ElementRareDataVector::SetFieldToNullIfExists(FieldId field_id) {
  ElementRareDataVector* vec = this;
  if (HasField(field_id)) {
    vec->ArraySlot(field_id) = nullptr;
  }
}

NodeListsNodeData* ElementRareDataVector::NodeLists() const {
  return static_cast<NodeListsNodeData*>(GetField(FieldId::kNodeLists));
}

std::pair<std::reference_wrapper<NodeListsNodeData>, ElementRareDataVector*>
ElementRareDataVector::EnsureNodeLists() {
  return EnsureField<NodeListsNodeData>(FieldId::kNodeLists);
}

FlatTreeNodeData* ElementRareDataVector::GetFlatTreeNodeData() const {
  return static_cast<FlatTreeNodeData*>(GetField(FieldId::kFlatTreeNodeData));
}

NodeMutationObserverData* ElementRareDataVector::MutationObserverData() {
  return static_cast<NodeMutationObserverData*>(
      GetField(FieldId::kMutationObserverData));
}
std::pair<std::reference_wrapper<NodeMutationObserverData>,
          ElementRareDataVector*>
ElementRareDataVector::EnsureMutationObserverData() {
  return EnsureField<NodeMutationObserverData>(FieldId::kMutationObserverData);
}

bool ElementRareDataVector::HasPseudoElements() const {
  PseudoElementData* data =
      static_cast<PseudoElementData*>(GetField(FieldId::kPseudoElementData));
  if (!data)
    return false;
  return data->HasPseudoElements();
}
void ElementRareDataVector::ClearPseudoElements() {
  PseudoElementData* data =
      static_cast<PseudoElementData*>(GetField(FieldId::kPseudoElementData));
  if (data) {
    data->ClearPseudoElements();
    SetFieldToNullIfExists(FieldId::kPseudoElementData);
  }
}
ElementRareDataVector* ElementRareDataVector::SetPseudoElement(
    PseudoId pseudo_id,
    PseudoElement* element,
    const AtomicString& document_transition_tag) {
  PseudoElementData* data =
      static_cast<PseudoElementData*>(GetField(FieldId::kPseudoElementData));
  ElementRareDataVector* vec = this;
  if (!data) {
    if (!element)
      return this;
    data = MakeGarbageCollected<PseudoElementData>();
    vec = SetField(FieldId::kPseudoElementData, data);
  }
  data->SetPseudoElement(pseudo_id, element, document_transition_tag);
  return vec;
}
PseudoElement* ElementRareDataVector::GetPseudoElement(
    PseudoId pseudo_id,
    const AtomicString& document_transition_tag) const {
  PseudoElementData* data =
      static_cast<PseudoElementData*>(GetField(FieldId::kPseudoElementData));
  if (!data)
    return nullptr;
  return data->GetPseudoElement(pseudo_id, document_transition_tag);
}

bool ElementRareDataVector::HasScrollButtonOrMarkerGroupPseudos() const {
  PseudoElementData* data =
      static_cast<PseudoElementData*>(GetField(FieldId::kPseudoElementData));
  return data && data->HasScrollButtonOrMarkerGroupPseudos();
}

PseudoElementData::PseudoElementVector
ElementRareDataVector::GetPseudoElements() const {
  PseudoElementData* data =
      static_cast<PseudoElementData*>(GetField(FieldId::kPseudoElementData));
  if (!data)
    return {};
  return data->GetPseudoElements();
}
ElementRareDataVector* ElementRareDataVector::AddColumnPseudoElement(
    ColumnPseudoElement& column_pseudo_element) {
  auto [data, vec] =
      EnsureField<PseudoElementData>(FieldId::kPseudoElementData);
  data.get().AddColumnPseudoElement(column_pseudo_element);
  return vec;
}

const ColumnPseudoElementsVector*
ElementRareDataVector::GetColumnPseudoElements() const {
  PseudoElementData* data =
      static_cast<PseudoElementData*>(GetField(FieldId::kPseudoElementData));
  if (!data) {
    return nullptr;
  }
  return data->GetColumnPseudoElements();
}

ColumnPseudoElement* ElementRareDataVector::GetColumnPseudoElement(
    wtf_size_t idx) const {
  PseudoElementData* data =
      static_cast<PseudoElementData*>(GetField(FieldId::kPseudoElementData));
  if (!data) {
    return nullptr;
  }
  return data->GetColumnPseudoElement(idx);
}

void ElementRareDataVector::ClearColumnPseudoElements(wtf_size_t to_keep) {
  PseudoElementData* data =
      static_cast<PseudoElementData*>(GetField(FieldId::kPseudoElementData));
  if (!data) {
    return;
  }
  data->ClearColumnPseudoElements(to_keep);
}

std::pair<std::reference_wrapper<CSSStyleDeclaration>, ElementRareDataVector*>
ElementRareDataVector::EnsureInlineCSSStyleDeclaration(Element* owner_element) {
  return EnsureField<InlineCSSStyleDeclaration>(FieldId::kCssomWrapper,
                                                owner_element);
}

ShadowRoot* ElementRareDataVector::GetShadowRoot() const {
  return static_cast<ShadowRoot*>(GetField(FieldId::kShadowRoot));
}
ElementRareDataVector* ElementRareDataVector::SetShadowRoot(
    ShadowRoot& shadow_root) {
  DCHECK(!GetField(FieldId::kShadowRoot));
  return SetField(FieldId::kShadowRoot, &shadow_root);
}

NamedNodeMap* ElementRareDataVector::AttributeMap() const {
  return static_cast<NamedNodeMap*>(GetField(FieldId::kAttributeMap));
}
ElementRareDataVector* ElementRareDataVector::SetAttributeMap(
    NamedNodeMap* attribute_map) {
  return SetField(FieldId::kAttributeMap, attribute_map);
}

DOMTokenList* ElementRareDataVector::GetClassList() const {
  return static_cast<DOMTokenList*>(GetField(FieldId::kClassList));
}
ElementRareDataVector* ElementRareDataVector::SetClassList(
    DOMTokenList* class_list) {
  return SetField(FieldId::kClassList, class_list);
}

DatasetDOMStringMap* ElementRareDataVector::Dataset() const {
  return static_cast<DatasetDOMStringMap*>(GetField(FieldId::kDataset));
}
ElementRareDataVector* ElementRareDataVector::SetDataset(
    DatasetDOMStringMap* dataset) {
  return SetField(FieldId::kDataset, dataset);
}

ScrollOffset ElementRareDataVector::SavedLayerScrollOffset() const {
  if (auto* value =
          GetWrappedField<ScrollOffset>(FieldId::kSavedLayerScrollOffset)) {
    return *value;
  }
  static ScrollOffset offset;
  return offset;
}
ElementRareDataVector* ElementRareDataVector::SetSavedLayerScrollOffset(
    ScrollOffset offset) {
  return SetWrappedField<ScrollOffset>(FieldId::kSavedLayerScrollOffset,
                                       offset);
}

ElementAnimations* ElementRareDataVector::GetElementAnimations() {
  return static_cast<ElementAnimations*>(GetField(FieldId::kElementAnimations));
}
ElementRareDataVector* ElementRareDataVector::SetElementAnimations(
    ElementAnimations* element_animations) {
  return SetField(FieldId::kElementAnimations, element_animations);
}

std::pair<std::reference_wrapper<AttrNodeList>, ElementRareDataVector*>
ElementRareDataVector::EnsureAttrNodeList() {
  return EnsureWrappedField<AttrNodeList>(FieldId::kAttrNodeList);
}
AttrNodeList* ElementRareDataVector::GetAttrNodeList() {
  return GetWrappedField<AttrNodeList>(FieldId::kAttrNodeList);
}
void ElementRareDataVector::RemoveAttrNodeList() {
  SetFieldToNullIfExists(FieldId::kAttrNodeList);
}
ElementRareDataVector* ElementRareDataVector::AddAttr(Attr* attr) {
  auto [node_list, vec] = EnsureAttrNodeList();
  node_list.get().push_back(attr);
  return vec;
}

ElementIntersectionObserverData*
ElementRareDataVector::IntersectionObserverData() const {
  return static_cast<ElementIntersectionObserverData*>(
      GetField(FieldId::kIntersectionObserverData));
}
std::pair<std::reference_wrapper<ElementIntersectionObserverData>,
          ElementRareDataVector*>
ElementRareDataVector::EnsureIntersectionObserverData() {
  return EnsureField<ElementIntersectionObserverData>(
      FieldId::kIntersectionObserverData);
}

ContainerQueryEvaluator* ElementRareDataVector::GetContainerQueryEvaluator()
    const {
  ContainerQueryData* container_query_data = GetContainerQueryData();
  if (!container_query_data)
    return nullptr;
  return container_query_data->GetContainerQueryEvaluator();
}
ElementRareDataVector* ElementRareDataVector::SetContainerQueryEvaluator(
    ContainerQueryEvaluator* evaluator) {
  ContainerQueryData* container_query_data = GetContainerQueryData();
  if (container_query_data) {
    container_query_data->SetContainerQueryEvaluator(evaluator);
    return this;
  } else if (evaluator) {
    auto [new_container_query_data, vec] = EnsureContainerQueryData();
    new_container_query_data.get().SetContainerQueryEvaluator(evaluator);
    return vec;
  } else {
    return this;
  }
}

const AtomicString& ElementRareDataVector::GetNonce() const {
  auto* value = GetWrappedField<AtomicString>(FieldId::kNonce);
  return value ? *value : g_null_atom;
}
ElementRareDataVector* ElementRareDataVector::SetNonce(
    const AtomicString& nonce) {
  return SetWrappedField<AtomicString>(FieldId::kNonce, nonce);
}

const AtomicString& ElementRareDataVector::IsValue() const {
  auto* value = GetWrappedField<AtomicString>(FieldId::kIsValue);
  return value ? *value : g_null_atom;
}
ElementRareDataVector* ElementRareDataVector::SetIsValue(
    const AtomicString& is_value) {
  return SetWrappedField<AtomicString>(FieldId::kIsValue, is_value);
}

EditContext* ElementRareDataVector::GetEditContext() const {
  return static_cast<EditContext*>(GetField(FieldId::kEditContext));
}
ElementRareDataVector* ElementRareDataVector::SetEditContext(
    EditContext* edit_context) {
  return SetField(FieldId::kEditContext, edit_context);
}

ElementRareDataVector* ElementRareDataVector::SetPart(DOMTokenList* part) {
  return SetField(FieldId::kPart, part);
}

DOMTokenList* ElementRareDataVector::GetPart() const {
  return static_cast<DOMTokenList*>(GetField(FieldId::kPart));
}

ElementRareDataVector* ElementRareDataVector::SetMarker(DOMTokenList* marker) {
  return SetField(FieldId::kMarker, marker);
}
DOMTokenList* ElementRareDataVector::GetMarker() const {
  return static_cast<DOMTokenList*>(GetField(FieldId::kMarker));
}

ElementRareDataVector* ElementRareDataVector::SetPartNamesMap(
    const AtomicString part_names) {
  auto [names_map, vec] = EnsureField<NamesMap>(FieldId::kPartNamesMap);
  names_map.get().Set(part_names);
  return vec;
}
const NamesMap* ElementRareDataVector::PartNamesMap() const {
  return static_cast<NamesMap*>(GetField(FieldId::kPartNamesMap));
}

std::pair<std::reference_wrapper<InlineStylePropertyMap>,
          ElementRareDataVector*>
ElementRareDataVector::EnsureInlineStylePropertyMap(Element* owner_element) {
  return EnsureField<InlineStylePropertyMap>(FieldId::kCssomMapWrapper,
                                             owner_element);
}
InlineStylePropertyMap* ElementRareDataVector::GetInlineStylePropertyMap() {
  return static_cast<InlineStylePropertyMap*>(
      GetField(FieldId::kCssomMapWrapper));
}

const ElementInternals* ElementRareDataVector::GetElementInternals() const {
  return static_cast<ElementInternals*>(GetField(FieldId::kElementInternals));
}
std::pair<std::reference_wrapper<ElementInternals>, ElementRareDataVector*>
ElementRareDataVector::EnsureElementInternals(HTMLElement& target) {
  return EnsureField<ElementInternals>(FieldId::kElementInternals, target);
}

std::pair<std::reference_wrapper<DisplayLockContext>, ElementRareDataVector*>
ElementRareDataVector::EnsureDisplayLockContext(Element* element) {
  return EnsureField<DisplayLockContext>(FieldId::kDisplayLockContext, element);
}
DisplayLockContext* ElementRareDataVector::GetDisplayLockContext() const {
  return static_cast<DisplayLockContext*>(
      GetField(FieldId::kDisplayLockContext));
}

std::pair<std::reference_wrapper<ContainerQueryData>, ElementRareDataVector*>
ElementRareDataVector::EnsureContainerQueryData() {
  return EnsureField<ContainerQueryData>(FieldId::kContainerQueryData);
}
ContainerQueryData* ElementRareDataVector::GetContainerQueryData() const {
  return static_cast<ContainerQueryData*>(
      GetField(FieldId::kContainerQueryData));
}
void ElementRareDataVector::ClearContainerQueryData() {
  SetFieldToNullIfExists(FieldId::kContainerQueryData);
}

std::pair<std::reference_wrapper<StyleScopeData>, ElementRareDataVector*>
ElementRareDataVector::EnsureStyleScopeData() {
  return EnsureField<StyleScopeData>(FieldId::kStyleScopeData);
}
StyleScopeData* ElementRareDataVector::GetStyleScopeData() const {
  return static_cast<StyleScopeData*>(GetField(FieldId::kStyleScopeData));
}

std::pair<std::reference_wrapper<OutOfFlowData>, ElementRareDataVector*>
ElementRareDataVector::EnsureOutOfFlowData() {
  return EnsureField<OutOfFlowData>(FieldId::kOutOfFlowData);
}

OutOfFlowData* ElementRareDataVector::GetOutOfFlowData() const {
  return static_cast<OutOfFlowData*>(GetField(FieldId::kOutOfFlowData));
}

void ElementRareDataVector::ClearOutOfFlowData() {
  SetFieldToNullIfExists(FieldId::kOutOfFlowData);
}

const RegionCaptureCropId* ElementRareDataVector::GetRegionCaptureCropId()
    const {
  auto* value = GetWrappedField<std::unique_ptr<RegionCaptureCropId>>(
      FieldId::kRegionCaptureCropId);
  return value ? value->get() : nullptr;
}
ElementRareDataVector* ElementRareDataVector::SetRegionCaptureCropId(
    std::unique_ptr<RegionCaptureCropId> crop_id) {
  CHECK(!GetRegionCaptureCropId());
  CHECK(crop_id);
  CHECK(!crop_id->value().is_zero());
  return SetWrappedField<std::unique_ptr<RegionCaptureCropId>>(
      FieldId::kRegionCaptureCropId, std::move(crop_id));
}

const RestrictionTargetId* ElementRareDataVector::GetRestrictionTargetId()
    const {
  auto* value = GetWrappedField<std::unique_ptr<RestrictionTargetId>>(
      FieldId::kRestrictionTargetId);
  return value ? value->get() : nullptr;
}
ElementRareDataVector* ElementRareDataVector::SetRestrictionTargetId(
    std::unique_ptr<RestrictionTargetId> id) {
  CHECK(!GetRestrictionTargetId());
  CHECK(id);
  CHECK(!id->value().is_zero());
  return SetWrappedField<std::unique_ptr<RestrictionTargetId>>(
      FieldId::kRestrictionTargetId, std::move(id));
}

const TrackedElementRect* ElementRareDataVector::GetTrackedElementRect() const {
  auto* value = GetWrappedField<std::unique_ptr<TrackedElementRect>>(
      FieldId::kTrackedElementRect);
  return value ? value->get() : nullptr;
}

void ElementRareDataVector::ClearTrackedElementRect() {
  SetFieldToNullIfExists(FieldId::kTrackedElementRect);
}

ElementRareDataVector* ElementRareDataVector::SetTrackedElementRect(
    std::unique_ptr<TrackedElementRect> rect) {
  CHECK(!GetTrackedElementRect());
  CHECK(rect);
  CHECK(!rect->id.value().is_zero());
  return SetWrappedField<std::unique_ptr<TrackedElementRect>>(
      FieldId::kTrackedElementRect, std::move(rect));
}

ElementRareDataVector::ResizeObserverDataMap*
ElementRareDataVector::ResizeObserverData() const {
  return GetWrappedField<ElementRareDataVector::ResizeObserverDataMap>(
      FieldId::kResizeObserverData);
}

std::pair<std::reference_wrapper<ElementRareDataVector::ResizeObserverDataMap>,
          ElementRareDataVector*>
ElementRareDataVector::EnsureResizeObserverData() {
  return EnsureWrappedField<ElementRareDataVector::ResizeObserverDataMap>(
      FieldId::kResizeObserverData);
}

ElementRareDataVector* ElementRareDataVector::SetCustomElementDefinition(
    CustomElementDefinition* definition) {
  return SetField(FieldId::kCustomElementDefinition, definition);
}
CustomElementDefinition* ElementRareDataVector::GetCustomElementDefinition()
    const {
  return static_cast<CustomElementDefinition*>(
      GetField(FieldId::kCustomElementDefinition));
}

ElementRareDataVector* ElementRareDataVector::SetLastRememberedBlockSize(
    std::optional<LayoutUnit> size) {
  return SetOptionalField(FieldId::kLastRememberedBlockSize, size);
}
ElementRareDataVector* ElementRareDataVector::SetLastRememberedInlineSize(
    std::optional<LayoutUnit> size) {
  return SetOptionalField(FieldId::kLastRememberedInlineSize, size);
}

std::optional<LayoutUnit> ElementRareDataVector::LastRememberedBlockSize()
    const {
  return GetOptionalField<LayoutUnit>(FieldId::kLastRememberedBlockSize);
}
std::optional<LayoutUnit> ElementRareDataVector::LastRememberedInlineSize()
    const {
  return GetOptionalField<LayoutUnit>(FieldId::kLastRememberedInlineSize);
}

PopoverData* ElementRareDataVector::GetPopoverData() const {
  return static_cast<PopoverData*>(GetField(FieldId::kPopoverData));
}
std::pair<std::reference_wrapper<PopoverData>, ElementRareDataVector*>
ElementRareDataVector::EnsurePopoverData() {
  return EnsureField<PopoverData>(FieldId::kPopoverData);
}
void ElementRareDataVector::RemovePopoverData() {
  SetFieldToNullIfExists(FieldId::kPopoverData);
}

InvokerData* ElementRareDataVector::GetInvokerData() const {
  return static_cast<InvokerData*>(GetField(FieldId::kInvokerData));
}
std::pair<std::reference_wrapper<InvokerData>, ElementRareDataVector*>
ElementRareDataVector::EnsureInvokerData() {
  return EnsureField<InvokerData>(FieldId::kInvokerData);
}
InterestInvokerTargetData* ElementRareDataVector::GetInterestInvokerTargetData()
    const {
  return static_cast<InterestInvokerTargetData*>(
      GetField(FieldId::kInterestInvokerTargetData));
}
std::pair<std::reference_wrapper<InterestInvokerTargetData>,
          ElementRareDataVector*>
ElementRareDataVector::EnsureInterestInvokerTargetData() {
  return EnsureField<InterestInvokerTargetData>(
      FieldId::kInterestInvokerTargetData);
}
void ElementRareDataVector::RemoveInterestInvokerTargetData() {
  SetFieldToNullIfExists(FieldId::kInterestInvokerTargetData);
}

ScrollMarkerGroupData* ElementRareDataVector::GetScrollMarkerGroupData() const {
  return static_cast<ScrollMarkerGroupData*>(
      GetField(FieldId::kScrollMarkerGroupData));
}
void ElementRareDataVector::RemoveScrollMarkerGroupData() {
  SetFieldToNullIfExists(FieldId::kScrollMarkerGroupData);
}
std::pair<std::reference_wrapper<ScrollMarkerGroupData>, ElementRareDataVector*>
ElementRareDataVector::EnsureScrollMarkerGroupData(Element* element) {
  return EnsureField<ScrollMarkerGroupData>(FieldId::kScrollMarkerGroupData,
                                            element->GetDocument().GetFrame());
}

ElementRareDataVector* ElementRareDataVector::SetScrollMarkerGroupContainerData(
    ScrollMarkerGroupData* data) {
  return SetField(FieldId::kScrollMarkerGroupContainerData, data);
}
ScrollMarkerGroupData*
ElementRareDataVector::GetScrollMarkerGroupContainerData() const {
  return static_cast<ScrollMarkerGroupData*>(
      GetField(FieldId::kScrollMarkerGroupContainerData));
}

ElementRareDataVector* ElementRareDataVector::CacheCSSPseudoElement(
    PseudoId pseudo_id,
    CSSPseudoElement& pseudo_element) {
  auto [data, vec] =
      EnsureField<CSSPseudoElementsCacheData>(FieldId::kCSSPseudoElementData);
  data.get().CacheCSSPseudoElement(pseudo_id, pseudo_element);
  return vec;
}

CSSPseudoElement* ElementRareDataVector::GetCSSPseudoElement(
    PseudoId pseudo_id) const {
  auto* data = static_cast<CSSPseudoElementsCacheData*>(
      GetField(FieldId::kCSSPseudoElementData));
  if (!data) {
    return {};
  }
  return data->GetCSSPseudoElement(pseudo_id);
}

AnchorPositionScrollData* ElementRareDataVector::GetAnchorPositionScrollData()
    const {
  return static_cast<AnchorPositionScrollData*>(
      GetField(FieldId::kAnchorPositionScrollData));
}

void ElementRareDataVector::RemoveAnchorPositionScrollData() {
  if (auto* scroll_data = GetAnchorPositionScrollData()) {
    if (auto* observer = scroll_data->GetAnchorPositionVisibilityObserver()) {
      observer->MonitorAnchor(nullptr);
    }
  }
  SetFieldToNullIfExists(FieldId::kAnchorPositionScrollData);
}

std::pair<std::reference_wrapper<AnchorPositionScrollData>,
          ElementRareDataVector*>
ElementRareDataVector::EnsureAnchorPositionScrollData(
    Element* anchored_element) {
  DCHECK(!GetAnchorPositionScrollData() ||
         GetAnchorPositionScrollData()->AnchoredElement() == anchored_element);
  return EnsureField<AnchorPositionScrollData>(
      FieldId::kAnchorPositionScrollData, anchored_element);
}

ExplicitlySetAttrElementsMap*
ElementRareDataVector::GetExplicitlySetElementsForAttr() const {
  return static_cast<ExplicitlySetAttrElementsMap*>(
      GetField(FieldId::kExplicitlySetElementsForAttr));
}

std::pair<std::reference_wrapper<ExplicitlySetAttrElementsMap>,
          ElementRareDataVector*>
ElementRareDataVector::EnsureExplicitlySetElementsForAttr() {
  return EnsureField<ExplicitlySetAttrElementsMap>(
      FieldId::kExplicitlySetElementsForAttr);
}

bool ElementRareDataVector::HasCustomElementRegistrySet() const {
  DCHECK(RuntimeEnabledFeatures::ScopedCustomElementRegistryEnabled());
  return flags_.has_custom_element_registry_;
}

CustomElementRegistry* ElementRareDataVector::GetCustomElementRegistry() const {
  DCHECK(RuntimeEnabledFeatures::ScopedCustomElementRegistryEnabled());
  DCHECK(HasCustomElementRegistrySet());
  return static_cast<CustomElementRegistry*>(
      GetField(FieldId::kCustomElementRegistry));
}

ElementRareDataVector* ElementRareDataVector::SetCustomElementRegistry(
    CustomElementRegistry* registry) {
  // An element's custom element registry should only be set once unless the
  // registry is a global registry and can be reset during cross document node
  // adoption.
  DCHECK(RuntimeEnabledFeatures::ScopedCustomElementRegistryEnabled());
  DCHECK(!GetField(FieldId::kCustomElementRegistry) ||
         static_cast<CustomElementRegistry*>(
             GetField(FieldId::kCustomElementRegistry))
             ->IsGlobalRegistry());
  // We intentionally don't use ElementRareDataVector::SetField because it will
  // erase the field if we set null to the field. However, when we want an
  // element to have null registry explicitly, we want to keep the existence of
  // field while setting it to null.
  flags_.has_custom_element_registry_ = true;
  return SetField(FieldId::kCustomElementRegistry, registry);
}

void ElementRareDataVector::ClearCustomElementRegistry() {
  DCHECK(RuntimeEnabledFeatures::ScopedCustomElementRegistryEnabled());
  flags_.has_custom_element_registry_ = false;
  SetFieldToNullIfExists(FieldId::kCustomElementRegistry);
}

ElementAnimationTriggerData* ElementRareDataVector::AnimationTriggerData() {
  return static_cast<ElementAnimationTriggerData*>(
      GetField(FieldId::kAnimationTriggerData));
}

std::pair<std::reference_wrapper<ElementAnimationTriggerData>,
          ElementRareDataVector*>
ElementRareDataVector::EnsureAnimationTriggerData() {
  return EnsureField<ElementAnimationTriggerData>(
      FieldId::kAnimationTriggerData);
}

DisplayAdElementMonitor* ElementRareDataVector::GetDisplayAdElementMonitor()
    const {
  return static_cast<DisplayAdElementMonitor*>(
      GetField(FieldId::kDisplayAdElementMonitor));
}

std::pair<std::reference_wrapper<DisplayAdElementMonitor>,
          ElementRareDataVector*>
ElementRareDataVector::EnsureDisplayAdElementMonitor(Element* element) {
  return EnsureField<DisplayAdElementMonitor>(FieldId::kDisplayAdElementMonitor,
                                              element);
}

ElementRareDataVector* ElementRareDataVector::SetFocusgroupLastFocused(
    Element* element) {
  // Store weak reference, this should not keep the element alive.
  return SetWrappedField<WeakMember<Element>>(FieldId::kFocusgroupLastFocused,
                                              element);
}

Element* ElementRareDataVector::GetFocusgroupLastFocused() const {
  if (auto* value = GetWrappedField<WeakMember<Element>>(
          FieldId::kFocusgroupLastFocused)) {
    return value->Get();
  }
  return nullptr;
}

ContentData* ElementRareDataVector::GetAltContentData() const {
  if (auto* value =
          GetWrappedField<Member<ContentData>>(FieldId::kAltContentData)) {
    return value->Get();
  }
  return nullptr;
}

ElementRareDataVector* ElementRareDataVector::SetAltContentData(
    ContentData* content_data) {
  if (content_data) {
    return SetWrappedField<Member<ContentData>>(FieldId::kAltContentData,
                                                content_data);
  } else {
    SetFieldToNullIfExists(FieldId::kAltContentData);
    return this;
  }
}

ElementRareDataVector* ElementRareDataVector::SetOverscrollContainer(
    Element* element) {
  return SetWrappedField<WeakMember<Element>>(FieldId::kOverscrollContainer,
                                              element);
}

Element* ElementRareDataVector::GetOverscrollContainer() const {
  if (auto* value =
          GetWrappedField<WeakMember<Element>>(FieldId::kOverscrollContainer)) {
    return value->Get();
  }
  return nullptr;
}

std::pair<std::reference_wrapper<OverscrollAreaTracker>, ElementRareDataVector*>
ElementRareDataVector::EnsureOverscrollAreaTracker(Element* element) {
  return EnsureField<class OverscrollAreaTracker>(
      FieldId::kOverscrollAreaTracker, element);
}
OverscrollAreaTracker* ElementRareDataVector::OverscrollAreaTracker() const {
  return static_cast<class OverscrollAreaTracker*>(
      GetField(FieldId::kOverscrollAreaTracker));
}

void ElementRareDataVector::Trace(blink::Visitor* visitor) const {
  visitor->TraceMultiple(ArrayBase(), size());
}

void NodeMutationObserverData::Trace(Visitor* visitor) const {
  ElementRareDataField::Trace(visitor);
  visitor->Trace(registry_);
  visitor->Trace(transient_registry_);
}

void ScrollTimelineHashSet::Trace(Visitor* visitor) const {
  ElementRareDataField::Trace(visitor);
  visitor->Trace(set_);
}

void NodeMutationObserverData::AddTransientRegistration(
    MutationObserverRegistration* registration) {
  transient_registry_.insert(registration);
}

void NodeMutationObserverData::RemoveTransientRegistration(
    MutationObserverRegistration* registration) {
  DCHECK(transient_registry_.Contains(registration));
  transient_registry_.erase(registration);
}

void NodeMutationObserverData::AddRegistration(
    MutationObserverRegistration* registration) {
  registry_.push_back(registration);
}

void NodeMutationObserverData::RemoveRegistration(
    MutationObserverRegistration* registration) {
  DCHECK(registry_.Contains(registration));
  registry_.EraseAt(registry_.Find(registration));
}

ElementRareDataVector* ElementRareDataVector::RegisterScrollTimeline(
    ScrollTimeline* timeline) {
  auto [timeline_set, vec] =
      EnsureField<ScrollTimelineHashSet>(FieldId::kScrollTimelines);
  timeline_set.get().set_.insert(timeline);
  return vec;
}
ElementRareDataVector* ElementRareDataVector::UnregisterScrollTimeline(
    ScrollTimeline* timeline) {
  auto [timeline_set, vec] =
      EnsureField<ScrollTimelineHashSet>(FieldId::kScrollTimelines);
  timeline_set.get().set_.erase(timeline);
  return vec;
}

void ElementRareDataVector::IncrementConnectedSubframeCount() {
  SECURITY_CHECK((flags_.connected_frame_count_ + 1) <=
                 Page::MaxNumberOfFrames());
  ++flags_.connected_frame_count_;
}

std::pair<std::reference_wrapper<FlatTreeNodeData>, ElementRareDataVector*>
ElementRareDataVector::EnsureFlatTreeNodeData() {
  return EnsureField<FlatTreeNodeData>(FieldId::kFlatTreeNodeData);
}

static_assert(static_cast<int>(ElementRareDataVector::kNumberOfElementFlags) ==
                  static_cast<int>(ElementFlags::kNumberOfElementFlags),
              "kNumberOfElementFlags must match.");
static_assert(
    static_cast<int>(ElementRareDataVector::kNumberOfDynamicRestyleFlags) ==
        static_cast<int>(DynamicRestyleFlags::kNumberOfDynamicRestyleFlags),
    "kNumberOfDynamicRestyleFlags must match.");

}  // namespace blink
