// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PAINT_CHUNKS_TO_CC_LAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PAINT_CHUNKS_TO_CC_LAYER_H_

#include "base/memory/scoped_refptr.h"
#include "cc/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace cc {
class DisplayItemList;
}  // namespace cc

namespace gfx {
class Vector2dF;
}  // namespace gfx

namespace blink {

class DisplayItemList;
class PaintChunkSubset;
class FloatSize;
class PropertyTreeState;
class RasterInvalidationTracking;

struct RasterUnderInvalidationCheckingParams {
  RasterUnderInvalidationCheckingParams(RasterInvalidationTracking& tracking,
                                        const IntRect& interest_rect,
                                        const String& debug_name)
      : tracking(tracking),
        interest_rect(interest_rect),
        debug_name(debug_name) {}

  RasterInvalidationTracking& tracking;
  IntRect interest_rect;
  String debug_name;
};

class PLATFORM_EXPORT PaintChunksToCcLayer {
  STATIC_ONLY(PaintChunksToCcLayer);

 public:
  // Converts a list of Blink paint chunks and display items into cc display
  // items, inserting appropriate begin/end items with respect to property
  // tree state. The converted items are appended into a unfinalized cc display
  // item list.
  // |layer_state| is the target property tree state of the output. This method
  // generates begin/end items for the relative state differences between the
  // layer state and the chunk state.
  // |layer_offset| is an extra translation on top of layer_state.Transform(),
  // in other word, point (x, y) in the output list maps to
  // layer_state.Transform() * (layer_offset + (x, y)) on the screen. It is
  // equivalent to say that |layer_offset| is the layer origin in the space
  // of layer_state.Transform().
  static void ConvertInto(const PaintChunkSubset&,
                          const PropertyTreeState& layer_state,
                          const gfx::Vector2dF& layer_offset,
                          const FloatSize& visual_rect_subpixel_offset,
                          const DisplayItemList&,
                          cc::DisplayItemList&);

  // Similar to ConvertInto(), but returns a finalized new list instead of
  // appending converted items to an existing list.
  static scoped_refptr<cc::DisplayItemList> Convert(
      const PaintChunkSubset&,
      const PropertyTreeState& layer_state,
      const gfx::Vector2dF& layer_offset,
      const DisplayItemList&,
      cc::DisplayItemList::UsageHint,
      RasterUnderInvalidationCheckingParams* = nullptr);
};

}  // namespace blink

#endif  // PaintArtifactCompositor_h
