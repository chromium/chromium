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
  explicit CullRectUpdater(PaintLayer& root_layer);

  void Update() { UpdateInternal(CullRect::Infinite()); }

 private:
  void UpdateInternal(const CullRect& input_cull_rect);
  void UpdateRecursively(PaintLayer&,
                         const PaintLayer& parent_painting_layer,
                         bool force_update);
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

  PaintLayer& root_layer_;
  PropertyTreeState root_state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_CULL_RECT_UPDATER_H_
