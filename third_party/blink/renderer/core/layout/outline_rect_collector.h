// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_OUTLINE_RECT_COLLECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_OUTLINE_RECT_COLLECTOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_traits.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
class LayoutObject;
class LayoutBoxModelObject;

class OutlineRectCollector {
 public:
  enum class Type { kUnion, kVector };

  virtual Type GetType() const = 0;
  virtual void AddRect(const PhysicalRect&) = 0;
  virtual OutlineRectCollector* ForDescendantCollector() const = 0;
  virtual void Combine(OutlineRectCollector*,
                       const LayoutObject& descendant,
                       const LayoutBoxModelObject* ancestor,
                       const PhysicalOffset& post_offset) = 0;
  virtual void Combine(OutlineRectCollector*,
                       const PhysicalOffset& additional_offset) = 0;
  virtual bool IsEmpty() const = 0;
};

class CORE_EXPORT UnionOutlineRectCollector
    : public GarbageCollected<UnionOutlineRectCollector>,
      public OutlineRectCollector {
 public:
  virtual ~UnionOutlineRectCollector() = default;

  Type GetType() const final { return Type::kUnion; }

  void AddRect(const PhysicalRect& r) final { rect_.Unite(r); }
  const PhysicalRect& Rect() const { return rect_; }

  OutlineRectCollector* ForDescendantCollector() const final {
    return MakeGarbageCollected<UnionOutlineRectCollector>();
  }

  void Combine(OutlineRectCollector* collector,
               const LayoutObject& descendant,
               const LayoutBoxModelObject* ancestor,
               const PhysicalOffset& post_offset) final;
  void Combine(OutlineRectCollector*,
               const PhysicalOffset& additional_offset) final;

  bool IsEmpty() const final { return rect_.IsEmpty(); }

  void Trace(Visitor* visitor) const {}

 private:
  PhysicalRect rect_;
};

class CORE_EXPORT VectorOutlineRectCollector
    : public GarbageCollected<VectorOutlineRectCollector>,
      public OutlineRectCollector {
 public:
  virtual ~VectorOutlineRectCollector() = default;

  Type GetType() const final { return Type::kVector; }

  void AddRect(const PhysicalRect& r) override { rects_.push_back(r); }
  VectorOf<PhysicalRect> TakeRects() { return std::move(rects_); }

  OutlineRectCollector* ForDescendantCollector() const final {
    return MakeGarbageCollected<VectorOutlineRectCollector>();
  }

  void Combine(OutlineRectCollector* collector,
               const LayoutObject& descendant,
               const LayoutBoxModelObject* ancestor,
               const PhysicalOffset& post_offset) final;
  void Combine(OutlineRectCollector*,
               const PhysicalOffset& additional_offset) final;

  bool IsEmpty() const final { return rects_.empty(); }

  void Trace(Visitor* visitor) const {}

 private:
  VectorOf<PhysicalRect> rects_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_OUTLINE_RECT_COLLECTOR_H_
