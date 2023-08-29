// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/element_rare_data_vector.h"

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
#include "third_party/blink/renderer/core/dom/has_invalidation_flags.h"
#include "third_party/blink/renderer/core/dom/named_node_map.h"
#include "third_party/blink/renderer/core/dom/names_map.h"
#include "third_party/blink/renderer/core/dom/node_rare_data.h"
#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/editing/ime/edit_context.h"
#include "third_party/blink/renderer/core/html/anchor_element_observer.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/intersection_observer/element_intersection_observer_data.h"
#include "third_party/blink/renderer/core/layout/anchor_position_scroll_data.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observation.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

ElementRareDataVector::ElementRareDataVector(NodeData* node_layout_data)
    : NodeRareData(ClassType::kElementRareData, std::move(*node_layout_data)) {}

ElementRareDataVector::~ElementRareDataVector() {
  DCHECK(!GetField(FieldId::kPseudoElementData));
}

unsigned ElementRareDataVector::GetFieldIndex(FieldId field_id) const {
  unsigned field_id_int = static_cast<unsigned>(field_id);
  DCHECK(fields_bitfield_ & (static_cast<BitfieldType>(1) << field_id_int));
  return __builtin_popcount(fields_bitfield_ &
                            ~(~static_cast<BitfieldType>(0) << field_id_int));
}

ElementRareDataField* ElementRareDataVector::GetField(FieldId field_id) const {
  if (fields_bitfield_ &
      (static_cast<BitfieldType>(1) << static_cast<unsigned>(field_id)))
    return fields_[GetFieldIndex(field_id)];
  return nullptr;
}

void ElementRareDataVector::SetField(FieldId field_id,
                                     ElementRareDataField* field) {
  unsigned field_id_int = static_cast<unsigned>(field_id);
  if (fields_bitfield_ & (static_cast<BitfieldType>(1) << field_id_int)) {
    if (field) {
      fields_[GetFieldIndex(field_id)] = field;
    } else {
      fields_.EraseAt(GetFieldIndex(field_id));
      fields_bitfield_ =
          fields_bitfield_ & ~(static_cast<BitfieldType>(1) << field_id_int);
    }
  } else if (field) {
    fields_bitfield_ =
        fields_bitfield_ | (static_cast<BitfieldType>(1) << field_id_int);
    fields_.insert(GetFieldIndex(field_id), field);
  }
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
    SetField(FieldId::kPseudoElementData, nullptr);
  }
}
void ElementRareDataVector::SetPseudoElement(
    PseudoId pseudo_id,
    PseudoElement* element,
    const AtomicString& document_transition_tag) {
  PseudoElementData* data =
      static_cast<PseudoElementData*>(GetField(FieldId::kPseudoElementData));
  if (!data) {
    if (!element)
      return;
    data = MakeGarbageCollected<PseudoElementData>();
    SetField(FieldId::kPseudoElementData, data);
  }
  data->SetPseudoElement(pseudo_id, element, document_transition_tag);
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
PseudoElementData::PseudoElementVector
ElementRareDataVector::GetPseudoElements() const {
  PseudoElementData* data =
      static_cast<PseudoElementData*>(GetField(FieldId::kPseudoElementData));
  if (!data)
    return {};
  return data->GetPseudoElements();
}

CSSStyleDeclaration& ElementRareDataVector::EnsureInlineCSSStyleDeclaration(
    Element* owner_element) {
  return EnsureField<InlineCSSStyleDeclaration>(FieldId::kCssomWrapper,
                                                owner_element);
}

ShadowRoot* ElementRareDataVector::GetShadowRoot() const {
  return static_cast<ShadowRoot*>(GetField(FieldId::kShadowRoot));
}
void ElementRareDataVector::SetShadowRoot(ShadowRoot& shadow_root) {
  DCHECK(!GetField(FieldId::kShadowRoot));
  SetField(FieldId::kShadowRoot, &shadow_root);
}

NamedNodeMap* ElementRareDataVector::AttributeMap() const {
  return static_cast<NamedNodeMap*>(GetField(FieldId::kAttributeMap));
}
void ElementRareDataVector::SetAttributeMap(NamedNodeMap* attribute_map) {
  SetField(FieldId::kAttributeMap, attribute_map);
}

DOMTokenList* ElementRareDataVector::GetClassList() const {
  return static_cast<DOMTokenList*>(GetField(FieldId::kClassList));
}
void ElementRareDataVector::SetClassList(DOMTokenList* class_list) {
  SetField(FieldId::kClassList, class_list);
}

DatasetDOMStringMap* ElementRareDataVector::Dataset() const {
  return static_cast<DatasetDOMStringMap*>(GetField(FieldId::kDataset));
}
void ElementRareDataVector::SetDataset(DatasetDOMStringMap* dataset) {
  SetField(FieldId::kDataset, dataset);
}

ScrollOffset ElementRareDataVector::SavedLayerScrollOffset() const {
  if (auto* value =
          GetWrappedField<ScrollOffset>(FieldId::kSavedLayerScrollOffset)) {
    return *value;
  }
  static ScrollOffset offset;
  return offset;
}
void ElementRareDataVector::SetSavedLayerScrollOffset(ScrollOffset offset) {
  SetWrappedField<ScrollOffset>(FieldId::kSavedLayerScrollOffset, offset);
}

ElementAnimations* ElementRareDataVector::GetElementAnimations() {
  return static_cast<ElementAnimations*>(GetField(FieldId::kElementAnimations));
}
void ElementRareDataVector::SetElementAnimations(
    ElementAnimations* element_animations) {
  SetField(FieldId::kElementAnimations, element_animations);
}

AttrNodeList& ElementRareDataVector::EnsureAttrNodeList() {
  return EnsureWrappedField<AttrNodeList>(FieldId::kAttrNodeList);
}
AttrNodeList* ElementRareDataVector::GetAttrNodeList() {
  return GetWrappedField<AttrNodeList>(FieldId::kAttrNodeList);
}
void ElementRareDataVector::RemoveAttrNodeList() {
  SetField(FieldId::kAttrNodeList, nullptr);
}
void ElementRareDataVector::AddAttr(Attr* attr) {
  EnsureAttrNodeList().push_back(attr);
}

ElementIntersectionObserverData*
ElementRareDataVector::IntersectionObserverData() const {
  return static_cast<ElementIntersectionObserverData*>(
      GetField(FieldId::kIntersectionObserverData));
}
ElementIntersectionObserverData&
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
void ElementRareDataVector::SetContainerQueryEvaluator(
    ContainerQueryEvaluator* evaluator) {
  ContainerQueryData* container_query_data = GetContainerQueryData();
  if (container_query_data)
    container_query_data->SetContainerQueryEvaluator(evaluator);
  else if (evaluator)
    EnsureContainerQueryData().SetContainerQueryEvaluator(evaluator);
}

const AtomicString& ElementRareDataVector::GetNonce() const {
  auto* value = GetWrappedField<AtomicString>(FieldId::kNonce);
  return value ? *value : g_null_atom;
}
void ElementRareDataVector::SetNonce(const AtomicString& nonce) {
  SetWrappedField<AtomicString>(FieldId::kNonce, nonce);
}

const AtomicString& ElementRareDataVector::IsValue() const {
  auto* value = GetWrappedField<AtomicString>(FieldId::kIsValue);
  return value ? *value : g_null_atom;
}
void ElementRareDataVector::SetIsValue(const AtomicString& is_value) {
  SetWrappedField<AtomicString>(FieldId::kIsValue, is_value);
}

EditContext* ElementRareDataVector::GetEditContext() const {
  return static_cast<EditContext*>(GetField(FieldId::kEditContext));
}
void ElementRareDataVector::SetEditContext(EditContext* edit_context) {
  SetField(FieldId::kEditContext, edit_context);
}

void ElementRareDataVector::SetPart(DOMTokenList* part) {
  SetField(FieldId::kPart, part);
}
DOMTokenList* ElementRareDataVector::GetPart() const {
  return static_cast<DOMTokenList*>(GetField(FieldId::kPart));
}

void ElementRareDataVector::SetPartNamesMap(const AtomicString part_names) {
  EnsureWrappedField<NamesMap>(FieldId::kPartNamesMap).Set(part_names);
}
const NamesMap* ElementRareDataVector::PartNamesMap() const {
  return GetWrappedField<NamesMap>(FieldId::kPartNamesMap);
}

InlineStylePropertyMap& ElementRareDataVector::EnsureInlineStylePropertyMap(
    Element* owner_element) {
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
ElementInternals& ElementRareDataVector::EnsureElementInternals(
    HTMLElement& target) {
  return EnsureField<ElementInternals>(FieldId::kElementInternals, target);
}

AccessibleNode* ElementRareDataVector::GetAccessibleNode() const {
  return static_cast<AccessibleNode*>(GetField(FieldId::kAccessibleNode));
}
AccessibleNode* ElementRareDataVector::EnsureAccessibleNode(
    Element* owner_element) {
  return &EnsureField<AccessibleNode>(FieldId::kAccessibleNode, owner_element);
}
void ElementRareDataVector::ClearAccessibleNode() {
  SetField(FieldId::kAccessibleNode, nullptr);
}

DisplayLockContext* ElementRareDataVector::EnsureDisplayLockContext(
    Element* element) {
  return &EnsureField<DisplayLockContext>(FieldId::kDisplayLockContext,
                                          element);
}
DisplayLockContext* ElementRareDataVector::GetDisplayLockContext() const {
  return static_cast<DisplayLockContext*>(
      GetField(FieldId::kDisplayLockContext));
}

ContainerQueryData& ElementRareDataVector::EnsureContainerQueryData() {
  return EnsureField<ContainerQueryData>(FieldId::kContainerQueryData);
}
ContainerQueryData* ElementRareDataVector::GetContainerQueryData() const {
  return static_cast<ContainerQueryData*>(
      GetField(FieldId::kContainerQueryData));
}
void ElementRareDataVector::ClearContainerQueryData() {
  SetField(FieldId::kContainerQueryData, nullptr);
}

const RegionCaptureCropId* ElementRareDataVector::GetRegionCaptureCropId()
    const {
  auto* value = GetWrappedField<std::unique_ptr<RegionCaptureCropId>>(
      FieldId::kRegionCaptureCropId);
  return value ? value->get() : nullptr;
}
void ElementRareDataVector::SetRegionCaptureCropId(
    std::unique_ptr<RegionCaptureCropId> crop_id) {
  CHECK(!GetRegionCaptureCropId());
  CHECK(crop_id);
  CHECK(!crop_id->value().is_zero());
  SetWrappedField<std::unique_ptr<RegionCaptureCropId>>(
      FieldId::kRegionCaptureCropId, std::move(crop_id));
}

const RestrictionTargetId* ElementRareDataVector::GetRestrictionTargetId()
    const {
  auto* value = GetWrappedField<std::unique_ptr<RestrictionTargetId>>(
      FieldId::kRestrictionTargetId);
  return value ? value->get() : nullptr;
}
void ElementRareDataVector::SetRestrictionTargetId(
    std::unique_ptr<RestrictionTargetId> id) {
  CHECK(!GetRestrictionTargetId());
  CHECK(id);
  CHECK(!id->value().is_zero());
  SetWrappedField<std::unique_ptr<RestrictionTargetId>>(
      FieldId::kRestrictionTargetId, std::move(id));
}

ElementRareDataVector::ResizeObserverDataMap*
ElementRareDataVector::ResizeObserverData() const {
  return GetWrappedField<ElementRareDataVector::ResizeObserverDataMap>(
      FieldId::kResizeObserverData);
}
ElementRareDataVector::ResizeObserverDataMap&
ElementRareDataVector::EnsureResizeObserverData() {
  return EnsureWrappedField<ElementRareDataVector::ResizeObserverDataMap>(
      FieldId::kResizeObserverData);
}

void ElementRareDataVector::SetCustomElementDefinition(
    CustomElementDefinition* definition) {
  SetField(FieldId::kCustomElementDefinition, definition);
}
CustomElementDefinition* ElementRareDataVector::GetCustomElementDefinition()
    const {
  return static_cast<CustomElementDefinition*>(
      GetField(FieldId::kCustomElementDefinition));
}

void ElementRareDataVector::SetLastRememberedBlockSize(
    absl::optional<LayoutUnit> size) {
  SetOptionalField(FieldId::kLastRememberedBlockSize, size);
}
void ElementRareDataVector::SetLastRememberedInlineSize(
    absl::optional<LayoutUnit> size) {
  SetOptionalField(FieldId::kLastRememberedInlineSize, size);
}

absl::optional<LayoutUnit> ElementRareDataVector::LastRememberedBlockSize()
    const {
  return GetOptionalField<LayoutUnit>(FieldId::kLastRememberedBlockSize);
}
absl::optional<LayoutUnit> ElementRareDataVector::LastRememberedInlineSize()
    const {
  return GetOptionalField<LayoutUnit>(FieldId::kLastRememberedInlineSize);
}

PopoverData* ElementRareDataVector::GetPopoverData() const {
  return static_cast<PopoverData*>(GetField(FieldId::kPopoverData));
}
PopoverData& ElementRareDataVector::EnsurePopoverData() {
  return EnsureField<PopoverData>(FieldId::kPopoverData);
}
void ElementRareDataVector::RemovePopoverData() {
  SetField(FieldId::kPopoverData, nullptr);
}

CSSToggleMap* ElementRareDataVector::GetToggleMap() const {
  return static_cast<CSSToggleMap*>(GetField(FieldId::kToggleMap));
}
CSSToggleMap& ElementRareDataVector::EnsureToggleMap(Element* owner_element) {
  DCHECK(!GetToggleMap() || GetToggleMap()->OwnerElement() == owner_element);
  return EnsureField<CSSToggleMap>(FieldId::kToggleMap, owner_element);
}

AnchorPositionScrollData* ElementRareDataVector::GetAnchorPositionScrollData()
    const {
  return static_cast<AnchorPositionScrollData*>(
      GetField(FieldId::kAnchorPositionScrollData));
}
void ElementRareDataVector::RemoveAnchorPositionScrollData() {
  SetField(FieldId::kAnchorPositionScrollData, nullptr);
}
AnchorPositionScrollData& ElementRareDataVector::EnsureAnchorPositionScrollData(
    Element* owner_element) {
  DCHECK(!GetAnchorPositionScrollData() ||
         GetAnchorPositionScrollData()->OwnerElement() == owner_element);
  return EnsureField<AnchorPositionScrollData>(
      FieldId::kAnchorPositionScrollData, owner_element);
}

AnchorElementObserver& ElementRareDataVector::EnsureAnchorElementObserver(
    HTMLElement* element) {
  DCHECK(!GetAnchorElementObserver() ||
         GetAnchorElementObserver()->GetElement() == element);
  return EnsureField<AnchorElementObserver>(FieldId::kAnchorElementObserver,
                                            element);
}

AnchorElementObserver* ElementRareDataVector::GetAnchorElementObserver() const {
  return static_cast<AnchorElementObserver*>(
      GetField(FieldId::kAnchorElementObserver));
}

void ElementRareDataVector::IncrementImplicitlyAnchoredElementCount() {
  EnsureWrappedField<wtf_size_t>(FieldId::kImplicitlyAnchoredElementCount)++;
}
void ElementRareDataVector::DecrementImplicitlyAnchoredElementCount() {
  wtf_size_t& anchored_element_count =
      EnsureWrappedField<wtf_size_t>(FieldId::kImplicitlyAnchoredElementCount);
  DCHECK(anchored_element_count);
  anchored_element_count--;
}
bool ElementRareDataVector::HasImplicitlyAnchoredElement() const {
  wtf_size_t* anchored_element_count =
      GetWrappedField<wtf_size_t>(FieldId::kImplicitlyAnchoredElementCount);
  return anchored_element_count ? *anchored_element_count : false;
}

void ElementRareDataVector::Trace(blink::Visitor* visitor) const {
  visitor->Trace(fields_);
  NodeRareData::Trace(visitor);
}

}  // namespace blink
