// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_INPUTS_UPDATER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_INPUTS_UPDATER_H_

#include "third_party/blink/renderer/core/layout/layout_geometry_map.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class PaintLayer;
class CompositingReasonFinder;

class CompositingInputsUpdater {
  STACK_ALLOCATED();

 public:
  explicit CompositingInputsUpdater(
      PaintLayer* root_layer,
      CompositingReasonFinder& compositing_reason_finder);
  ~CompositingInputsUpdater();

  void Update();

#if DCHECK_IS_ON()
  static void AssertNeedsCompositingInputsUpdateBitsCleared(PaintLayer*);
#endif

 private:
  enum UpdateType {
    kDoNotForceUpdate,
    kForceUpdate,
  };

  struct AncestorInfo {
    PaintLayer* enclosing_composited_layer = nullptr;
    // A "squashing composited layer" is a PaintLayer that owns a squashing
    // layer. This variable stores the squashing composited layer for the
    // nearest PaintLayer ancestor which is squashed.
    PaintLayer* enclosing_squashing_composited_layer = nullptr;
    PaintLayer* last_overflow_clip_layer = nullptr;

    PaintLayer* clip_chain_parent_for_absolute = nullptr;
    PaintLayer* clip_chain_parent_for_fixed = nullptr;
    // These flags are set if we encountered a stacking context
    // that will make descendants to inherit more clip than desired,
    // so we have to setup an alternative clip parent instead.
    PaintLayer* escape_clip_to = nullptr;
    PaintLayer* escape_clip_to_for_absolute = nullptr;
    PaintLayer* escape_clip_to_for_fixed = nullptr;

    PaintLayer* scrolling_ancestor = nullptr;
    PaintLayer* scrolling_ancestor_for_absolute = nullptr;
    PaintLayer* scrolling_ancestor_for_fixed = nullptr;
    // These flags are set to true if a non-stacking context scroller
    // is encountered, so that a descendant element won't inherit scroll
    // translation from its compositing ancestor directly thus having to
    // setup an alternative scroll parent instead.
    bool needs_reparent_scroll = false;
    bool needs_reparent_scroll_for_absolute = false;
    bool needs_reparent_scroll_for_fixed = false;
  };

  void UpdateRecursive(PaintLayer*, UpdateType, AncestorInfo);
  void UpdateAncestorDependentCompositingInputs(PaintLayer*,
                                                const AncestorInfo&);

  LayoutGeometryMap geometry_map_;
  PaintLayer* root_layer_;
  CompositingReasonFinder& compositing_reason_finder_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_INPUTS_UPDATER_H_
