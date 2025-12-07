// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/anchor_map.h"

#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/physical_fragment.h"
#include "third_party/blink/renderer/core/layout/transform_utils.h"

namespace blink {

namespace {

bool HasRunningTransformAnimation(const LayoutObject& layout_object) {
  const auto* element = To<Element>(layout_object.GetNode());
  if (!element) {
    return false;
  }
  const ElementAnimations* animations = element->GetElementAnimations();
  if (!animations) {
    return false;
  }
  const EffectStack& stack = animations->GetEffectStack();
  PropertyHandle transform(CSSProperty::Get(CSSPropertyID::kTransform));
  PropertyHandle translate(CSSProperty::Get(CSSPropertyID::kTranslate));
  PropertyHandle rotate(CSSProperty::Get(CSSPropertyID::kRotate));
  PropertyHandle scale(CSSProperty::Get(CSSPropertyID::kScale));
  return stack.HasActiveAnimationsOnCompositor(transform) ||
         stack.HasActiveAnimationsOnCompositor(translate) ||
         stack.HasActiveAnimationsOnCompositor(rotate) ||
         stack.HasActiveAnimationsOnCompositor(scale);
}

}  // anonymous namespace

void PhysicalAnchorReference::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(next_);
  visitor->Trace(display_locks_);
}

void PhysicalAnchorReference::InsertInReverseTreeOrderInto(
    Member<PhysicalAnchorReference>* head_ptr) {
  for (;;) {
    PhysicalAnchorReference* const head = *head_ptr;
    DCHECK(!head || head->GetLayoutObject());
    DCHECK(GetLayoutObject());
    if (!head ||
        head->GetLayoutObject()->IsBeforeInPreOrder(*GetLayoutObject())) {
      next_ = head;
      *head_ptr = this;
      break;
    }

    head_ptr = &head->next_;
  }
}

const PhysicalAnchorReference* AnchorMap::AnchorReference(
    const LayoutBox& query_box,
    const LayoutObject* query_box_actual_containing_block,
    const AnchorKey& key) const {
  const PhysicalAnchorReference* reference = GetAnchorReference(key);
  if (!reference) {
    return nullptr;
  }
  for (const PhysicalAnchorReference* result = reference; result;
       result = result->Next()) {
    const LayoutObject* layout_object = result->GetLayoutObject();
    // TODO(crbug.com/384523570): If the layout object has been detached, we
    // really shouldn't be here.
    if (!layout_object || layout_object == &query_box ||
        (result->IsOutOfFlow() &&
         !layout_object->IsBeforeInPreOrder(query_box))) {
      continue;
    }

    // If an actual containing block has been specified, it means that we may
    // have found an anchor that isn't acceptable, due to an inconsistency
    // between the actual (CSS) containing block chain, and the physical
    // fragment tree structure. This happens for OOFs in block fragmentation.
    if (query_box_actual_containing_block &&
        !layout_object->Container()->IsContainedBy(
            query_box_actual_containing_block)) {
      continue;
    }

    return result;
  }
  return nullptr;
}

const LayoutObject* AnchorMap::AnchorLayoutObject(const LayoutBox& query_box,
                                                  const AnchorKey& key) const {
  if (const PhysicalAnchorReference* reference = AnchorReference(
          query_box, /*query_box_actual_containing_block=*/nullptr, key)) {
    return reference->GetLayoutObject();
  }
  return nullptr;
}

void AnchorMap::Set(const AnchorKey& key,
                    const LayoutObject& layout_object,
                    const TransformState& transform_state,
                    const PhysicalRect& rect_without_transforms,
                    SetOptions options,
                    Element* element_for_display_lock) {
  GCedHeapHashSet<Member<Element>>* display_locks = nullptr;
  if (element_for_display_lock) {
    display_locks = MakeGarbageCollected<GCedHeapHashSet<Member<Element>>>();
    display_locks->insert(element_for_display_lock);
  }

  auto* reference = MakeGarbageCollected<PhysicalAnchorReference>(
      *To<Element>(layout_object.GetNode()), transform_state,
      rect_without_transforms, options == SetOptions::kOutOfFlow,
      HasRunningTransformAnimation(layout_object), display_locks);
  Set(key, reference);
}

void AnchorMap::Set(const AnchorKey& key, PhysicalAnchorReference* reference) {
  DCHECK(reference);
  DCHECK(!reference->Next());
  const auto result = insert(key, reference);
  if (result.is_new_entry) {
    return;
  }

  // If this is a fragment of the existing |LayoutObject|, unite the rect.
  Member<PhysicalAnchorReference>* const existing_head_ptr =
      result.stored_value;
  DCHECK(*existing_head_ptr);
  DCHECK(reference->GetLayoutObject());
  for (PhysicalAnchorReference* existing = *existing_head_ptr; existing;
       existing = existing->Next()) {
    DCHECK(existing->GetLayoutObject());
    if (existing->GetLayoutObject() == reference->GetLayoutObject()) {
      existing->UniteRectWithoutTransforms(reference->RectWithoutTransforms());

      gfx::RectF rect =
          existing->GetTransformState().MappedQuad().BoundingBox();
      rect.Union(reference->GetTransformState().MappedQuad().BoundingBox());
      existing->SetTransformState(
          TransformState(TransformState::kApplyTransformDirection,
                         gfx::QuadF(gfx::RectF(rect))));
      return;
    }
  }

  // When out-of-flow objects are involved, callers can't guarantee the call
  // order. Insert into the list in the reverse tree order.
  reference->InsertInReverseTreeOrderInto(existing_head_ptr);
}

void AnchorMap::SetFromChild(const PhysicalFragment& child_fragment,
                             PhysicalOffset additional_offset,
                             const LayoutObject& container_object,
                             PhysicalSize container_size,
                             SetOptions options,
                             Element* element_for_display_lock) {
  const AnchorMap* child_anchor_map = child_fragment.GetAnchorMap();
  DCHECK(child_anchor_map);

  for (auto entry : *child_anchor_map) {
    // For each key, only the last reference in tree order is reachable
    // under normal circumstances. However, the presence of anchor-scope
    // can make it necessary to skip past any number of references to reach
    // an earlier one. Therefore, all references must be propagated.
    //
    // See also InSameAnchorScope.
    for (PhysicalAnchorReference* reference = entry.value; reference;
         reference = reference->Next()) {
      PhysicalRect rect_without_transforms = reference->RectWithoutTransforms();
      rect_without_transforms.offset += additional_offset;

      TransformState transform_state(reference->GetTransformState());
      UpdateTransformState(child_fragment, additional_offset, container_object,
                           container_size, &transform_state);

      GCedHeapHashSet<Member<Element>>* display_locks = nullptr;
      if (reference->GetDisplayLocks() || element_for_display_lock) {
        display_locks =
            MakeGarbageCollected<GCedHeapHashSet<Member<Element>>>();
      }
      if (reference->GetDisplayLocks()) {
        *display_locks = *reference->GetDisplayLocks();
      }
      if (element_for_display_lock) {
        display_locks->insert(element_for_display_lock);
      }
      DCHECK(reference->GetLayoutObject());
      const LayoutObject* child_object = child_fragment.GetLayoutObject();
      bool has_running_transform_animation =
          reference->HasRunningTransformAnimation() ||
          (child_object && HasRunningTransformAnimation(*child_object));
      auto* parent_reference = MakeGarbageCollected<PhysicalAnchorReference>(
          reference->GetElement(), transform_state, rect_without_transforms,
          options == SetOptions::kOutOfFlow, has_running_transform_animation,
          display_locks);
      Set(entry.key, parent_reference);
    }
  }
}

}  // namespace blink
