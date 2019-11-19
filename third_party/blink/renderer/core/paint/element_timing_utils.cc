// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/element_timing_utils.h"

#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_base.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"

namespace blink {

// static
FloatRect ElementTimingUtils::ComputeIntersectionRect(
    LocalFrame* frame,
    const IntRect& int_visual_rect,
    const PropertyTreeState& current_paint_chunk_properties) {
  // Compute the visible part of the image rect.
  FloatClipRect visual_rect = FloatClipRect(FloatRect(int_visual_rect));
  GeometryMapper::LocalToAncestorVisualRect(current_paint_chunk_properties,
                                            frame->View()
                                                ->GetLayoutView()
                                                ->FirstFragment()
                                                .LocalBorderBoxProperties(),
                                            visual_rect);
  WebFloatRect intersection_rect = visual_rect.Rect();
  WebFrameWidgetBase* widget =
      WebLocalFrameImpl::FromFrame(frame)->LocalRootFrameWidget();
  DCHECK(widget);
  widget->Client()->ConvertViewportToWindow(&intersection_rect);
  return intersection_rect;
}

}  // namespace blink
