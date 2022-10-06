// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OLD_CULL_RECT_UPDATER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OLD_CULL_RECT_UPDATER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class FragmentData;
class LayoutObject;
class PaintLayer;
struct PaintPropertiesChangeInfo;

// This class is equivalent to the cull rect update code prior to r1033837, but
// with improvements made after r1033837. It only exists temporarily to compare
// the performance against the new cull rect update in |cull_rect_updater.h|.
// This code should only be used when ScrollUpdateOptimizations is enabled.
//
// This class is used for updating the cull rects of PaintLayer fragments (see:
// |FragmentData::cull_rect_| and |FragmentData::contents_cull_rect_|.
// Cull rects are used as an optimization to limit painting to areas "near" the
// viewport. This update should happen during the PrePaint lifecycle stage.
//
// Dirty bits (see: |PaintLayer::NeedsCullRectUpdate()| and
// PaintLayer::DescendantNeedsCullRectUpdate()|) are used to optimize this
// update, and are cleared at the end.
class CORE_EXPORT OldCullRectUpdater {
  STACK_ALLOCATED();

 public:
  explicit OldCullRectUpdater(PaintLayer& starting_layer);

  void Update();

  static void PaintPropertiesChanged(const LayoutObject&,
                                     PaintLayer& painting_layer,
                                     const PaintPropertiesChangeInfo&,
                                     const gfx::Vector2dF& old_scroll_offset);

 private:
  friend class OverriddenOldCullRectScope;

  void UpdateInternal(const CullRect& input_cull_rect);
  void UpdateRecursively(PaintLayer&,
                         const PaintLayer& parent_painting_layer,
                         bool force_update_self);
  // Returns true if the contents cull rect changed which requires forced update
  // for children.
  bool UpdateForSelf(PaintLayer&, const PaintLayer& parent_painting_layer);
  void UpdateForDescendants(PaintLayer&, bool force_update_children);
  CullRect ComputeFragmentCullRect(PaintLayer&,
                                   const FragmentData& fragment,
                                   const FragmentData& parent_fragment);
  CullRect ComputeFragmentContentsCullRect(PaintLayer&,
                                           const FragmentData& fragment,
                                           const CullRect& cull_rect);
  bool ShouldProactivelyUpdate(const PaintLayer&) const;

  PaintLayer& starting_layer_;
  PropertyTreeState root_state_ = PropertyTreeState::Uninitialized();
  bool force_proactive_update_ = false;
  bool subtree_is_out_of_cull_rect_ = false;
  bool subtree_should_use_infinite_cull_rect_ = false;
};

// Used when painting with a custom top-level cull rect, e.g. when printing a
// page. It temporarily overrides the cull rect on the PaintLayer (which must be
// a stacking context) and marks the PaintLayer as needing to recalculate the
// cull rect when leaving this scope.
// TODO(crbug.com/1215251): Avoid repaint after the scope if the scope is used
// to paint into a separate PaintController.
class CORE_EXPORT OverriddenOldCullRectScope {
  STACK_ALLOCATED();

 public:
  OverriddenOldCullRectScope(PaintLayer&, const CullRect&);
  ~OverriddenOldCullRectScope();

 private:
  PaintLayer& starting_layer_;
  bool updated_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OLD_CULL_RECT_UPDATER_H_
