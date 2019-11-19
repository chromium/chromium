// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FIND_PAINT_OFFSET_AND_VISUAL_RECT_NEEDING_UPDATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FIND_PAINT_OFFSET_AND_VISUAL_RECT_NEEDING_UPDATE_H_

#if DCHECK_IS_ON()

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_builder.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// This file contains scope classes for catching cases where paint offset or
// visual rect needed an update but were not marked as such. If paint offset or
// any visual rect (including visual rect of the object itself, scroll controls,
// caret, selection, etc.) will change, the object must be marked as such by
// LayoutObject::setNeedsPaintOffsetAndVisualRectUpdate() (which is a private
// function called by several public paint-invalidation-flag setting functions).

class FindPaintOffsetNeedingUpdateScope {
  STACK_ALLOCATED();

 public:
  FindPaintOffsetNeedingUpdateScope(const LayoutObject& object,
                                    const FragmentData& fragment_data,
                                    const bool& is_actually_needed)
      : object_(object),
        fragment_data_(fragment_data),
        is_actually_needed_(is_actually_needed),
        old_paint_offset_(fragment_data.PaintOffset()) {
    if (const auto* properties = fragment_data.PaintProperties()) {
      if (const auto* translation = properties->PaintOffsetTranslation()) {
        old_parent_ = translation->Parent();
        old_translation_ = translation->Translation2D();
      }
    }
  }

  ~FindPaintOffsetNeedingUpdateScope() {
    if (is_actually_needed_)
      return;
    auto paint_offset = fragment_data_.PaintOffset();
    DCHECK_EQ(old_paint_offset_, paint_offset) << object_.DebugName();

    const TransformPaintPropertyNode* new_parent = nullptr;
    base::Optional<FloatSize> new_translation;
    if (const auto* properties = fragment_data_.PaintProperties()) {
      if (const auto* translation = properties->PaintOffsetTranslation()) {
        new_parent = translation->Parent();
        new_translation = translation->Translation2D();
      }
    }
    DCHECK_EQ(!!old_translation_, !!new_translation) << object_.DebugName();
    DCHECK_EQ(old_parent_, new_parent) << object_.DebugName();
    if (old_translation_ && new_translation)
      DCHECK_EQ(*old_translation_, *new_translation) << object_.DebugName();
  }

 private:
  const LayoutObject& object_;
  const FragmentData& fragment_data_;
  const bool& is_actually_needed_;
  PhysicalOffset old_paint_offset_;
  const TransformPaintPropertyNode* old_parent_ = nullptr;
  base::Optional<FloatSize> old_translation_;
};

class FindVisualRectNeedingUpdateScopeBase {
 protected:
  FindVisualRectNeedingUpdateScopeBase(const LayoutObject& object,
                                       const PaintInvalidatorContext& context,
                                       const IntRect& old_visual_rect)
      : object_(object),
        context_(context),
        old_visual_rect_(old_visual_rect),
        needed_visual_rect_update_(context.NeedsVisualRectUpdate(object)) {
    if (needed_visual_rect_update_) {
      DCHECK(context.tree_builder_context_actually_needed_);
      return;
    }
    context.force_visual_rect_update_for_checking_ = true;
    DCHECK(context.NeedsVisualRectUpdate(object));
  }

  ~FindVisualRectNeedingUpdateScopeBase() {
    context_.force_visual_rect_update_for_checking_ = false;
    DCHECK_EQ(needed_visual_rect_update_,
              context_.NeedsVisualRectUpdate(object_));
  }

  static IntRect InflatedRect(const IntRect& r) {
    IntRect result = r;
    result.Inflate(1);
    return result;
  }

  void CheckVisualRect(const IntRect& new_visual_rect) {
    if (needed_visual_rect_update_)
      return;
    DCHECK((old_visual_rect_.IsEmpty() && new_visual_rect.IsEmpty()) ||
           object_.EnclosingLayer()->SubtreeIsInvisible() ||
           old_visual_rect_ == new_visual_rect ||
           // The following check is to tolerate the differences caused by
           // pixel snapping that may happen for one rect but not for another
           // while we need neither paint invalidation nor raster invalidation
           // for the change. This may miss some real subpixel changes of visual
           // rects. TODO(wangxianzhu): Look into whether we can tighten this
           // for CAP.
           (InflatedRect(old_visual_rect_).Contains(new_visual_rect) &&
            InflatedRect(new_visual_rect).Contains(old_visual_rect_)))
        << "Visual rect changed without needing update"
        << " object=" << object_.DebugName()
        << " old=" << old_visual_rect_.ToString()
        << " new=" << new_visual_rect.ToString();
  }

  const LayoutObject& object_;
  const PaintInvalidatorContext& context_;
  IntRect old_visual_rect_;
  bool needed_visual_rect_update_;
};

// For updates of visual rects (e.g. of scroll controls, caret, selection,etc.)
// contained by an object.
class FindVisualRectNeedingUpdateScope : FindVisualRectNeedingUpdateScopeBase {
 public:
  FindVisualRectNeedingUpdateScope(const LayoutObject& object,
                                   const PaintInvalidatorContext& context,
                                   const IntRect& old_visual_rect,
                                   // Must be a reference to a rect that
                                   // outlives this scope.
                                   const IntRect& new_visual_rect)
      : FindVisualRectNeedingUpdateScopeBase(object, context, old_visual_rect),
        new_visual_rect_ref_(new_visual_rect) {}

  ~FindVisualRectNeedingUpdateScope() { CheckVisualRect(new_visual_rect_ref_); }

 private:
  const IntRect& new_visual_rect_ref_;
};

// For updates of object visual rect and location.
class FindObjectVisualRectNeedingUpdateScope
    : FindVisualRectNeedingUpdateScopeBase {
 public:
  FindObjectVisualRectNeedingUpdateScope(const LayoutObject& object,
                                         const FragmentData& fragment_data,
                                         const PaintInvalidatorContext& context)
      : FindVisualRectNeedingUpdateScopeBase(object,
                                             context,
                                             fragment_data.VisualRect()),
        fragment_data_(fragment_data) {}

  ~FindObjectVisualRectNeedingUpdateScope() {
    CheckVisualRect(fragment_data_.VisualRect());
  }

 private:
  const FragmentData& fragment_data_;
};

}  // namespace blink

#endif  // DCHECK_IS_ON()

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FIND_PAINT_OFFSET_AND_VISUAL_RECT_NEEDING_UPDATE_H_
