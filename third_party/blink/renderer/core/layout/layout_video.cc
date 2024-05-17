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
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/video_painter.h"

namespace blink {

namespace {

const float kInitEffectZoom = 1.0f;

}  // namespace

LayoutVideo::LayoutVideo(HTMLVideoElement* video) : LayoutMedia(video) {
  SetIntrinsicSize(CalculateIntrinsicSize(kInitEffectZoom));
}

LayoutVideo::~LayoutVideo() = default;

PhysicalSize LayoutVideo::DefaultSize() {
  return PhysicalSize(LayoutUnit(kDefaultWidth), LayoutUnit(kDefaultHeight));
}

void LayoutVideo::IntrinsicSizeChanged() {
  NOT_DESTROYED();
  if (VideoElement()->IsShowPosterFlagSet())
    LayoutMedia::IntrinsicSizeChanged();
  UpdateIntrinsicSize();
}

void LayoutVideo::UpdateIntrinsicSize() {
  NOT_DESTROYED();

  PhysicalSize size = CalculateIntrinsicSize(StyleRef().EffectiveZoom());

  // Never set the element size to zero when in a media document.
  if (size.IsEmpty() && GetNode()->ownerDocument() &&
      GetNode()->ownerDocument()->IsMediaDocument())
    return;

  if (size == IntrinsicSize())
    return;

  SetIntrinsicSize(size);
  SetIntrinsicLogicalWidthsDirty();
  SetNeedsLayoutAndFullPaintInvalidation(
      layout_invalidation_reason::kSizeChanged);
}

PhysicalSize LayoutVideo::CalculateIntrinsicSize(float scale) {
  NOT_DESTROYED();
  HTMLVideoElement* video = VideoElement();
  DCHECK(video);

  auto display_mode = GetDisplayMode();

  // Special case: If the poster image is the "default poster image", we should
  // NOT use that for calculating intrinsic size.
  // TODO(1190335): Remove this once default poster image is removed
  if (display_mode == kPoster && video->IsDefaultPosterImageURL()) {
    display_mode = kVideo;
  }

  switch (display_mode) {
    // This implements the intrinsic width/height calculation from:
    // https://html.spec.whatwg.org/#the-video-element:dimension-attributes:~:text=The%20intrinsic%20width%20of%20a%20video%20element's%20playback%20area
    // If the video playback area is currently represented by the poster image,
    // the intrinsic width and height are that of the poster image.
    case kPoster:
      if (!cached_image_size_.IsEmpty() && !ImageResource()->ErrorOccurred()) {
        return cached_image_size_;
      }
      break;

    // Otherwise, the intrinsic width is that of the video.
    case kVideo:
      if (const auto* player = MediaElement()->GetWebMediaPlayer()) {
        gfx::Size size = player->NaturalSize();
        if (!size.IsEmpty()) {
          PhysicalSize layout_size = PhysicalSize(size);
          layout_size.Scale(scale);
          return layout_size;
        }
      }
      break;
  }

  PhysicalSize size = DefaultSize();
  size.Scale(scale);
  return size;
}

void LayoutVideo::ImageChanged(WrappedImagePtr new_image,
                               CanDeferInvalidation defer) {
  NOT_DESTROYED();
  LayoutMedia::ImageChanged(new_image, defer);

  // Cache the image intrinsic size so we can continue to use it to draw the
  // image correctly even if we know the video intrinsic size but aren't able to
  // draw video frames yet (we don't want to scale the poster to the video size
  // without keeping aspect ratio). We do not need to check
  // |ShouldDisplayPosterImage| because the image can be ready before we find
  // out we actually need it.
  cached_image_size_ = IntrinsicSize();

  // The intrinsic size is now that of the image, but in case we already had the
  // intrinsic size of the video we call this here to restore the video size.
  UpdateIntrinsicSize();
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

void LayoutVideo::UpdateFromElement() {
  NOT_DESTROYED();
  LayoutMedia::UpdateFromElement();
  InvalidateCompositing();
  UpdateIntrinsicSize();
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
  if (GetDisplayMode() == kVideo) {
    // Video codecs may need to restart from an I-frame when the output is
    // resized. Round size in advance to avoid 1px snap difference.
    return PreSnappedRectForPersistentSizing(
        ComputeReplacedContentRect(base_content_rect));
  }
  // If we are displaying the poster image no pre-rounding is needed, but the
  // size of the image should be used for fitting instead.
  return ComputeReplacedContentRect(base_content_rect, &cached_image_size_);
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
