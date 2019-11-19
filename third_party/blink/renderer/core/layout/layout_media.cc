/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_media.h"

#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/media_controls.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

LayoutMedia::LayoutMedia(HTMLMediaElement* video) : LayoutImage(video) {
  SetImageResource(MakeGarbageCollected<LayoutImageResource>());
}

LayoutMedia::~LayoutMedia() = default;

HTMLMediaElement* LayoutMedia::MediaElement() const {
  return ToHTMLMediaElement(GetNode());
}

void LayoutMedia::UpdateLayout() {
  LayoutSize old_size(ContentWidth(), ContentHeight());

  LayoutImage::UpdateLayout();

  auto new_rect = PhysicalContentBoxRect().ToLayoutRect();

  LayoutState state(*this);

// Iterate the children in reverse order so that the media controls are laid
// out before the text track container. This is to ensure that the text
// track rendering has an up-to-date position of the media controls for
// overlap checking, see LayoutVTTCue.
#if DCHECK_IS_ON()
  bool seen_text_track_container = false;
  bool seen_interstitial = false;
#endif
  for (LayoutObject* child = children_.LastChild(); child;
       child = child->PreviousSibling()) {
#if DCHECK_IS_ON()
    if (child->GetNode()->IsMediaControls()) {
      DCHECK(!seen_text_track_container);
      DCHECK(!seen_interstitial);
    } else if (child->GetNode()->IsTextTrackContainer()) {
      seen_text_track_container = true;
      DCHECK(!seen_interstitial);
    } else if (child->GetNode()->IsMediaRemotingInterstitial() ||
               child->GetNode()->IsPictureInPictureInterstitial()) {
      // Only one interstitial can be shown at a time.
      seen_interstitial = true;
    } else {
      NOTREACHED();
    }
#endif

    // TODO(mlamouri): we miss some layouts because needsLayout returns false in
    // some cases where we want to change the width of the controls because the
    // visible viewport has changed for example.
    if (new_rect.Size() == old_size && !child->NeedsLayout())
      continue;

    LayoutUnit width = new_rect.Width();
    if (child->GetNode()->IsMediaControls()) {
      width = ComputePanelWidth(new_rect);
    }

    LayoutBox* layout_box = ToLayoutBox(child);
    layout_box->SetLocation(new_rect.Location());
    layout_box->SetOverrideLogicalWidth(width);
    layout_box->SetOverrideLogicalHeight(new_rect.Height());
    // TODO(cbiesinger): Can this just be ForceLayout()?
    layout_box->ForceLayoutWithPaintInvalidation();
  }

  ClearNeedsLayout();
}

bool LayoutMedia::IsChildAllowed(LayoutObject* child,
                                 const ComputedStyle& style) const {
  // Two types of child layout objects are allowed: media controls
  // and the text track container. Filter children by node type.
  DCHECK(child->GetNode());

  // Out-of-flow positioned or floating child breaks layout hierarchy.
  // This check can be removed if ::-webkit-media-controls is made internal.
  if (style.HasOutOfFlowPosition() || style.IsFloating())
    return false;

  // The user agent stylesheet (mediaControls.css) has
  // ::-webkit-media-controls { display: flex; }. If author style
  // sets display: inline we would get an inline layoutObject as a child
  // of replaced content, which is not supposed to be possible. This
  // check can be removed if ::-webkit-media-controls is made
  // internal.
  if (child->GetNode()->IsMediaControls())
    return child->IsFlexibleBoxIncludingNG();

  if (child->GetNode()->IsTextTrackContainer() ||
      child->GetNode()->IsMediaRemotingInterstitial() ||
      child->GetNode()->IsPictureInPictureInterstitial())
    return true;

  return false;
}

void LayoutMedia::PaintReplaced(const PaintInfo&,
                                const PhysicalOffset& paint_offset) const {}

LayoutUnit LayoutMedia::ComputePanelWidth(const LayoutRect& media_rect) const {
  // TODO(mlamouri): we don't know if the main frame has an horizontal scrollbar
  // if it is out of process. See https://crbug.com/662480
  if (GetDocument().GetPage()->MainFrame()->IsRemoteFrame())
    return media_rect.Width();

  // TODO(foolip): when going fullscreen, the animation sometimes does not clear
  // up properly and the last `absoluteXOffset` received is incorrect. This is
  // a shortcut that we could ideally avoid. See https://crbug.com/663680
  if (MediaElement() && MediaElement()->IsFullscreen())
    return media_rect.Width();

  Page* page = GetDocument().GetPage();
  LocalFrame* main_frame = page->DeprecatedLocalMainFrame();
  LocalFrameView* page_view = main_frame ? main_frame->View() : nullptr;
  if (!main_frame || !page_view || !page_view->GetLayoutView())
    return media_rect.Width();

  // If the main frame can have a scrollbar, we'll never be cut off.
  // TODO(crbug.com/771379): Once we no longer assume that the video is in the
  // main frame for the visibility calculation below, we will only care about
  // the video's frame's scrollbar check below.
  ScrollbarMode h_mode, v_mode;
  page_view->GetLayoutView()->CalculateScrollbarModes(h_mode, v_mode);
  if (h_mode != ScrollbarMode::kAlwaysOff)
    return media_rect.Width();

  // If the video's frame (can be different from main frame if video is in an
  // iframe) can have a scrollbar, we'll never be cut off.
  LocalFrame* media_frame = GetFrame();
  LocalFrameView* media_page_view = media_frame ? media_frame->View() : nullptr;
  if (media_page_view && media_page_view->GetLayoutView()) {
    media_page_view->GetLayoutView()->CalculateScrollbarModes(h_mode, v_mode);
    if (h_mode != ScrollbarMode::kAlwaysOff)
      return media_rect.Width();
  }

  // TODO(crbug.com/771379): This code assumes the video is in the main frame.
  // On desktop, this will include scrollbars when they stay visible.
  const LayoutUnit visible_width(page->GetVisualViewport().VisibleWidth());
  // The bottom left corner of the video.
  const FloatPoint bottom_left_point(
      LocalToAbsoluteFloatPoint(FloatPoint(media_rect.X(), media_rect.MaxY()),
                                kTraverseDocumentBoundaries));
  // The bottom right corner of the video.
  const FloatPoint bottom_right_point(LocalToAbsoluteFloatPoint(
      FloatPoint(media_rect.MaxX(), media_rect.MaxY()),
      kTraverseDocumentBoundaries));

  const bool bottom_left_corner_visible = bottom_left_point.X() < visible_width;
  const bool bottom_right_corner_visible =
      bottom_right_point.X() < visible_width;

  // If both corners are visible, then we can see the whole panel.
  if (bottom_left_corner_visible && bottom_right_corner_visible)
    return media_rect.Width();

  // TODO(crbug.com/771379): Should we return zero here?
  // If neither corner is visible, use the whole length.
  if (!bottom_left_corner_visible && !bottom_right_corner_visible)
    return media_rect.Width();

  // TODO(crbug.com/771379): Right now, LayoutMedia will assume that the panel
  // will start at the bottom left corner, so if the bottom right corner is
  // showing, we'll need to set the panel width to the width of the video.
  // However, in an ideal world, if the bottom right corner is showing and the
  // bottom left corner is not, we'd shorten the panel *and* shift it towards
  // the bottom right corner (this can happen when the video has been rotated).
  if (bottom_right_corner_visible)
    return media_rect.Width();

  // One corner is within the visible viewport, while the other is outside of
  // it, so we know that the panel will cross the right edge of the page, so
  // we'll calculate the point where the panel intersects the right edge of the
  // page and then calculate the visible width of the panel from the distance
  // between the visible point and the edge intersection point.
  const float slope = (bottom_right_point.Y() - bottom_left_point.Y()) /
                      (bottom_right_point.X() - bottom_left_point.X());
  const float edge_intersection_y =
      bottom_left_point.Y() + ((visible_width - bottom_left_point.X()) * slope);

  const FloatPoint edge_intersection_point(visible_width, edge_intersection_y);

  // Calculate difference.
  FloatPoint difference_vector = edge_intersection_point;
  difference_vector.Move(-bottom_left_point.X(), -bottom_left_point.Y());

  return LayoutUnit(difference_vector.length());
}

}  // namespace blink
