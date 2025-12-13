/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc.  All rights reserved.
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

#include "third_party/blink/renderer/core/layout/layout_video.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/video_painter.h"

namespace blink {

LayoutVideo::LayoutVideo(HTMLVideoElement* video) : LayoutMedia(video) {}

LayoutVideo::~LayoutVideo() = default;

void LayoutVideo::NaturalSizeChanged() {
  NOT_DESTROYED();
  if (VideoElement()->IsShowPosterFlagSet())
    LayoutMedia::NaturalSizeChanged();
  UpdateNaturalSize();
}

void LayoutVideo::UpdateNaturalSize() {
  NOT_DESTROYED();
  const PhysicalNaturalSizingInfo sizing_info = GetNaturalDimensions();

  // Never set the element size to zero when in a media document.
  if (sizing_info.size.IsEmpty() && GetDocument().IsMediaDocument()) {
    return;
  }
  if (sizing_info == natural_dimensions_) {
    return;
  }
  natural_dimensions_ = sizing_info;

  SetIntrinsicLogicalWidthsDirty();
  SetNeedsLayoutAndFullPaintInvalidation(
      layout_invalidation_reason::kSizeChanged);
}

PhysicalNaturalSizingInfo LayoutVideo::GetNaturalDimensions() const {
  NOT_DESTROYED();

  auto display_mode = GetDisplayMode();
  const auto* video = VideoElement();

  // Special case: If the poster image is the "default poster image", we should
  // NOT use that for calculating natural dimensions.
  // TODO: crbug.com/40174114 - Remove this once default poster image is
  // removed.
  if (display_mode == kPoster && video->IsDefaultPosterImageURL()) {
    display_mode = kVideo;
  }

  // This implements the intrinsic width/height calculation from:
  // https://html.spec.whatwg.org/#the-video-element:dimension-attributes:~:text=The%20intrinsic%20width%20of%20a%20video%20element's%20playback%20area
  switch (display_mode) {
    case kPoster:
      // If the video playback area is currently represented by the poster
      // image, the natural dimensions are that of the poster image.
      if (!ImageResource()->ErrorOccurred()) {
        return LayoutImage::GetNaturalDimensions();
      }
      break;
    case kVideo:
      // Otherwise, the natural dimensions are that of the video.
      if (const auto* player = video->GetWebMediaPlayer()) {
        gfx::Size video_size = player->NaturalSize();
        if (!video_size.IsEmpty()) {
          PhysicalSize natural_size(video_size);
          natural_size.Scale(StyleRef().EffectiveZoom());
          return PhysicalNaturalSizingInfo::MakeFixed(natural_size);
        }
      }
      break;
  }

  return PhysicalNaturalSizingInfo::None();
}

void LayoutVideo::ImageChanged(WrappedImagePtr new_image,
                               CanDeferInvalidation defer) {
  NOT_DESTROYED();
  LayoutMedia::ImageChanged(new_image, defer);
  UpdateNaturalSize();
}

LayoutVideo::DisplayMode LayoutVideo::GetDisplayMode() const {
  NOT_DESTROYED();

  const auto* video = VideoElement();
  // If the show-poster-flag is set (or there is no video frame to display) AND
  // there is a poster image, display that.
  if ((video->IsShowPosterFlagSet() || !video->HasAvailableVideoFrame()) &&
      !video->PosterImageURL().IsEmpty()) {
    return kPoster;
  }
  // Otherwise, try displaying a video frame.
  else {
    return kVideo;
  }
}

void LayoutVideo::PaintReplaced(const PaintInfo& paint_info,
                                const PhysicalOffset& paint_offset) const {
  NOT_DESTROYED();
  if (ChildPaintBlockedByDisplayLock()) {
    return;
  }
  VideoPainter(*this).PaintReplaced(paint_info, paint_offset);
}

void LayoutVideo::UpdateAfterLayout() {
  NOT_DESTROYED();
  LayoutMedia::UpdateAfterLayout();
  InvalidateCompositing();
}

HTMLVideoElement* LayoutVideo::VideoElement() const {
  NOT_DESTROYED();
  return To<HTMLVideoElement>(GetNode());
}

void LayoutVideo::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style,
    const StyleChangeContext& style_change_context) {
  NOT_DESTROYED();
  LayoutImage::StyleDidChange(diff, old_style, style_change_context);
  VideoElement()->StyleDidChange(old_style, StyleRef());
}

void LayoutVideo::UpdateFromElement() {
  NOT_DESTROYED();
  LayoutMedia::UpdateFromElement();
  InvalidateCompositing();
  UpdateNaturalSize();
  SetShouldDoFullPaintInvalidation();
}

void LayoutVideo::InvalidateCompositing() {
  NOT_DESTROYED();
  WebMediaPlayer* media_player = MediaElement()->GetWebMediaPlayer();
  if (!media_player)
    return;

  if (!VideoElement()->InActiveDocument())
    return;

  VideoElement()->SetNeedsCompositingUpdate();
  if (HasLayer())
    Layer()->SetNeedsCompositingInputsUpdate();
}

PhysicalRect LayoutVideo::ReplacedContentRectFrom(
    const PhysicalRect& base_content_rect) const {
  NOT_DESTROYED();
  PhysicalRect replaced_content_rect =
      LayoutMedia::ReplacedContentRectFrom(base_content_rect);
  if (GetDisplayMode() == kVideo) {
    // Video codecs may need to restart from an I-frame when the output is
    // resized. Round size in advance to avoid 1px snap difference.
    replaced_content_rect =
        PreSnappedRectForPersistentSizing(replaced_content_rect);
  } else {
    // If we are displaying the poster image no pre-rounding is needed, but the
    // size of the image should be used for fitting instead.
  }
  return replaced_content_rect;
}

bool LayoutVideo::SupportsAcceleratedRendering() const {
  NOT_DESTROYED();
  return !!MediaElement()->CcLayer();
}

CompositingReasons LayoutVideo::AdditionalCompositingReasons() const {
  NOT_DESTROYED();
  if (GetDisplayMode() == kVideo && SupportsAcceleratedRendering())
    return CompositingReason::kVideo;

  return CompositingReason::kNone;
}

}  // namespace blink
