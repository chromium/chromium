// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/node_rare_data.h"

#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/css/container_query_data.h"
#include "third_party/blink/renderer/core/css/cssom/inline_style_property_map.h"
#include "third_party/blink/renderer/core/css/inline_css_style_declaration.h"
#include "third_party/blink/renderer/core/css/out_of_flow_data.h"
#include "third_party/blink/renderer/core/css/style_scope_data.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/css_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/dataset_dom_string_map.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_animation_trigger_data.h"
#include "third_party/blink/renderer/core/dom/explicitly_set_attr_elements_map.h"
#include "third_party/blink/renderer/core/dom/flat_tree_node_data.h"
#include "third_party/blink/renderer/core/dom/interest_invoker_target_data.h"
#include "third_party/blink/renderer/core/dom/invoker_data.h"
#include "third_party/blink/renderer/core/dom/mutation_observer_registration.h"
#include "third_party/blink/renderer/core/dom/named_node_map.h"
#include "third_party/blink/renderer/core/dom/names_map.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/dom/node_rare_data.h"
#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_group_data.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/ime/edit_context.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/display_ad_element_monitor.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/unbounded_event_data.h"
#include "third_party/blink/renderer/core/intersection_observer/element_intersection_observer_data.h"
#include "third_party/blink/renderer/core/layout/anchor_position_scroll_data.h"
#include "third_party/blink/renderer/core/layout/anchor_position_visibility_observer.h"
#include "third_party/blink/renderer/core/overscroll/overscroll_area_tracker.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observation.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/core/style/content_data.h"
#include "third_party/blink/renderer/platform/graphics/paint/tracked_element_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

NodeRareData::~NodeRareData() {
  DCHECK(!GetField(FieldId::kPseudoElementData));
}

NodeRareDataField* NodeRareData::GetField(FieldId field_id) const {
  if (HasField(field_id)) {
    return ArraySlot(field_id);
  }
  return nullptr;
}

NodeRareData* NodeRareData::SetField(FieldId field_id,
                                     NodeRareDataField* field) {
  NodeRareData* vec = this;
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
      vec = MakeGarbageCollected<NodeRareData>(
          AdditionalBytes(kSlotSizeBytes * new_size), PassKey(),
          std::move(*this));
    }

    // Update the bitfield first, so that if we're tracing in parallel,
    // we're not missing the last field. AdditionalBytes is guaranteed to
    // initially be zero, so tracing the newly visible member is safe.
    vec->fields_bitfield_ |= FieldIdMask(field_id);

    size_t idx = GetFieldIndex(field_id);
    UNSAFE_BUFFERS(
        VectorTypeOperations<Member<NodeRareDataField>, HeapAllocator>::
            MoveOverlapping(vec->ArrayBase() + idx,
                            vec->ArrayBase() + current_size,
                            vec->ArrayBase() + idx + 1,
                            VectorOperationOrigin::kRegularModification));
  }
  vec->ArraySlot(field_id) = field;
  return vec;
}

void NodeRareData::SetFieldToNullIfExists(FieldId field_id) {
  NodeRareData* vec = this;
  if (HasField(field_id)) {
    vec->ArraySlot(field_id) = nullptr;
  }
}

NodeListsNodeData* NodeRareData::NodeLists() const {
  return static_cast<NodeListsNodeData*>(GetField(FieldId::kNodeLists));
}

RareDataUpdate<NodeListsNodeData> NodeRareData::EnsureNodeLists() {
  return EnsureField<NodeListsNodeData>(FieldId::kNodeLists);
}

FlatTreeNodeData* NodeRareData::GetFlatTreeNodeData() const {
  return static_cast<FlatTreeNodeData*>(GetField(FieldId::kFlatTreeNodeData));
}

NodeMutationObserverData* NodeRareData::MutationObserverData() {
  return static_cast<NodeMutationObserverData*>(
      GetField(FieldId::kMutationObserverData));
}
RareDataUpdate<NodeMutationObserverData>
NodeRareData::EnsureMutationObserverData() {
  return EnsureField<NodeMutationObserverData>(FieldId::kMutationObserverData);
}

bool NodeRareData::HasPseudoElements() const {
  PseudoElementData* data =
      static_cast<PseudoElementData*>(GetField(FieldId::kPseudoElementData));
  if (!data) {
    return false;
  }
  return data->HasPseudoElements();
}
void NodeRareData::ClearPseudoElements() {
  PseudoElementData* data =
      static_cast<PseudoElementData*>(GetField(FieldId::kPseudoElementData));
  if (data) {
    data->ClearPseudoElements();
    SetFieldToNullIfExists(FieldId::kPseudoElementData);
  }
}
NodeRareData* NodeRareData::SetPseudoElement(
    PseudoId pseudo_id,
    PseudoElement* element,
    const AtomicString& document_transition_tag) {
  PseudoElementData* data =
      static_cast<PseudoElementData*>(GetField(FieldId::kPseudoElementData));
  NodeRareData* vec = this;
  if (!data) {
    if (!element) {
      return this;
    }
    data = MakeGarbageCollected<PseudoElementData>();
    vec = SetField(FieldId::kPseudoElementData, data);
  }
  data->SetPseudoElement(pseudo_id, element, document_transition_tag);
  return vec;
}
PseudoElement* NodeRareData::GetPseudoElement(
    PseudoId pseudo_id,
    const AtomicString& document_transition_tag) const {
  PseudoElementData* data =
      static_cast<PseudoElementData*>(GetField(FieldId::kPseudoElementData));
  if (!data) {
    return nullptr;
  }
  return data->GetPseudoElement(pseudo_id, document_transition_tag);
}

bool NodeRareData::HasAnyPseudos() const {
  return GetField(FieldId::kPseudoElementData);
}

bool NodeRareData::HasScrollButtonOrMarkerGroupPseudos() const {
  PseudoElementData* data =
      static_cast<PseudoElementData*>(GetField(FieldId::kPseudoElementData));
  return data && data->HasScrollButtonOrMarkerGroupPseudos();
}

PseudoElementData::PseudoElementVector NodeRareData::GetPseudoElements() const {
  PseudoElementData* data =
      static_cast<PseudoElementData*>(GetField(FieldId::kPseudoElementData));
  if (!data) {
    return {};
  }
  return data->GetPseudoElements();
}
NodeRareData* NodeRareData::AddColumnPseudoElement(
    ColumnPseudoElement& column_pseudo_element) {
  auto update = EnsureField<PseudoElementData>(FieldId::kPseudoElementData);
  update.field_->AddColumnPseudoElement(column_pseudo_element);
  return update.rare_data_;
}

const ColumnPseudoElementsVector* NodeRareData::GetColumnPseudoElements()
    const {
  PseudoElementData* data =
      static_cast<PseudoElementData*>(GetField(FieldId::kPseudoElementData));
  if (!data) {
    return nullptr;
  }
  return data->GetColumnPseudoElements();
}

ColumnPseudoElement* NodeRareData::GetColumnPseudoElement(
    wtf_size_t idx) const {
  PseudoElementData* data =
      static_cast<PseudoElementData*>(GetField(FieldId::kPseudoElementData));
  if (!data) {
    return nullptr;
  }
  return data->GetColumnPseudoElement(idx);
}

void NodeRareData::ClearColumnPseudoElements(wtf_size_t to_keep) {
  PseudoElementData* data =
      static_cast<PseudoElementData*>(GetField(FieldId::kPseudoElementData));
  if (!data) {
    return;
  }
  data->ClearColumnPseudoElements(to_keep);
}

RareDataUpdate<CSSStyleDeclaration>
NodeRareData::EnsureInlineCSSStyleDeclaration(Element* owner_element) {
  return EnsureField<InlineCSSStyleDeclaration>(FieldId::kCssomWrapper,
                                                owner_element);
}

ShadowRoot* NodeRareData::GetShadowRoot() const {
  return static_cast<ShadowRoot*>(GetField(FieldId::kShadowRoot));
}
NodeRareData* NodeRareData::SetShadowRoot(ShadowRoot& shadow_root) {
  DCHECK(!GetField(FieldId::kShadowRoot));
  return SetField(FieldId::kShadowRoot, &shadow_root);
}

NamedNodeMap* NodeRareData::AttributeMap() const {
  return static_cast<NamedNodeMap*>(GetField(FieldId::kAttributeMap));
}
NodeRareData* NodeRareData::SetAttributeMap(NamedNodeMap* attribute_map) {
  return SetField(FieldId::kAttributeMap, attribute_map);
}

DOMTokenList* NodeRareData::GetClassList() const {
  return static_cast<DOMTokenList*>(GetField(FieldId::kClassList));
}
NodeRareData* NodeRareData::SetClassList(DOMTokenList* class_list) {
  return SetField(FieldId::kClassList, class_list);
}

DOMTokenList* NodeRareData::GetFocusgroupTokenList() const {
  return static_cast<DOMTokenList*>(GetField(FieldId::kFocusgroupTokenList));
}
NodeRareData* NodeRareData::SetFocusgroupTokenList(DOMTokenList* token_list) {
  return SetField(FieldId::kFocusgroupTokenList, token_list);
}

DatasetDOMStringMap* NodeRareData::Dataset() const {
  return static_cast<DatasetDOMStringMap*>(GetField(FieldId::kDataset));
}
NodeRareData* NodeRareData::SetDataset(DatasetDOMStringMap* dataset) {
  return SetField(FieldId::kDataset, dataset);
}

ScrollOffset NodeRareData::SavedLayerScrollOffset() const {
  if (auto* value =
          GetWrappedField<ScrollOffset>(FieldId::kSavedLayerScrollOffset)) {
    return *value;
  }
  static ScrollOffset offset;
  return offset;
}
NodeRareData* NodeRareData::SetSavedLayerScrollOffset(ScrollOffset offset) {
  return SetWrappedField<ScrollOffset>(FieldId::kSavedLayerScrollOffset,
                                       offset);
}

ElementAnimations* NodeRareData::GetElementAnimations() {
  return static_cast<ElementAnimations*>(GetField(FieldId::kElementAnimations));
}
NodeRareData* NodeRareData::SetElementAnimations(
    ElementAnimations* element_animations) {
  return SetField(FieldId::kElementAnimations, element_animations);
}

RareDataUpdate<AttrNodeList> NodeRareData::EnsureAttrNodeList() {
  return EnsureWrappedField<AttrNodeList>(FieldId::kAttrNodeList);
}
AttrNodeList* NodeRareData::GetAttrNodeList() {
  return GetWrappedField<AttrNodeList>(FieldId::kAttrNodeList);
}
void NodeRareData::RemoveAttrNodeList() {
  SetFieldToNullIfExists(FieldId::kAttrNodeList);
}
NodeRareData* NodeRareData::AddAttr(Attr* attr) {
  auto update = EnsureAttrNodeList();
  update.field_->push_back(attr);
  return update.rare_data_;
}

ElementIntersectionObserverData* NodeRareData::IntersectionObserverData()
    const {
  return static_cast<ElementIntersectionObserverData*>(
      GetField(FieldId::kIntersectionObserverData));
}
RareDataUpdate<ElementIntersectionObserverData>
NodeRareData::EnsureIntersectionObserverData() {
  return EnsureField<ElementIntersectionObserverData>(
      FieldId::kIntersectionObserverData);
}

ContainerQueryEvaluator* NodeRareData::GetContainerQueryEvaluator() const {
  ContainerQueryData* container_query_data = GetContainerQueryData();
  if (!container_query_data) {
    return nullptr;
  }
  return container_query_data->GetContainerQueryEvaluator();
}
NodeRareData* NodeRareData::SetContainerQueryEvaluator(
    ContainerQueryEvaluator* evaluator) {
  ContainerQueryData* container_query_data = GetContainerQueryData();
  if (container_query_data) {
    container_query_data->SetContainerQueryEvaluator(evaluator);
    return this;
  } else if (evaluator) {
    auto update = EnsureContainerQueryData();
    update.field_->SetContainerQueryEvaluator(evaluator);
    return update.rare_data_;
  } else {
    return this;
  }
}

const AtomicString& NodeRareData::GetNonce() const {
  auto* value = GetWrappedField<AtomicString>(FieldId::kNonce);
  return value ? *value : g_null_atom;
}
NodeRareData* NodeRareData::SetNonce(const AtomicString& nonce) {
  return SetWrappedField<AtomicString>(FieldId::kNonce, nonce);
}

const AtomicString& NodeRareData::IsValue() const {
  auto* value = GetWrappedField<AtomicString>(FieldId::kIsValue);
  return value ? *value : g_null_atom;
}
NodeRareData* NodeRareData::SetIsValue(const AtomicString& is_value) {
  return SetWrappedField<AtomicString>(FieldId::kIsValue, is_value);
}

EditContext* NodeRareData::GetEditContext() const {
  return static_cast<EditContext*>(GetField(FieldId::kEditContext));
}
NodeRareData* NodeRareData::SetEditContext(EditContext* edit_context) {
  return SetField(FieldId::kEditContext, edit_context);
}

NodeRareData* NodeRareData::SetPart(DOMTokenList* part) {
  return SetField(FieldId::kPart, part);
}

DOMTokenList* NodeRareData::GetPart() const {
  return static_cast<DOMTokenList*>(GetField(FieldId::kPart));
}

NodeRareData* NodeRareData::SetPartNamesMap(const AtomicString part_names) {
  auto update = EnsureField<NamesMap>(FieldId::kPartNamesMap);
  update.field_->Set(part_names);
  return update.rare_data_;
}
const NamesMap* NodeRareData::PartNamesMap() const {
  return static_cast<NamesMap*>(GetField(FieldId::kPartNamesMap));
}

RareDataUpdate<InlineStylePropertyMap>
NodeRareData::EnsureInlineStylePropertyMap(Element* owner_element) {
  return EnsureField<InlineStylePropertyMap>(FieldId::kCssomMapWrapper,
                                             owner_element);
}
InlineStylePropertyMap* NodeRareData::GetInlineStylePropertyMap() {
  return static_cast<InlineStylePropertyMap*>(
      GetField(FieldId::kCssomMapWrapper));
}

const ElementInternals* NodeRareData::GetElementInternals() const {
  return static_cast<ElementInternals*>(GetField(FieldId::kElementInternals));
}
RareDataUpdate<ElementInternals> NodeRareData::EnsureElementInternals(
    HTMLElement& target) {
  return EnsureField<ElementInternals>(FieldId::kElementInternals, target);
}

RareDataUpdate<DisplayLockContext> NodeRareData::EnsureDisplayLockContext(
    Element* element) {
  return EnsureField<DisplayLockContext>(FieldId::kDisplayLockContext, element);
}
DisplayLockContext* NodeRareData::GetDisplayLockContext() const {
  return static_cast<DisplayLockContext*>(
      GetField(FieldId::kDisplayLockContext));
}

RareDataUpdate<ContainerQueryData> NodeRareData::EnsureContainerQueryData() {
  return EnsureField<ContainerQueryData>(FieldId::kContainerQueryData);
}
ContainerQueryData* NodeRareData::GetContainerQueryData() const {
  return static_cast<ContainerQueryData*>(
      GetField(FieldId::kContainerQueryData));
}
void NodeRareData::ClearContainerQueryData() {
  SetFieldToNullIfExists(FieldId::kContainerQueryData);
}

RareDataUpdate<StyleScopeData> NodeRareData::EnsureStyleScopeData() {
  return EnsureField<StyleScopeData>(FieldId::kStyleScopeData);
}
StyleScopeData* NodeRareData::GetStyleScopeData() const {
  return static_cast<StyleScopeData*>(GetField(FieldId::kStyleScopeData));
}

RareDataUpdate<OutOfFlowData> NodeRareData::EnsureOutOfFlowData() {
  return EnsureField<OutOfFlowData>(FieldId::kOutOfFlowData);
}

OutOfFlowData* NodeRareData::GetOutOfFlowData() const {
  return static_cast<OutOfFlowData*>(GetField(FieldId::kOutOfFlowData));
}

void NodeRareData::ClearOutOfFlowData() {
  SetFieldToNullIfExists(FieldId::kOutOfFlowData);
}

const RegionCaptureCropId* NodeRareData::GetRegionCaptureCropId() const {
  auto* value = GetWrappedField<std::unique_ptr<RegionCaptureCropId>>(
      FieldId::kRegionCaptureCropId);
  return value ? value->get() : nullptr;
}
NodeRareData* NodeRareData::SetRegionCaptureCropId(
    std::unique_ptr<RegionCaptureCropId> crop_id) {
  CHECK(!GetRegionCaptureCropId());
  CHECK(crop_id);
  CHECK(!crop_id->value().is_zero());
  return SetWrappedField<std::unique_ptr<RegionCaptureCropId>>(
      FieldId::kRegionCaptureCropId, std::move(crop_id));
}

const RestrictionTargetId* NodeRareData::GetRestrictionTargetId() const {
  auto* value = GetWrappedField<std::unique_ptr<RestrictionTargetId>>(
      FieldId::kRestrictionTargetId);
  return value ? value->get() : nullptr;
}
NodeRareData* NodeRareData::SetRestrictionTargetId(
    std::unique_ptr<RestrictionTargetId> id) {
  CHECK(!GetRestrictionTargetId());
  CHECK(id);
  CHECK(!id->value().is_zero());
  return SetWrappedField<std::unique_ptr<RestrictionTargetId>>(
      FieldId::kRestrictionTargetId, std::move(id));
}

const TrackedElementSubRect* NodeRareData::GetTrackedElementSubRect(
    viz::TrackedElementFeature feature) const {
  if (auto* map = GetTrackedElementSubRects()) {
    auto it = map->find(feature);
    if (it != map->end()) {
      return &it->second;
    }
  }
  return nullptr;
}

void NodeRareData::ClearTrackedElementSubRect(
    viz::TrackedElementFeature feature) {
  if (auto* map = GetWrappedField<TrackedElementSubRects>(
          FieldId::kTrackedElementRect)) {
    map->erase(feature);
    // If no more features are tracking this element, remove the field entirely.
    if (map->empty()) {
      SetFieldToNullIfExists(FieldId::kTrackedElementRect);
    }
  }
}

NodeRareData* NodeRareData::SetTrackedElementSubRect(
    viz::TrackedElementFeature feature,
    const TrackedElementSubRect& rect) {
  CHECK(!rect.id.value().is_zero());
  auto update =
      EnsureWrappedField<TrackedElementSubRects>(FieldId::kTrackedElementRect);
  auto [_, inserted] = update.field_->try_emplace(feature, rect);
  CHECK(inserted);
  return update.rare_data_;
}

const TrackedElementSubRects* NodeRareData::GetTrackedElementSubRects() const {
  return GetWrappedField<TrackedElementSubRects>(FieldId::kTrackedElementRect);
}

NodeRareData::ResizeObserverDataMap* NodeRareData::ResizeObserverData() const {
  return GetWrappedField<NodeRareData::ResizeObserverDataMap>(
      FieldId::kResizeObserverData);
}

RareDataUpdate<NodeRareData::ResizeObserverDataMap>
NodeRareData::EnsureResizeObserverData() {
  return EnsureWrappedField<NodeRareData::ResizeObserverDataMap>(
      FieldId::kResizeObserverData);
}

NodeRareData* NodeRareData::SetCustomElementDefinition(
    CustomElementDefinition* definition) {
  return SetField(FieldId::kCustomElementDefinition, definition);
}
CustomElementDefinition* NodeRareData::GetCustomElementDefinition() const {
  return static_cast<CustomElementDefinition*>(
      GetField(FieldId::kCustomElementDefinition));
}

NodeRareData* NodeRareData::SetLastRememberedBlockSize(
    std::optional<LayoutUnit> size) {
  return SetOptionalField(FieldId::kLastRememberedBlockSize, size);
}
NodeRareData* NodeRareData::SetLastRememberedInlineSize(
    std::optional<LayoutUnit> size) {
  return SetOptionalField(FieldId::kLastRememberedInlineSize, size);
}

std::optional<LayoutUnit> NodeRareData::LastRememberedBlockSize() const {
  return GetOptionalField<LayoutUnit>(FieldId::kLastRememberedBlockSize);
}
std::optional<LayoutUnit> NodeRareData::LastRememberedInlineSize() const {
  return GetOptionalField<LayoutUnit>(FieldId::kLastRememberedInlineSize);
}

gfx::Rect NodeRareData::LastSentUnboundedBounds() const {
  if (auto* value =
          GetWrappedField<gfx::Rect>(FieldId::kLastSentUnboundedBounds)) {
    return *value;
  }
  return gfx::Rect();
}
NodeRareData* NodeRareData::SetLastSentUnboundedBounds(
    const gfx::Rect& bounds) {
  return SetWrappedField<gfx::Rect>(FieldId::kLastSentUnboundedBounds, bounds);
}

UnboundedEventData* NodeRareData::GetUnboundedEventData() const {
  return static_cast<UnboundedEventData*>(
      GetField(FieldId::kUnboundedEventTask));
}
RareDataUpdate<UnboundedEventData> NodeRareData::EnsureUnboundedEventData() {
  return EnsureField<UnboundedEventData>(FieldId::kUnboundedEventTask);
}

PopoverData* NodeRareData::GetPopoverData() const {
  return static_cast<PopoverData*>(GetField(FieldId::kPopoverData));
}
RareDataUpdate<PopoverData> NodeRareData::EnsurePopoverData() {
  return EnsureField<PopoverData>(FieldId::kPopoverData);
}
void NodeRareData::RemovePopoverData() {
  SetFieldToNullIfExists(FieldId::kPopoverData);
}

InvokerData* NodeRareData::GetInvokerData() const {
  return static_cast<InvokerData*>(GetField(FieldId::kInvokerData));
}
RareDataUpdate<InvokerData> NodeRareData::EnsureInvokerData() {
  return EnsureField<InvokerData>(FieldId::kInvokerData);
}
InterestInvokerTargetData* NodeRareData::GetInterestInvokerTargetData() const {
  return static_cast<InterestInvokerTargetData*>(
      GetField(FieldId::kInterestInvokerTargetData));
}
RareDataUpdate<InterestInvokerTargetData>
NodeRareData::EnsureInterestInvokerTargetData() {
  return EnsureField<InterestInvokerTargetData>(
      FieldId::kInterestInvokerTargetData);
}
void NodeRareData::RemoveInterestInvokerTargetData() {
  SetFieldToNullIfExists(FieldId::kInterestInvokerTargetData);
}

ScrollMarkerGroupData* NodeRareData::GetScrollMarkerGroupData() const {
  return static_cast<ScrollMarkerGroupData*>(
      GetField(FieldId::kScrollMarkerGroupData));
}
void NodeRareData::RemoveScrollMarkerGroupData() {
  SetFieldToNullIfExists(FieldId::kScrollMarkerGroupData);
}
RareDataUpdate<ScrollMarkerGroupData> NodeRareData::EnsureScrollMarkerGroupData(
    Element* element) {
  return EnsureField<ScrollMarkerGroupData>(FieldId::kScrollMarkerGroupData,
                                            element->GetDocument().GetFrame());
}

NodeRareData* NodeRareData::SetScrollMarkerGroupContainerData(
    ScrollMarkerGroupData* data) {
  return SetField(FieldId::kScrollMarkerGroupContainerData, data);
}
ScrollMarkerGroupData* NodeRareData::GetScrollMarkerGroupContainerData() const {
  return static_cast<ScrollMarkerGroupData*>(
      GetField(FieldId::kScrollMarkerGroupContainerData));
}

NodeRareData* NodeRareData::CacheCSSPseudoElement(
    PseudoId pseudo_id,
    const AtomicString& pseudo_argument,
    CSSPseudoElement& pseudo_element) {
  auto update =
      EnsureField<CSSPseudoElementsCacheData>(FieldId::kCSSPseudoElementData);
  update.field_->CacheCSSPseudoElement(pseudo_id, pseudo_argument,
                                       pseudo_element);
  return update.rare_data_;
}

CSSPseudoElement* NodeRareData::GetCSSPseudoElement(
    PseudoId pseudo_id,
    const AtomicString& pseudo_argument) const {
  auto* data = static_cast<CSSPseudoElementsCacheData*>(
      GetField(FieldId::kCSSPseudoElementData));
  if (!data) {
    return {};
  }
  return data->GetCSSPseudoElement(pseudo_id, pseudo_argument);
}

AnchorPositionScrollData* NodeRareData::GetAnchorPositionScrollData() const {
  return static_cast<AnchorPositionScrollData*>(
      GetField(FieldId::kAnchorPositionScrollData));
}

void NodeRareData::RemoveAnchorPositionScrollData() {
  if (auto* scroll_data = GetAnchorPositionScrollData()) {
    if (auto* observer = scroll_data->GetAnchorPositionVisibilityObserver()) {
      observer->MonitorAnchor(nullptr);
    }
  }
  SetFieldToNullIfExists(FieldId::kAnchorPositionScrollData);
}

RareDataUpdate<AnchorPositionScrollData>
NodeRareData::EnsureAnchorPositionScrollData(Element* anchored_element) {
  DCHECK(!GetAnchorPositionScrollData() ||
         GetAnchorPositionScrollData()->AnchoredElement() == anchored_element);
  return EnsureField<AnchorPositionScrollData>(
      FieldId::kAnchorPositionScrollData, anchored_element);
}

ExplicitlySetAttrElementsMap* NodeRareData::GetExplicitlySetElementsForAttr()
    const {
  return static_cast<ExplicitlySetAttrElementsMap*>(
      GetField(FieldId::kExplicitlySetElementsForAttr));
}

RareDataUpdate<ExplicitlySetAttrElementsMap>
NodeRareData::EnsureExplicitlySetElementsForAttr() {
  return EnsureField<ExplicitlySetAttrElementsMap>(
      FieldId::kExplicitlySetElementsForAttr);
}

bool NodeRareData::HasCustomElementRegistrySet() const {
  return flags_.has_custom_element_registry_;
}

CustomElementRegistry* NodeRareData::GetCustomElementRegistry() const {
  DCHECK(HasCustomElementRegistrySet());
  return static_cast<CustomElementRegistry*>(
      GetField(FieldId::kCustomElementRegistry));
}

NodeRareData* NodeRareData::SetCustomElementRegistry(
    CustomElementRegistry* registry) {
  // An element's custom element registry should only be set once unless the
  // registry is a global registry and can be reset during cross document node
  // adoption.
  DCHECK(!GetField(FieldId::kCustomElementRegistry) ||
         static_cast<CustomElementRegistry*>(
             GetField(FieldId::kCustomElementRegistry))
             ->IsGlobalRegistry());
  // We intentionally don't rely solely on NodeRareData::SetField's presence
  // in the bitfield, because SetField won't allocate a slot if we set a
  // non-existent field to null. However, when we want an element to have a
  // null registry explicitly, we need to track that it was set. Thus, we use
  // the `has_custom_element_registry_` flag.
  flags_.has_custom_element_registry_ = true;
  return SetField(FieldId::kCustomElementRegistry, registry);
}

void NodeRareData::ClearCustomElementRegistry() {
  flags_.has_custom_element_registry_ = false;
  SetFieldToNullIfExists(FieldId::kCustomElementRegistry);
}

ElementAnimationTriggerData* NodeRareData::AnimationTriggerData() {
  return static_cast<ElementAnimationTriggerData*>(
      GetField(FieldId::kAnimationTriggerData));
}

RareDataUpdate<ElementAnimationTriggerData>
NodeRareData::EnsureAnimationTriggerData() {
  return EnsureField<ElementAnimationTriggerData>(
      FieldId::kAnimationTriggerData);
}

DisplayAdElementMonitor* NodeRareData::GetDisplayAdElementMonitor() const {
  return static_cast<DisplayAdElementMonitor*>(
      GetField(FieldId::kDisplayAdElementMonitor));
}

RareDataUpdate<DisplayAdElementMonitor>
NodeRareData::EnsureDisplayAdElementMonitor(Element* element,
                                            AdProvenance ad_provenance) {
  return EnsureField<DisplayAdElementMonitor>(
      FieldId::kDisplayAdElementMonitor, element, std::move(ad_provenance));
}

FocusgroupData NodeRareData::GetFocusgroupData() const {
  if (auto* value = GetWrappedField<FocusgroupData>(FieldId::kFocusgroupData)) {
    return *value;
  }
  return FocusgroupData();
}

NodeRareData* NodeRareData::SetFocusgroupData(FocusgroupData data) {
  return SetWrappedField<FocusgroupData>(FieldId::kFocusgroupData, data);
}

void NodeRareData::ClearFocusgroupData() {
  SetFieldToNullIfExists(FieldId::kFocusgroupData);
  SetFieldToNullIfExists(FieldId::kFocusgroupLastFocused);
}

NodeRareData* NodeRareData::SetFocusgroupLastFocused(Element* element) {
  // Store weak reference, this should not keep the element alive.
  return SetWrappedField<WeakMember<Element>>(FieldId::kFocusgroupLastFocused,
                                              element);
}

Element* NodeRareData::GetFocusgroupLastFocused() const {
  if (auto* value = GetWrappedField<WeakMember<Element>>(
          FieldId::kFocusgroupLastFocused)) {
    return value->Get();
  }
  return nullptr;
}

ContentData* NodeRareData::GetAltContentData() const {
  if (auto* value =
          GetWrappedField<Member<ContentData>>(FieldId::kAltContentData)) {
    return value->Get();
  }
  return nullptr;
}

NodeRareData* NodeRareData::SetAltContentData(ContentData* content_data) {
  if (content_data) {
    return SetWrappedField<Member<ContentData>>(FieldId::kAltContentData,
                                                content_data);
  } else {
    SetFieldToNullIfExists(FieldId::kAltContentData);
    return this;
  }
}

NodeRareData* NodeRareData::SetOverscrollContainer(Element* element) {
  return SetWrappedField<WeakMember<Element>>(FieldId::kOverscrollContainer,
                                              element);
}

Element* NodeRareData::GetOverscrollContainer() const {
  if (auto* value =
          GetWrappedField<WeakMember<Element>>(FieldId::kOverscrollContainer)) {
    return value->Get();
  }
  return nullptr;
}

RareDataUpdate<OverscrollAreaTracker> NodeRareData::EnsureOverscrollAreaTracker(
    Element* element) {
  return EnsureField<class OverscrollAreaTracker>(
      FieldId::kOverscrollAreaTracker, element);
}
OverscrollAreaTracker* NodeRareData::OverscrollAreaTracker() const {
  return static_cast<class OverscrollAreaTracker*>(
      GetField(FieldId::kOverscrollAreaTracker));
}

void NodeRareData::Trace(blink::Visitor* visitor) const {
  visitor->TraceMultiple(ArrayBase(), size());
}

void NodeMutationObserverData::Trace(Visitor* visitor) const {
  NodeRareDataField::Trace(visitor);
  visitor->Trace(registry_);
  visitor->Trace(transient_registry_);
}

void ScrollTimelineHashSet::Trace(Visitor* visitor) const {
  NodeRareDataField::Trace(visitor);
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

NodeRareData* NodeRareData::RegisterScrollTimeline(ScrollTimeline* timeline) {
  auto update = EnsureField<ScrollTimelineHashSet>(FieldId::kScrollTimelines);
  update.field_->set_.insert(timeline);
  return update.rare_data_;
}
NodeRareData* NodeRareData::UnregisterScrollTimeline(ScrollTimeline* timeline) {
  auto update = EnsureField<ScrollTimelineHashSet>(FieldId::kScrollTimelines);
  update.field_->set_.erase(timeline);
  return update.rare_data_;
}

void NodeRareData::IncrementConnectedSubframeCount() {
  SECURITY_CHECK((flags_.connected_frame_count_ + 1) <=
                 Page::MaxNumberOfFrames());
  ++flags_.connected_frame_count_;
}

RareDataUpdate<FlatTreeNodeData> NodeRareData::EnsureFlatTreeNodeData() {
  return EnsureField<FlatTreeNodeData>(FieldId::kFlatTreeNodeData);
}

static_assert(static_cast<int>(NodeRareData::kNumberOfElementFlags) ==
                  static_cast<int>(ElementFlags::kNumberOfElementFlags),
              "kNumberOfElementFlags must match.");
static_assert(
    static_cast<int>(NodeRareData::kNumberOfDynamicRestyleFlags) ==
        static_cast<int>(DynamicRestyleFlags::kNumberOfDynamicRestyleFlags),
    "kNumberOfDynamicRestyleFlags must match.");

}  // namespace blink
