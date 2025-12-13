// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_MAP_H_

#include <variant>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/anchor_scope.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/platform/geometry/physical_offset.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"

namespace blink {

class LayoutBox;
class LayoutObject;
class PhysicalFragment;

// Reference to an anchor at a specific container. The geometry getters here
// will return values relatively to this container.
//
// During layout, PhysicalAnchorReference objects will bubble up the containing
// block chain, to help resolve anchor queries (anchor() and anchor-size()
// functions) for elements that are to be anchored to a specific anchor.
class CORE_EXPORT PhysicalAnchorReference
    : public GarbageCollected<PhysicalAnchorReference> {
 public:
  PhysicalAnchorReference(const Element& element,
                          const TransformState& transform_state,
                          const PhysicalRect& rect_without_transforms,
                          bool is_out_of_flow,
                          bool has_running_transform_animation,
                          GCedHeapHashSet<Member<Element>>* display_locks)
      : transform_state_(transform_state),
        rect_without_transforms_(rect_without_transforms),
        element_(&element),
        display_locks_(display_locks),
        is_out_of_flow_(is_out_of_flow),
        has_running_transform_animation_(has_running_transform_animation) {}

  void Trace(Visitor* visitor) const;

  // Insert |this| into the given singly linked list in the reverse tree order.
  void InsertInReverseTreeOrderInto(Member<PhysicalAnchorReference>* head_ptr);

  LayoutObject* GetLayoutObject() const { return element_->GetLayoutObject(); }

  PhysicalRect TransformedBoundingRect() const {
    gfx::RectF rect_f = transform_state_.MappedQuad().BoundingBox();
    return PhysicalRect::EnclosingRect(rect_f);
  }

  PhysicalRect RectWithoutTransforms() const {
    return rect_without_transforms_;
  }
  void UniteRectWithoutTransforms(const PhysicalRect& rect) {
    rect_without_transforms_.Unite(rect);
  }

  const TransformState& GetTransformState() const { return transform_state_; }
  void SetTransformState(const TransformState& state) {
    transform_state_ = state;
  }

  const Element& GetElement() const { return *element_; }
  PhysicalAnchorReference* Next() const { return next_; }
  const Member<GCedHeapHashSet<Member<Element>>>& GetDisplayLocks() const {
    return display_locks_;
  }
  bool IsOutOfFlow() const { return is_out_of_flow_; }

  // True if the anchor itself has a transform, or if any of its containing
  // blocks thus far (see class documentation) has a transform.
  bool HasRunningTransformAnimation() const {
    return has_running_transform_animation_;
  }

 private:
  // For now, store both the transform state (to provide the bounding box after
  // applying transforms), and also the raw border box rectangle of the anchor
  // (without transforms). It may be possible that we can drop the latter, once
  // the CSSAnchorWithTransforms runtime feature sticks, but there are spec
  // discussions to be had first, if nothing else.
  TransformState transform_state_;
  PhysicalRect rect_without_transforms_;

  Member<const Element> element_;
  // A singly linked list in the reverse tree order. There can be at most one
  // in-flow reference, which if exists must be at the end of the list.
  Member<PhysicalAnchorReference> next_;
  Member<GCedHeapHashSet<Member<Element>>> display_locks_;
  bool is_out_of_flow_ = false;
  bool has_running_transform_animation_ = false;
};

using AnchorKey = std::variant<const AnchorScopedName*, const Element*>;

// Map of descendant anchor references at a specific container.
//
// This class is conceptually a concatenation of two hash maps with different
// key types but the same value type. To save memory, we don't implement it as
// one hash map with a unified key type; Otherwise, the size of each key will be
// increased by at least one pointer, which is undesired.
class CORE_EXPORT AnchorMap : public GarbageCollected<AnchorMap> {
  using NamedAnchorMap = HeapHashMap<Member<const AnchorScopedName>,
                                     Member<PhysicalAnchorReference>>;
  using ImplicitAnchorMap =
      HeapHashMap<Member<const Element>, Member<PhysicalAnchorReference>>;

 public:
  bool IsEmpty() const {
    return named_anchors_.empty() && implicit_anchors_.empty();
  }

  const PhysicalAnchorReference* GetAnchorReference(
      const AnchorKey& key) const {
    if (const AnchorScopedName* const* name =
            std::get_if<const AnchorScopedName*>(&key)) {
      return GetAnchorReference(named_anchors_, *name);
    }
    return GetAnchorReference(implicit_anchors_, std::get<const Element*>(key));
  }

  struct AddResult {
    Member<PhysicalAnchorReference>* stored_value;
    bool is_new_entry;
    STACK_ALLOCATED();
  };
  AddResult insert(const AnchorKey& key, PhysicalAnchorReference* reference) {
    if (const AnchorScopedName* const* name =
            std::get_if<const AnchorScopedName*>(&key)) {
      return insert(named_anchors_, *name, reference);
    }
    return insert(implicit_anchors_, std::get<const Element*>(key), reference);
  }

  class Iterator {
    STACK_ALLOCATED();

   public:
    Iterator(const AnchorMap* anchor_map,
             typename NamedAnchorMap::const_iterator named_map_iterator,
             typename ImplicitAnchorMap::const_iterator implicit_map_iterator)
        : anchor_map_(anchor_map),
          named_map_iterator_(named_map_iterator),
          implicit_map_iterator_(implicit_map_iterator) {}

    struct Entry {
      AnchorKey key;
      PhysicalAnchorReference* value;
      STACK_ALLOCATED();
    };
    Entry operator*() const {
      if (named_map_iterator_ != anchor_map_->named_anchors_.end()) {
        return Entry{named_map_iterator_->key, named_map_iterator_->value};
      }
      return Entry{implicit_map_iterator_->key, implicit_map_iterator_->value};
    }

    bool operator==(const Iterator& other) const {
      return named_map_iterator_ == other.named_map_iterator_ &&
             implicit_map_iterator_ == other.implicit_map_iterator_;
    }

    Iterator& operator++() {
      if (named_map_iterator_ != anchor_map_->named_anchors_.end()) {
        ++named_map_iterator_;
      } else {
        ++implicit_map_iterator_;
      }
      return *this;
    }

   private:
    const AnchorMap* anchor_map_;
    typename NamedAnchorMap::const_iterator named_map_iterator_;
    typename ImplicitAnchorMap::const_iterator implicit_map_iterator_;
  };
  Iterator begin() const {
    return Iterator{this, named_anchors_.begin(), implicit_anchors_.begin()};
  }
  Iterator end() const {
    return Iterator{this, named_anchors_.end(), implicit_anchors_.end()};
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(named_anchors_);
    visitor->Trace(implicit_anchors_);
  }

  enum class SetOptions {
    // An in-flow entry.
    kInFlow,
    // An out-of-flow entry.
    kOutOfFlow,
  };

  // Find and return a valid anchor reference for the specified anchor key.
  // Unless nullptr is returned, the returned anchor reference is guaranteed to
  // have a valid LayoutObject.
  const PhysicalAnchorReference* AnchorReference(
      const LayoutBox& query_box,
      const LayoutObject* query_box_actual_containing_block,
      const AnchorKey&) const;
  const LayoutObject* AnchorLayoutObject(const LayoutBox& query_box,
                                         const AnchorKey&) const;

  // If the element owning this object has a display lock, the element should be
  // passed as |element_for_display_lock|.
  void Set(const AnchorKey&,
           const LayoutObject& layout_object,
           const TransformState& transform_state,
           const PhysicalRect& rect_without_transforms,
           SetOptions,
           Element* element_for_display_lock);
  void Set(const AnchorKey&, PhysicalAnchorReference* reference);
  // If the element owning this object has a display lock, the element should be
  // passed as |element_for_display_lock|.
  void SetFromChild(const PhysicalFragment& child_fragment,
                    PhysicalOffset additional_offset,
                    const LayoutObject& container_object,
                    PhysicalSize container_size,
                    SetOptions,
                    Element* element_for_display_lock);

 private:
  template <typename AnchorMapType, typename KeyType>
  static const PhysicalAnchorReference* GetAnchorReference(
      const AnchorMapType& anchors,
      const KeyType& key) {
    auto it = anchors.find(key);
    return it != anchors.end() ? it->value : nullptr;
  }

  template <typename AnchorMapType, typename KeyType>
  static AddResult insert(AnchorMapType& anchors,
                          const KeyType& key,
                          PhysicalAnchorReference* reference) {
    auto result = anchors.insert(key, reference);
    return AddResult{&result.stored_value->value, result.is_new_entry};
  }

  NamedAnchorMap named_anchors_;
  ImplicitAnchorMap implicit_anchors_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_MAP_H_
