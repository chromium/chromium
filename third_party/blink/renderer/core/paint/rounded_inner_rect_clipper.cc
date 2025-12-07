// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/rounded_inner_rect_clipper.h"

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/geometry/contoured_rect.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"

namespace blink {

RoundedInnerRectClipper::RoundedInnerRectClipper(GraphicsContext& context,
                                                 const PhysicalRect& rect,
                                                 const ContouredRect& clip_rect)
    : context_(context) {
  Vector<ContouredRect> contoured_rect_clips;
  if (clip_rect.IsRenderable()) {
    contoured_rect_clips.push_back(clip_rect);
  } else {
    const auto& curvature = clip_rect.GetCornerCurvature();
    // We create a rounded rect for each of the corners and clip it, while
    // making sure we clip opposing corners together.
    if (!clip_rect.GetRadii().TopLeft().IsEmpty() ||
        !clip_rect.GetRadii().BottomRight().IsEmpty()) {
      gfx::RectF top_corner(clip_rect.Rect().x(), clip_rect.Rect().y(),
                            rect.Right() - clip_rect.Rect().x(),
                            rect.Bottom() - clip_rect.Rect().y());
      FloatRoundedRect::Radii top_corner_radii;
      top_corner_radii.SetTopLeft(clip_rect.GetRadii().TopLeft());
      contoured_rect_clips.push_back(ContouredRect(
          FloatRoundedRect(top_corner, top_corner_radii), curvature));

      gfx::RectF bottom_corner(rect.X().ToFloat(), rect.Y().ToFloat(),
                               clip_rect.Rect().right() - rect.X().ToFloat(),
                               clip_rect.Rect().bottom() - rect.Y().ToFloat());
      FloatRoundedRect::Radii bottom_corner_radii;
      bottom_corner_radii.SetBottomRight(clip_rect.GetRadii().BottomRight());
      contoured_rect_clips.push_back(ContouredRect(
          FloatRoundedRect(bottom_corner, bottom_corner_radii), curvature));
    }

    if (!clip_rect.GetRadii().TopRight().IsEmpty() ||
        !clip_rect.GetRadii().BottomLeft().IsEmpty()) {
      gfx::RectF top_corner(rect.X().ToFloat(), clip_rect.Rect().y(),
                            clip_rect.Rect().right() - rect.X().ToFloat(),
                            rect.Bottom() - clip_rect.Rect().y());
      FloatRoundedRect::Radii top_corner_radii;
      top_corner_radii.SetTopRight(clip_rect.GetRadii().TopRight());
      contoured_rect_clips.push_back(ContouredRect(
          FloatRoundedRect(top_corner, top_corner_radii), curvature));

      gfx::RectF bottom_corner(clip_rect.Rect().x(), rect.Y().ToFloat(),
                               rect.Right() - clip_rect.Rect().x(),
                               clip_rect.Rect().bottom() - rect.Y().ToFloat());
      FloatRoundedRect::Radii bottom_corner_radii;
      bottom_corner_radii.SetBottomLeft(clip_rect.GetRadii().BottomLeft());
      contoured_rect_clips.push_back(ContouredRect(
          FloatRoundedRect(bottom_corner, bottom_corner_radii), curvature));
    }
  }

  context.Save();
  for (const auto& rrect : contoured_rect_clips) {
    context.ClipContouredRect(rrect);
  }
}

RoundedInnerRectClipper::~RoundedInnerRectClipper() {
  context_.Restore();
}

}  // namespace blink
