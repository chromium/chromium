/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 *               All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 */

#include "third_party/blink/renderer/core/layout/floating_objects.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/shapes/shape_outside_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

struct SameSizeAsFloatingObject {
  void* pointers[2];
  LayoutRect rect;
  uint32_t bitfields : 8;
};

static_assert(sizeof(FloatingObject) == sizeof(SameSizeAsFloatingObject),
              "FloatingObject should stay small");

FloatingObject::FloatingObject(LayoutBox* layout_object, Type type)
    : layout_object_(layout_object),
      originating_line_(nullptr),
      type_(type),
      should_paint_(true),
      is_descendant_(false),
      is_placed_(false),
      is_lowest_non_overhanging_float_in_child_(false)
#if DCHECK_IS_ON()
      ,
      is_in_placed_tree_(false),
      has_geometry_(false)
#endif
{
}

FloatingObject::FloatingObject(LayoutBox* layout_object,
                               Type type,
                               const LayoutRect& frame_rect,
                               bool should_paint,
                               bool is_descendant,
                               bool is_lowest_non_overhanging_float_in_child)
    : layout_object_(layout_object),
      originating_line_(nullptr),
      frame_rect_(frame_rect),
      type_(type),
      should_paint_(should_paint),
      is_descendant_(is_descendant),
      is_placed_(true),
      is_lowest_non_overhanging_float_in_child_(
          is_lowest_non_overhanging_float_in_child)
#if DCHECK_IS_ON()
      ,
      is_in_placed_tree_(false),
      has_geometry_(false)
#endif
{
}

std::unique_ptr<FloatingObject> FloatingObject::Create(LayoutBox* layout_object,
                                                       Type type) {
  std::unique_ptr<FloatingObject> new_obj =
      base::WrapUnique(new FloatingObject(layout_object, type));

  // If a layer exists, the float will paint itself. Otherwise someone else
  // will.
  new_obj->SetShouldPaint(!layout_object->HasSelfPaintingLayer());

  new_obj->SetIsDescendant(true);

  // We set SelfPaintingStatusChanged in case we get to the next compositing
  // update and still haven't decided who should paint the float. If we've
  // decided that the current float owner can paint it that step is unnecessary,
  // so we can clear it now.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
      new_obj->ShouldPaint() && layout_object->Layer() &&
      layout_object->Layer()->SelfPaintingStatusChanged())
    layout_object->Layer()->ClearSelfPaintingStatusChanged();

  return new_obj;
}

std::unique_ptr<FloatingObject> FloatingObject::CopyToNewContainer(
    LayoutSize offset,
    bool should_paint,
    bool is_descendant) const {
  return base::WrapUnique(new FloatingObject(
      GetLayoutObject(), GetType(),
      LayoutRect(FrameRect().Location() - offset, FrameRect().Size()),
      should_paint, is_descendant, IsLowestNonOverhangingFloatInChild()));
}

std::unique_ptr<FloatingObject> FloatingObject::UnsafeClone() const {
  std::unique_ptr<FloatingObject> clone_object = base::WrapUnique(
      new FloatingObject(GetLayoutObject(), GetType(), frame_rect_,
                         should_paint_, is_descendant_, false));
  clone_object->is_placed_ = is_placed_;
#if DCHECK_IS_ON()
  clone_object->has_geometry_ = has_geometry_;
#endif
  return clone_object;
}

template <FloatingObject::Type FloatTypeValue>
class ComputeFloatOffsetAdapter {
 public:
  typedef FloatingObjectInterval IntervalType;

  ComputeFloatOffsetAdapter(const LayoutBlockFlow* layout_object,
                            LayoutUnit line_top,
                            LayoutUnit line_bottom,
                            LayoutUnit offset)
      : layout_object_(layout_object),
        line_top_(line_top),
        line_bottom_(line_bottom),
        offset_(offset),
        outermost_float_(nullptr) {}

  virtual ~ComputeFloatOffsetAdapter() = default;

  LayoutUnit LowValue() const { return line_top_; }
  LayoutUnit HighValue() const { return line_bottom_; }
  void CollectIfNeeded(const IntervalType&);

  LayoutUnit Offset() const { return offset_; }

 protected:
  virtual bool UpdateOffsetIfNeeded(const FloatingObject&) = 0;

  const LayoutBlockFlow* layout_object_;
  LayoutUnit line_top_;
  LayoutUnit line_bottom_;
  LayoutUnit offset_;
  const FloatingObject* outermost_float_;
};

template <FloatingObject::Type FloatTypeValue>
class ComputeFloatOffsetForFloatLayoutAdapter
    : public ComputeFloatOffsetAdapter<FloatTypeValue> {
 public:
  ComputeFloatOffsetForFloatLayoutAdapter(const LayoutBlockFlow* layout_object,
                                          LayoutUnit line_top,
                                          LayoutUnit line_bottom,
                                          LayoutUnit offset)
      : ComputeFloatOffsetAdapter<FloatTypeValue>(layout_object,
                                                  line_top,
                                                  line_bottom,
                                                  offset) {}

  ~ComputeFloatOffsetForFloatLayoutAdapter() override = default;

  LayoutUnit HeightRemaining() const;

 protected:
  bool UpdateOffsetIfNeeded(const FloatingObject&) final;
};

template <FloatingObject::Type FloatTypeValue>
class ComputeFloatOffsetForLineLayoutAdapter
    : public ComputeFloatOffsetAdapter<FloatTypeValue> {
 public:
  ComputeFloatOffsetForLineLayoutAdapter(const LayoutBlockFlow* layout_object,
                                         LayoutUnit line_top,
                                         LayoutUnit line_bottom,
                                         LayoutUnit offset)
      : ComputeFloatOffsetAdapter<FloatTypeValue>(layout_object,
                                                  line_top,
                                                  line_bottom,
                                                  offset) {}

  ~ComputeFloatOffsetForLineLayoutAdapter() override = default;

 protected:
  bool UpdateOffsetIfNeeded(const FloatingObject&) final;
};

class FindNextFloatLogicalBottomAdapter {
 public:
  typedef FloatingObjectInterval IntervalType;

  FindNextFloatLogicalBottomAdapter(const LayoutBlockFlow& renderer,
                                    LayoutUnit below_logical_height)
      : layout_object_(renderer),
        below_logical_height_(below_logical_height),
        above_logical_height_(LayoutUnit::Max()),
        next_logical_bottom_(),
        next_shape_logical_bottom_() {}

  LayoutUnit LowValue() const { return below_logical_height_; }
  LayoutUnit HighValue() const { return above_logical_height_; }
  void CollectIfNeeded(const IntervalType&);

  LayoutUnit NextLogicalBottom() { return next_logical_bottom_; }
  LayoutUnit NextShapeLogicalBottom() { return next_shape_logical_bottom_; }

 private:
  const LayoutBlockFlow& layout_object_;
  LayoutUnit below_logical_height_;
  LayoutUnit above_logical_height_;
  LayoutUnit next_logical_bottom_;
  LayoutUnit next_shape_logical_bottom_;
};

inline static bool RangesIntersect(LayoutUnit float_top,
                                   LayoutUnit float_bottom,
                                   LayoutUnit object_top,
                                   LayoutUnit object_bottom) {
  if (object_top >= float_bottom || object_bottom < float_top)
    return false;

  // The top of the object overlaps the float
  if (object_top >= float_top)
    return true;

  // The object encloses the float
  if (object_top < float_top && object_bottom > float_bottom)
    return true;

  // The bottom of the object overlaps the float
  if (object_bottom > object_top && object_bottom > float_top &&
      object_bottom <= float_bottom)
    return true;

  return false;
}

inline void FindNextFloatLogicalBottomAdapter::CollectIfNeeded(
    const IntervalType& interval) {
  const FloatingObject& floating_object = *(interval.Data());
  if (!RangesIntersect(interval.Low(), interval.High(), below_logical_height_,
                       above_logical_height_))
    return;

  // All the objects returned from the tree should be already placed.
  DCHECK(floating_object.IsPlaced());
  DCHECK(RangesIntersect(layout_object_.LogicalTopForFloat(floating_object),
                         layout_object_.LogicalBottomForFloat(floating_object),
                         below_logical_height_, above_logical_height_));

  LayoutUnit float_bottom =
      layout_object_.LogicalBottomForFloat(floating_object);

  if (ShapeOutsideInfo* shape_outside =
          floating_object.GetLayoutObject()->GetShapeOutsideInfo()) {
    LayoutUnit shape_bottom =
        layout_object_.LogicalTopForFloat(floating_object) +
        layout_object_.MarginBeforeForChild(
            *floating_object.GetLayoutObject()) +
        shape_outside->ShapeLogicalBottom();
    // Use the shapeBottom unless it extends outside of the margin box, in which
    // case it is clipped.
    next_shape_logical_bottom_ = next_shape_logical_bottom_
                                     ? std::min(shape_bottom, float_bottom)
                                     : shape_bottom;
  } else {
    next_shape_logical_bottom_ =
        next_shape_logical_bottom_
            ? std::min(next_shape_logical_bottom_, float_bottom)
            : float_bottom;
  }

  next_logical_bottom_ = next_logical_bottom_
                             ? std::min(next_logical_bottom_, float_bottom)
                             : float_bottom;
}

LayoutUnit FloatingObjects::FindNextFloatLogicalBottomBelow(
    LayoutUnit logical_height) {
  FindNextFloatLogicalBottomAdapter adapter(*layout_object_, logical_height);
  PlacedFloatsTree().AllOverlapsWithAdapter(adapter);

  return adapter.NextShapeLogicalBottom();
}

LayoutUnit FloatingObjects::FindNextFloatLogicalBottomBelowForBlock(
    LayoutUnit logical_height) {
  FindNextFloatLogicalBottomAdapter adapter(*layout_object_, logical_height);
  PlacedFloatsTree().AllOverlapsWithAdapter(adapter);

  return adapter.NextLogicalBottom();
}

FloatingObjects::~FloatingObjects() = default;
void FloatingObjects::ClearLineBoxTreePointers() {
  // Clear references to originating lines, since the lines are being deleted
  FloatingObjectSetIterator end = set_.end();
  for (FloatingObjectSetIterator it = set_.begin(); it != end; ++it) {
    DCHECK(
        !((*it)->OriginatingLine()) ||
        (*it)->OriginatingLine()->GetLineLayoutItem().IsEqual(layout_object_));
    (*it)->SetOriginatingLine(nullptr);
  }
}

FloatingObjects::FloatingObjects(const LayoutBlockFlow* layout_object,
                                 bool horizontal_writing_mode)
    : placed_floats_tree_(WTF::kUninitializedTree),
      left_objects_count_(0),
      right_objects_count_(0),
      horizontal_writing_mode_(horizontal_writing_mode),
      layout_object_(layout_object),
      cached_horizontal_writing_mode_(false) {}

void FloatingObjects::Clear() {
  set_.clear();
  placed_floats_tree_.Clear();
  left_objects_count_ = 0;
  right_objects_count_ = 0;
  MarkLowestFloatLogicalBottomCacheAsDirty();
}

LayoutUnit FloatingObjects::LowestFloatLogicalBottom(
    FloatingObject::Type float_type) {
  bool is_in_horizontal_writing_mode = horizontal_writing_mode_;
  if (float_type != FloatingObject::kFloatLeftRight) {
    if (HasLowestFloatLogicalBottomCached(is_in_horizontal_writing_mode,
                                          float_type))
      return GetCachedlowestFloatLogicalBottom(float_type);
  } else {
    if (HasLowestFloatLogicalBottomCached(is_in_horizontal_writing_mode,
                                          FloatingObject::kFloatLeft) &&
        HasLowestFloatLogicalBottomCached(is_in_horizontal_writing_mode,
                                          FloatingObject::kFloatRight)) {
      return std::max(
          GetCachedlowestFloatLogicalBottom(FloatingObject::kFloatLeft),
          GetCachedlowestFloatLogicalBottom(FloatingObject::kFloatRight));
    }
  }

  LayoutUnit lowest_float_bottom;
  const FloatingObjectSet& floating_object_set = Set();
  FloatingObjectSetIterator end = floating_object_set.end();
  if (float_type == FloatingObject::kFloatLeftRight) {
    FloatingObject* lowest_floating_object_left = nullptr;
    FloatingObject* lowest_floating_object_right = nullptr;
    LayoutUnit lowest_float_bottom_left;
    LayoutUnit lowest_float_bottom_right;
    for (FloatingObjectSetIterator it = floating_object_set.begin(); it != end;
         ++it) {
      FloatingObject& floating_object = *it->get();
      if (floating_object.IsPlaced()) {
        FloatingObject::Type cur_type = floating_object.GetType();
        LayoutUnit cur_float_logical_bottom =
            layout_object_->LogicalBottomForFloat(floating_object);
        if (cur_type & FloatingObject::kFloatLeft &&
            cur_float_logical_bottom > lowest_float_bottom_left) {
          lowest_float_bottom_left = cur_float_logical_bottom;
          lowest_floating_object_left = &floating_object;
        }
        if (cur_type & FloatingObject::kFloatRight &&
            cur_float_logical_bottom > lowest_float_bottom_right) {
          lowest_float_bottom_right = cur_float_logical_bottom;
          lowest_floating_object_right = &floating_object;
        }
      }
    }
    lowest_float_bottom =
        std::max(lowest_float_bottom_left, lowest_float_bottom_right);
    SetCachedLowestFloatLogicalBottom(is_in_horizontal_writing_mode,
                                      FloatingObject::kFloatLeft,
                                      lowest_floating_object_left);
    SetCachedLowestFloatLogicalBottom(is_in_horizontal_writing_mode,
                                      FloatingObject::kFloatRight,
                                      lowest_floating_object_right);
  } else {
    FloatingObject* lowest_floating_object = nullptr;
    for (FloatingObjectSetIterator it = floating_object_set.begin(); it != end;
         ++it) {
      FloatingObject& floating_object = *it->get();
      if (floating_object.IsPlaced() &&
          floating_object.GetType() == float_type) {
        if (layout_object_->LogicalBottomForFloat(floating_object) >
            lowest_float_bottom) {
          lowest_floating_object = &floating_object;
          lowest_float_bottom =
              layout_object_->LogicalBottomForFloat(floating_object);
        }
      }
    }
    SetCachedLowestFloatLogicalBottom(is_in_horizontal_writing_mode, float_type,
                                      lowest_floating_object);
  }

  return lowest_float_bottom;
}

bool FloatingObjects::HasLowestFloatLogicalBottomCached(
    bool is_horizontal,
    FloatingObject::Type type) const {
  int float_index = static_cast<int>(type) - 1;
  DCHECK_LT(float_index, static_cast<int>(sizeof(lowest_float_bottom_cache_) /
                                          sizeof(FloatBottomCachedValue)));
  DCHECK_GE(float_index, 0);
  return (cached_horizontal_writing_mode_ == is_horizontal &&
          !lowest_float_bottom_cache_[float_index].dirty);
}

LayoutUnit FloatingObjects::GetCachedlowestFloatLogicalBottom(
    FloatingObject::Type type) const {
  int float_index = static_cast<int>(type) - 1;
  DCHECK_LT(float_index, static_cast<int>(sizeof(lowest_float_bottom_cache_) /
                                          sizeof(FloatBottomCachedValue)));
  DCHECK_GE(float_index, 0);
  if (!lowest_float_bottom_cache_[float_index].floating_object)
    return LayoutUnit();
  return layout_object_->LogicalBottomForFloat(
      *lowest_float_bottom_cache_[float_index].floating_object);
}

void FloatingObjects::SetCachedLowestFloatLogicalBottom(
    bool is_horizontal,
    FloatingObject::Type type,
    FloatingObject* floating_object) {
  int float_index = static_cast<int>(type) - 1;
  DCHECK_LT(float_index, static_cast<int>(sizeof(lowest_float_bottom_cache_) /
                                          sizeof(FloatBottomCachedValue)));
  DCHECK_GE(float_index, 0);
  cached_horizontal_writing_mode_ = is_horizontal;
  lowest_float_bottom_cache_[float_index].floating_object = floating_object;
  lowest_float_bottom_cache_[float_index].dirty = false;
}

FloatingObject* FloatingObjects::LowestFloatingObject() {
  bool is_in_horizontal_writing_mode = horizontal_writing_mode_;

  // If we haven't yet found our lowest float, calculate it now:
  if (!HasLowestFloatLogicalBottomCached(is_in_horizontal_writing_mode,
                                         FloatingObject::kFloatLeft) &&
      !HasLowestFloatLogicalBottomCached(is_in_horizontal_writing_mode,
                                         FloatingObject::kFloatRight))
    LowestFloatLogicalBottom(FloatingObject::kFloatLeftRight);

  FloatingObject* lowest_left_object =
      lowest_float_bottom_cache_[0].floating_object;
  FloatingObject* lowest_right_object =
      lowest_float_bottom_cache_[1].floating_object;
  LayoutUnit lowest_float_bottom_left =
      lowest_left_object
          ? layout_object_->LogicalBottomForFloat(*lowest_left_object)
          : LayoutUnit();
  LayoutUnit lowest_float_bottom_right =
      lowest_right_object
          ? layout_object_->LogicalBottomForFloat(*lowest_right_object)
          : LayoutUnit();

  if (lowest_float_bottom_left > lowest_float_bottom_right)
    return lowest_left_object;
  return lowest_right_object;
}

void FloatingObjects::MarkLowestFloatLogicalBottomCacheAsDirty() {
  for (size_t i = 0;
       i < sizeof(lowest_float_bottom_cache_) / sizeof(FloatBottomCachedValue);
       ++i)
    lowest_float_bottom_cache_[i].dirty = true;
}

void FloatingObjects::MoveAllToFloatInfoMap(LayoutBoxToFloatInfoMap& map) {
  while (!set_.IsEmpty()) {
    std::unique_ptr<FloatingObject> floating_object = set_.TakeFirst();
    LayoutBox* layout_object = floating_object->GetLayoutObject();
    map.insert(layout_object, std::move(floating_object));
  }
  Clear();
}

inline void FloatingObjects::IncreaseObjectsCount(FloatingObject::Type type) {
  if (type == FloatingObject::kFloatLeft)
    left_objects_count_++;
  else
    right_objects_count_++;
}

inline void FloatingObjects::DecreaseObjectsCount(FloatingObject::Type type) {
  if (type == FloatingObject::kFloatLeft)
    left_objects_count_--;
  else
    right_objects_count_--;
}

inline FloatingObjectInterval FloatingObjects::IntervalForFloatingObject(
    FloatingObject& floating_object) {
  if (horizontal_writing_mode_)
    return FloatingObjectInterval(floating_object.FrameRect().Y(),
                                  floating_object.FrameRect().MaxY(),
                                  &floating_object);
  return FloatingObjectInterval(floating_object.FrameRect().X(),
                                floating_object.FrameRect().MaxX(),
                                &floating_object);
}

void FloatingObjects::AddPlacedObject(FloatingObject& floating_object) {
  DCHECK(!layout_object_->IsLayoutNGMixin());
  DCHECK(!floating_object.IsInPlacedTree());

  floating_object.SetIsPlaced(true);
  if (placed_floats_tree_.IsInitialized())
    placed_floats_tree_.Add(IntervalForFloatingObject(floating_object));

#if DCHECK_IS_ON()
  floating_object.SetIsInPlacedTree(true);
#endif
  MarkLowestFloatLogicalBottomCacheAsDirty();
}

void FloatingObjects::RemovePlacedObject(FloatingObject& floating_object) {
  DCHECK(!layout_object_->IsLayoutNGMixin());
  DCHECK(floating_object.IsPlaced());
  DCHECK(floating_object.IsInPlacedTree());

  if (placed_floats_tree_.IsInitialized()) {
    bool removed =
        placed_floats_tree_.Remove(IntervalForFloatingObject(floating_object));
    DCHECK(removed);
  }

  floating_object.SetIsPlaced(false);
#if DCHECK_IS_ON()
  floating_object.SetIsInPlacedTree(false);
#endif
  MarkLowestFloatLogicalBottomCacheAsDirty();
}

FloatingObject* FloatingObjects::Add(
    std::unique_ptr<FloatingObject> floating_object) {
  FloatingObject* new_object = floating_object.release();
  IncreaseObjectsCount(new_object->GetType());
  set_.insert(base::WrapUnique(new_object));
  if (new_object->IsPlaced())
    AddPlacedObject(*new_object);
  MarkLowestFloatLogicalBottomCacheAsDirty();
  return new_object;
}

void FloatingObjects::Remove(FloatingObject* to_be_removed) {
  DecreaseObjectsCount(to_be_removed->GetType());
  std::unique_ptr<FloatingObject> floating_object = set_.Take(to_be_removed);
  DCHECK(floating_object->IsPlaced() || !floating_object->IsInPlacedTree());
  if (floating_object->IsPlaced())
    RemovePlacedObject(*floating_object);
  MarkLowestFloatLogicalBottomCacheAsDirty();
  DCHECK(!floating_object->OriginatingLine());
}

void FloatingObjects::ComputePlacedFloatsTree() {
  DCHECK(!placed_floats_tree_.IsInitialized());
  if (set_.IsEmpty())
    return;
  placed_floats_tree_.InitIfNeeded(layout_object_->View()->GetIntervalArena());
  FloatingObjectSetIterator it = set_.begin();
  FloatingObjectSetIterator end = set_.end();
  for (; it != end; ++it) {
    FloatingObject& floating_object = *it->get();
    if (floating_object.IsPlaced())
      placed_floats_tree_.Add(IntervalForFloatingObject(floating_object));
  }
}

LayoutUnit FloatingObjects::LogicalLeftOffsetForPositioningFloat(
    LayoutUnit fixed_offset,
    LayoutUnit logical_top,
    LayoutUnit* height_remaining) {
  ComputeFloatOffsetForFloatLayoutAdapter<FloatingObject::kFloatLeft> adapter(
      layout_object_, logical_top, logical_top, fixed_offset);
  PlacedFloatsTree().AllOverlapsWithAdapter(adapter);

  if (height_remaining)
    *height_remaining = adapter.HeightRemaining();

  return adapter.Offset();
}

LayoutUnit FloatingObjects::LogicalRightOffsetForPositioningFloat(
    LayoutUnit fixed_offset,
    LayoutUnit logical_top,
    LayoutUnit* height_remaining) {
  ComputeFloatOffsetForFloatLayoutAdapter<FloatingObject::kFloatRight> adapter(
      layout_object_, logical_top, logical_top, fixed_offset);
  PlacedFloatsTree().AllOverlapsWithAdapter(adapter);

  if (height_remaining)
    *height_remaining = adapter.HeightRemaining();

  return std::min(fixed_offset, adapter.Offset());
}

LayoutUnit FloatingObjects::LogicalLeftOffset(LayoutUnit fixed_offset,
                                              LayoutUnit logical_top,
                                              LayoutUnit logical_height) {
  ComputeFloatOffsetForLineLayoutAdapter<FloatingObject::kFloatLeft> adapter(
      layout_object_, logical_top, logical_top + logical_height, fixed_offset);
  PlacedFloatsTree().AllOverlapsWithAdapter(adapter);

  return adapter.Offset();
}

LayoutUnit FloatingObjects::LogicalRightOffset(LayoutUnit fixed_offset,
                                               LayoutUnit logical_top,
                                               LayoutUnit logical_height) {
  ComputeFloatOffsetForLineLayoutAdapter<FloatingObject::kFloatRight> adapter(
      layout_object_, logical_top, logical_top + logical_height, fixed_offset);
  PlacedFloatsTree().AllOverlapsWithAdapter(adapter);

  return std::min(fixed_offset, adapter.Offset());
}

LayoutUnit FloatingObjects::LogicalLeftOffsetForAvoidingFloats(
    LayoutUnit fixed_offset,
    LayoutUnit logical_top,
    LayoutUnit logical_height) {
  ComputeFloatOffsetForFloatLayoutAdapter<FloatingObject::kFloatLeft> adapter(
      layout_object_, logical_top, logical_top + logical_height, fixed_offset);
  PlacedFloatsTree().AllOverlapsWithAdapter(adapter);

  return adapter.Offset();
}

LayoutUnit FloatingObjects::LogicalRightOffsetForAvoidingFloats(
    LayoutUnit fixed_offset,
    LayoutUnit logical_top,
    LayoutUnit logical_height) {
  ComputeFloatOffsetForFloatLayoutAdapter<FloatingObject::kFloatRight> adapter(
      layout_object_, logical_top, logical_top + logical_height, fixed_offset);
  PlacedFloatsTree().AllOverlapsWithAdapter(adapter);

  return std::min(fixed_offset, adapter.Offset());
}

FloatingObjects::FloatBottomCachedValue::FloatBottomCachedValue()
    : floating_object(nullptr), dirty(true) {}

template <>
inline bool ComputeFloatOffsetForFloatLayoutAdapter<
    FloatingObject::kFloatLeft>::UpdateOffsetIfNeeded(const FloatingObject&
                                                          floating_object) {
  LayoutUnit logical_right =
      layout_object_->LogicalRightForFloat(floating_object);
  if (logical_right > offset_) {
    offset_ = logical_right;
    return true;
  }
  return false;
}

template <>
inline bool ComputeFloatOffsetForFloatLayoutAdapter<
    FloatingObject::kFloatRight>::UpdateOffsetIfNeeded(const FloatingObject&
                                                           floating_object) {
  LayoutUnit logical_left =
      layout_object_->LogicalLeftForFloat(floating_object);
  if (logical_left < offset_) {
    offset_ = logical_left;
    return true;
  }
  return false;
}

template <FloatingObject::Type FloatTypeValue>
LayoutUnit ComputeFloatOffsetForFloatLayoutAdapter<
    FloatTypeValue>::HeightRemaining() const {
  return this->outermost_float_ ? this->layout_object_->LogicalBottomForFloat(
                                      *this->outermost_float_) -
                                      this->line_top_
                                : LayoutUnit(1);
}

template <FloatingObject::Type FloatTypeValue>
DISABLE_CFI_PERF inline void
ComputeFloatOffsetAdapter<FloatTypeValue>::CollectIfNeeded(
    const IntervalType& interval) {
  const FloatingObject& floating_object = *(interval.Data());
  if (floating_object.GetType() != FloatTypeValue ||
      !RangesIntersect(interval.Low(), interval.High(), line_top_,
                       line_bottom_))
    return;

  // Make sure the float hasn't changed since it was added to the placed floats
  // tree.
  DCHECK(floating_object.IsPlaced());
  DCHECK_EQ(interval.Low(),
            layout_object_->LogicalTopForFloat(floating_object));
  DCHECK_EQ(interval.High(),
            layout_object_->LogicalBottomForFloat(floating_object));

  bool float_is_new_extreme = UpdateOffsetIfNeeded(floating_object);
  if (float_is_new_extreme)
    outermost_float_ = &floating_object;
}

template <>
inline bool ComputeFloatOffsetForLineLayoutAdapter<FloatingObject::kFloatLeft>::
    UpdateOffsetIfNeeded(const FloatingObject& floating_object) {
  LayoutUnit logical_right =
      layout_object_->LogicalRightForFloat(floating_object);
  if (ShapeOutsideInfo* shape_outside =
          floating_object.GetLayoutObject()->GetShapeOutsideInfo()) {
    ShapeOutsideDeltas shape_deltas =
        shape_outside->ComputeDeltasForContainingBlockLine(
            LineLayoutBlockFlow(const_cast<LayoutBlockFlow*>(layout_object_)),
            floating_object, line_top_, line_bottom_ - line_top_);
    if (!shape_deltas.LineOverlapsShape())
      return false;

    logical_right += shape_deltas.RightMarginBoxDelta();
  }
  if (logical_right > offset_) {
    offset_ = logical_right;
    return true;
  }

  return false;
}

template <>
inline bool ComputeFloatOffsetForLineLayoutAdapter<
    FloatingObject::kFloatRight>::UpdateOffsetIfNeeded(const FloatingObject&
                                                           floating_object) {
  LayoutUnit logical_left =
      layout_object_->LogicalLeftForFloat(floating_object);
  if (ShapeOutsideInfo* shape_outside =
          floating_object.GetLayoutObject()->GetShapeOutsideInfo()) {
    ShapeOutsideDeltas shape_deltas =
        shape_outside->ComputeDeltasForContainingBlockLine(
            LineLayoutBlockFlow(const_cast<LayoutBlockFlow*>(layout_object_)),
            floating_object, line_top_, line_bottom_ - line_top_);
    if (!shape_deltas.LineOverlapsShape())
      return false;

    logical_left += shape_deltas.LeftMarginBoxDelta();
  }
  if (logical_left < offset_) {
    offset_ = logical_left;
    return true;
  }

  return false;
}

}  // namespace blink

namespace WTF {
#ifndef NDEBUG
// These helpers are only used by the PODIntervalTree for debugging purposes.
String ValueToString<blink::LayoutUnit>::ToString(
    const blink::LayoutUnit value) {
  return String::Number(value.ToFloat());
}

String ValueToString<blink::FloatingObject*>::ToString(
    const blink::FloatingObject* floating_object) {
  return String::Format("%p (%gx%g %gx%g)", floating_object,
                        floating_object->FrameRect().X().ToFloat(),
                        floating_object->FrameRect().Y().ToFloat(),
                        floating_object->FrameRect().MaxX().ToFloat(),
                        floating_object->FrameRect().MaxY().ToFloat());
}
#endif
}  // namespace WTF
