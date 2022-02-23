// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_CULL_RECT_UPDATER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_CULL_RECT_UPDATER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class FragmentData;
class LayoutObject;
class PaintLayer;

// This class is used for updating the cull rects of PaintLayer fragments (see:
// |FragmentData::cull_rect_| and |FragmentData::contents_cull_rect_|.
// Cull rects are used as an optimization to limit painting to areas "near" the
// viewport. This update should happen during the PrePaint lifecycle stage.
//
// Dirty bits (see: |PaintLayer::NeedsCullRectUpdate()| and
// PaintLayer::DescendantNeedsCullRectUpdate()|) are used to optimize this
// update, and are cleared at the end.
class CORE_EXPORT CullRectUpdater {
  STACK_ALLOCATED();

 public:
  explicit CullRectUpdater(PaintLayer& starting_layer)
      : starting_layer_(starting_layer) {}

  void Update();

  static void PaintPropertiesChanged(const LayoutObject&,
                                     PaintLayer& painting_layer);

 private:
  friend class OverriddenCullRectScope;

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
};

// Used when painting with a custom top-level cull rect, e.g. when printing a
// page. It temporarily overrides the cull rect on the PaintLayer (which must be
// a stacking context) and marks the PaintLayer as needing to recalculate the
// cull rect when leaving this scope.
// TODO(crbug.com/1215251): Avoid repaint after the scope if the scope is used
// to paint into a separate PaintController.
class OverriddenCullRectScope {
  STACK_ALLOCATED();

 public:
  OverriddenCullRectScope(PaintLayer&, const CullRect&);
  ~OverriddenCullRectScope();

 private:
  PaintLayer& starting_layer_;
  bool updated_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_CULL_RECT_UPDATER_H_
