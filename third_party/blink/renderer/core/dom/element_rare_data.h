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
#include "third_party/blink/renderer/core/dom/element_rare_data_base.h"
#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"
#include "third_party/blink/renderer/core/dom/has_invalidation_flags.h"
#include "third_party/blink/renderer/core/dom/named_node_map.h"
#include "third_party/blink/renderer/core/dom/names_map.h"
#include "third_party/blink/renderer/core/dom/node_rare_data.h"
#include "third_party/blink/renderer/core/dom/popover_data.h"
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

class AnchorScrollData;
class ContainerQueryData;
class Element;
class HTMLElement;
class ResizeObservation;
class ResizeObserver;

class ElementRareData final : public ElementRareDataBase {
 public:
  explicit ElementRareData(NodeData*);
  ~ElementRareData() override;

  void SetPseudoElement(
      PseudoId,
      PseudoElement*,
      const AtomicString& view_transition_name = g_null_atom) override;
  PseudoElement* GetPseudoElement(
      PseudoId,
      const AtomicString& view_transition_name = g_null_atom) const override;
  PseudoElementData::PseudoElementVector GetPseudoElements() const override;

  CSSStyleDeclaration& EnsureInlineCSSStyleDeclaration(
      Element* owner_element) override;

  ShadowRoot* GetShadowRoot() const override { return shadow_root_.Get(); }
  void SetShadowRoot(ShadowRoot& shadow_root) override {
    DCHECK(!shadow_root_);
    shadow_root_ = &shadow_root;
  }

  NamedNodeMap* AttributeMap() const override { return attribute_map_.Get(); }
  void SetAttributeMap(NamedNodeMap* attribute_map) override {
    attribute_map_ = attribute_map;
  }

  DOMTokenList* GetClassList() const override { return class_list_.Get(); }
  void SetClassList(DOMTokenList* class_list) override {
    class_list_ = class_list;
  }

  DatasetDOMStringMap* Dataset() const override { return dataset_.Get(); }
  void SetDataset(DatasetDOMStringMap* dataset) override { dataset_ = dataset; }

  ScrollOffset SavedLayerScrollOffset() const override {
    return saved_layer_scroll_offset_;
  }
  void SetSavedLayerScrollOffset(ScrollOffset offset) override {
    saved_layer_scroll_offset_ = offset;
  }

  ElementAnimations* GetElementAnimations() override {
    return element_animations_.Get();
  }
  void SetElementAnimations(ElementAnimations* element_animations) override {
    element_animations_ = element_animations;
  }

  bool HasPseudoElements() const override;
  void ClearPseudoElements() override;

  AttrNodeList& EnsureAttrNodeList() override;
  AttrNodeList* GetAttrNodeList() override { return attr_node_list_.Get(); }
  void RemoveAttrNodeList() override { attr_node_list_.Clear(); }
  void AddAttr(Attr* attr) override { EnsureAttrNodeList().push_back(attr); }

  ElementIntersectionObserverData* IntersectionObserverData() const override {
    return intersection_observer_data_.Get();
  }
  ElementIntersectionObserverData& EnsureIntersectionObserverData() override {
    if (!intersection_observer_data_) {
      intersection_observer_data_ =
          MakeGarbageCollected<ElementIntersectionObserverData>();
    }
    return *intersection_observer_data_;
  }

  ContainerQueryEvaluator* GetContainerQueryEvaluator() const override {
    ContainerQueryData* container_query_data = GetContainerQueryData();
    if (!container_query_data)
      return nullptr;
    return container_query_data->GetContainerQueryEvaluator();
  }
  void SetContainerQueryEvaluator(ContainerQueryEvaluator* evaluator) override {
    ContainerQueryData* container_query_data = GetContainerQueryData();
    if (container_query_data)
      container_query_data->SetContainerQueryEvaluator(evaluator);
    else if (evaluator)
      EnsureContainerQueryData().SetContainerQueryEvaluator(evaluator);
  }

  const AtomicString& GetNonce() const override { return nonce_; }
  void SetNonce(const AtomicString& nonce) override { nonce_ = nonce; }

  EditContext* GetEditContext() const override { return edit_context_.Get(); }
  void SetEditContext(EditContext* edit_context) override {
    edit_context_ = edit_context;
  }

  void SetPart(DOMTokenList* part) override { part_ = part; }
  DOMTokenList* GetPart() const override { return part_.Get(); }

  void SetPartNamesMap(const AtomicString part_names) override {
    if (!part_names_map_) {
      part_names_map_ = std::make_unique<NamesMap>();
    }
    part_names_map_->Set(part_names);
  }
  const NamesMap* PartNamesMap() const override {
    return part_names_map_.get();
  }

  InlineStylePropertyMap& EnsureInlineStylePropertyMap(
      Element* owner_element) override;
  InlineStylePropertyMap* GetInlineStylePropertyMap() override {
    return cssom_map_wrapper_.Get();
  }

  ElementInternals& EnsureElementInternals(HTMLElement& target) override;
  const ElementInternals* GetElementInternals() const override {
    return element_internals_;
  }

  AccessibleNode* GetAccessibleNode() const override {
    return accessible_node_.Get();
  }
  AccessibleNode* EnsureAccessibleNode(Element* owner_element) override {
    if (!accessible_node_) {
      accessible_node_ = MakeGarbageCollected<AccessibleNode>(owner_element);
    }
    return accessible_node_;
  }
  void ClearAccessibleNode() override { accessible_node_.Clear(); }

  DisplayLockContext* EnsureDisplayLockContext(Element* element) override {
    if (!display_lock_context_) {
      display_lock_context_ = MakeGarbageCollected<DisplayLockContext>(element);
    }
    return display_lock_context_.Get();
  }
  DisplayLockContext* GetDisplayLockContext() const override {
    return display_lock_context_;
  }

  ContainerQueryData& EnsureContainerQueryData() override {
    if (!container_query_data_)
      container_query_data_ = MakeGarbageCollected<ContainerQueryData>();
    return *container_query_data_;
  }
  ContainerQueryData* GetContainerQueryData() const override {
    return container_query_data_;
  }
  void ClearContainerQueryData() override { container_query_data_ = nullptr; }

  // Returns the crop-ID if one was set, or nullptr otherwise.
  const RegionCaptureCropId* GetRegionCaptureCropId() const override {
    return region_capture_crop_id_.get();
  }

  // Sets a crop-ID on the item. Must be called at most once. Cannot be used
  // to unset a previously set crop-ID.
  void SetRegionCaptureCropId(
      std::unique_ptr<RegionCaptureCropId> crop_id) override {
    DCHECK(!GetRegionCaptureCropId());
    DCHECK(crop_id);
    DCHECK(!crop_id->value().is_zero());
    region_capture_crop_id_ = std::move(crop_id);
  }

  using ResizeObserverDataMap =
      HeapHashMap<Member<ResizeObserver>, Member<ResizeObservation>>;

  ResizeObserverDataMap* ResizeObserverData() const override {
    return resize_observer_data_;
  }
  ResizeObserverDataMap& EnsureResizeObserverData() override;

  void SetCustomElementDefinition(
      CustomElementDefinition* definition) override {
    custom_element_definition_ = definition;
  }
  CustomElementDefinition* GetCustomElementDefinition() const override {
    return custom_element_definition_.Get();
  }

  void SetIsValue(const AtomicString& is_value) override {
    is_value_ = is_value;
  }
  const AtomicString& IsValue() const override { return is_value_; }

  void SaveLastIntrinsicSize(ResizeObserverSize* size) override {
    last_intrinsic_size_ = size;
  }
  const ResizeObserverSize* LastIntrinsicSize() const override {
    return last_intrinsic_size_;
  }

  PopoverData* GetPopoverData() const override { return popover_data_; }
  PopoverData& EnsurePopoverData() override;
  void RemovePopoverData() override;

  CSSToggleMap* GetToggleMap() const override { return toggle_map_.Get(); }
  CSSToggleMap& EnsureToggleMap(Element* owner_element) override;

  AnchorScrollData* GetAnchorScrollData() const override {
    return anchor_scroll_data_;
  }
  void RemoveAnchorScrollData() override { anchor_scroll_data_ = nullptr; }
  AnchorScrollData& EnsureAnchorScrollData(Element*) override;

  bool HasElementFlag(ElementFlags mask) const override {
    return element_flags_ & static_cast<uint16_t>(mask);
  }
  void SetElementFlag(ElementFlags mask, bool value) override {
    element_flags_ =
        (element_flags_ & ~static_cast<uint16_t>(mask)) |
        (-static_cast<uint16_t>(value) & static_cast<uint16_t>(mask));
  }
  void ClearElementFlag(ElementFlags mask) override {
    element_flags_ &= ~static_cast<uint16_t>(mask);
  }

  bool HasRestyleFlags() const override {
    return bit_field_.get<RestyleFlags>();
  }
  void ClearRestyleFlags() override { bit_field_.set<RestyleFlags>(0); }

  void SetTabIndexExplicitly() override {
    SetElementFlag(ElementFlags::kTabIndexWasSetExplicitly, true);
  }
  void ClearTabIndexExplicitly() override {
    ClearElementFlag(ElementFlags::kTabIndexWasSetExplicitly);
  }

  void IncrementAnchoredPopoverCount() override { ++anchored_popover_count_; }
  void DecrementAnchoredPopoverCount() override {
    DCHECK(anchored_popover_count_);
    --anchored_popover_count_;
  }
  bool HasAnchoredPopover() const override { return anchored_popover_count_; }

  void Trace(blink::Visitor*) const override;

 private:
  AtomicString nonce_;
  AtomicString is_value_;

  std::unique_ptr<NamesMap> part_names_map_;
  std::unique_ptr<RegionCaptureCropId> region_capture_crop_id_;

  Member<DatasetDOMStringMap> dataset_;
  Member<ShadowRoot> shadow_root_;
  Member<DOMTokenList> class_list_;
  Member<NamedNodeMap> attribute_map_;
  Member<AttrNodeList> attr_node_list_;
  Member<InlineCSSStyleDeclaration> cssom_wrapper_;
  Member<ElementAnimations> element_animations_;
  Member<ElementIntersectionObserverData> intersection_observer_data_;
  Member<PseudoElementData> pseudo_element_data_;
  Member<EditContext> edit_context_;
  Member<DOMTokenList> part_;
  Member<InlineStylePropertyMap> cssom_map_wrapper_;
  Member<ElementInternals> element_internals_;
  Member<AccessibleNode> accessible_node_;
  Member<DisplayLockContext> display_lock_context_;
  Member<ContainerQueryData> container_query_data_;
  Member<ResizeObserverDataMap> resize_observer_data_;
  Member<CustomElementDefinition> custom_element_definition_;
  Member<ResizeObserverSize> last_intrinsic_size_;
  Member<PopoverData> popover_data_;
  Member<CSSToggleMap> toggle_map_;
  Member<AnchorScrollData> anchor_scroll_data_;

  ScrollOffset saved_layer_scroll_offset_;
  wtf_size_t anchored_popover_count_ = 0;
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
    const AtomicString& view_transition_name) {
  if (!pseudo_element_data_) {
    if (!element)
      return;
    pseudo_element_data_ = MakeGarbageCollected<PseudoElementData>();
  }
  pseudo_element_data_->SetPseudoElement(pseudo_id, element,
                                         view_transition_name);
}

inline PseudoElement* ElementRareData::GetPseudoElement(
    PseudoId pseudo_id,
    const AtomicString& view_transition_name) const {
  if (!pseudo_element_data_)
    return nullptr;
  return pseudo_element_data_->GetPseudoElement(pseudo_id,
                                                view_transition_name);
}

inline PseudoElementData::PseudoElementVector
ElementRareData::GetPseudoElements() const {
  if (!pseudo_element_data_)
    return {};
  return pseudo_element_data_->GetPseudoElements();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_H_
