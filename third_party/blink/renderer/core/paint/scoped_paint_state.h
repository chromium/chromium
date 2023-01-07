// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SCOPED_PAINT_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SCOPED_PAINT_STATE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/mobile_metrics/mobile_friendliness_checker.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"

namespace blink {

// Adjusts paint chunk properties, cull rect of the input PaintInfo and finds
// the paint offset for a LayoutObject or an NGPaintFragment before painting.
//
// Normally a Paint(const PaintInfo&) method creates an ScopedPaintState and
// holds it in the stack, and pass its GetPaintInfo() and PaintOffset() to the
// other PaintXXX() methods that paint different parts of the object.
//
// Each object create its own ScopedPaintState, so ScopedPaintState created for
// one object won't be passed to another object. Instead, PaintInfo is passed
// between objects.
class ScopedPaintState {
  STACK_ALLOCATED();

 public:
  // If |paint_legacy_table_part_in_ancestor_layer| is true, we'll
  // unconditionally apply PaintOffsetTranslation adjustment. For self-painting
  // layers, this adjustment is typically applied by PaintLayerPainter rather
  // than ScopedPaintState, but legacy tables table parts sometimes paint into
  // ancestor's self-painting layer instead of their own.
  // TODO(layout-dev): Remove this parameter when removing legacy table classes.
  ScopedPaintState(const LayoutObject&,
                   const PaintInfo&,
                   const FragmentData*,
                   bool painting_legacy_table_part_in_ancestor_layer = false);

  ScopedPaintState(const LayoutObject& object,
                   const PaintInfo& paint_info,
                   bool painting_legacy_table_part_in_ancestor_layer = false)
      : ScopedPaintState(object,
                         paint_info,
                         paint_info.FragmentToPaint(object),
                         painting_legacy_table_part_in_ancestor_layer) {}

  ScopedPaintState(const NGPhysicalFragment& fragment,
                   const PaintInfo& paint_info)
      : ScopedPaintState(*fragment.GetLayoutObject(),
                         paint_info,
                         paint_info.FragmentToPaint(fragment)) {}

  ~ScopedPaintState() {
    if (paint_offset_translation_as_drawing_)
      FinishPaintOffsetTranslationAsDrawing();
  }

  const PaintInfo& GetPaintInfo() const {
    return adjusted_paint_info_ ? *adjusted_paint_info_ : input_paint_info_;
  }

  PaintInfo& MutablePaintInfo() {
    if (!adjusted_paint_info_)
      adjusted_paint_info_.emplace(input_paint_info_);
    return *adjusted_paint_info_;
  }

  PhysicalOffset PaintOffset() const { return paint_offset_; }

  const FragmentData* FragmentToPaint() const { return fragment_to_paint_; }

  bool LocalRectIntersectsCullRect(const PhysicalRect& local_rect) const {
    return GetPaintInfo().IntersectsCullRect(local_rect, PaintOffset());
  }

 protected:
  // Constructors for subclasses to create the initial state before adjustment.
  ScopedPaintState(const ScopedPaintState& input)
      : fragment_to_paint_(input.fragment_to_paint_),
        input_paint_info_(input.GetPaintInfo()),
        paint_offset_(input.PaintOffset()) {}
  ScopedPaintState(const PaintInfo& paint_info,
                   const PhysicalOffset& paint_offset,
                   const LayoutObject& object)
      : fragment_to_paint_(paint_info.FragmentToPaint(object)),
        input_paint_info_(paint_info),
        paint_offset_(paint_offset) {}

 private:
  void AdjustForPaintProperties(const LayoutObject&);

  void FinishPaintOffsetTranslationAsDrawing();

 protected:
  const FragmentData* fragment_to_paint_;
  const PaintInfo& input_paint_info_;
  PhysicalOffset paint_offset_;
  absl::optional<PaintInfo> adjusted_paint_info_;
  absl::optional<ScopedPaintChunkProperties> chunk_properties_;
  bool paint_offset_translation_as_drawing_ = false;
};

// Adjusts paint chunk properties, cull rect and paint offset of the input
// ScopedPaintState for box contents if needed.
class ScopedBoxContentsPaintState : public ScopedPaintState {
 public:
  ScopedBoxContentsPaintState(const ScopedPaintState& input,
                              const LayoutBox& box)
      : ScopedPaintState(input) {
    AdjustForBoxContents(box);
  }

  ScopedBoxContentsPaintState(const PaintInfo& paint_info,
                              const PhysicalOffset& paint_offset,
                              const LayoutBox& box)
      : ScopedPaintState(paint_info, paint_offset, box) {
    AdjustForBoxContents(box);
  }

 private:
  void AdjustForBoxContents(const LayoutBox&);
  absl::optional<MobileFriendlinessChecker::IgnoreBeyondViewportScope>
      mf_ignore_scope_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SCOPED_PAINT_STATE_H_
