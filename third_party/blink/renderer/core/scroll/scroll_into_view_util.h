// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_INTO_VIEW_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_INTO_VIEW_UTIL_H_

#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace gfx {
class RectF;
}

namespace blink {

class LayoutObject;
class LayoutView;
struct PhysicalRect;

namespace scroll_into_view_util {

// Takes the given rect, in absolute coordinates of the frame of the given
// LayoutObject, and scrolls the LayoutObject and all its containers such that
// the child content of the LayoutObject at that rect is visible in the
// viewport.
// TODO(bokan): `from_remote_frame` is temporary, to track cross-origin
// scroll-into-view prevalence. https://crbug.com/1339003.
void CORE_EXPORT ScrollRectToVisible(const LayoutObject&,
                                     const PhysicalRect&,
                                     mojom::blink::ScrollIntoViewParamsPtr,
                                     bool from_remote_frame = false);

// ScrollFocusedEditableIntoView uses the caret rect for ScrollIntoView but
// stores enough information in `params` so that the editable element's bounds
// can be reconstructed. Given `caret_rect`, this will return the editable
// element's rect (in whatever coordinate space `caret_rect` is in).
gfx::RectF FocusedEditableBoundsFromParams(
    const gfx::RectF& caret_rect,
    const mojom::blink::ScrollIntoViewParamsPtr& params);

// Whenever ScrollIntoView bubbles up across a frame boundary, the origin for
// the absolute coordinate space changes. This function will convert any
// parameters in `params` into the updated coordinate space.
void ConvertParamsToParentFrame(mojom::blink::ScrollIntoViewParamsPtr& params,
                                const gfx::RectF& caret_rect_in_src,
                                const LayoutObject& src_frame,
                                const LayoutView& dest_frame);

}  // namespace scroll_into_view_util

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_INTO_VIEW_UTIL_H_
