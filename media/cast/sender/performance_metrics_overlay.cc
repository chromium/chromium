// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/performance_metrics_overlay.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "ui/gfx/geometry/rect.h"

namespace media::cast {

namespace {

constexpr int kScale = 4;             // Physical pixels per one logical pixel.
constexpr int kCharacterWidth = 3;    // Logical pixel width of one character.
constexpr int kCharacterHeight = 5;   // Logical pixel height of one character.
constexpr int kCharacterSpacing = 1;  // Logical pixels between each character.
constexpr int kLineSpacing = 2;  // Logical pixels between each line of chars.

// The total height of each line of characters.
constexpr int kLineHeight = (kCharacterHeight + kLineSpacing) * kScale;

// Y-plane in YUV formats.
constexpr int kPlane = 0;

// Total number of pixels per character.
constexpr int kPixelsPerChar = (kCharacterWidth + kCharacterSpacing) * kScale;

// A map from an ASCII character to the set of rectangles corresponding to how
// it should be rendered on the frame.
const auto& GetCharacterRenderMap() {
  static const base::NoDestructor<base::flat_map<char, std::vector<gfx::Rect>>>
      kCharacterRenderMap({
          {'!', {gfx::Rect(1, 0, 1, 3), gfx::Rect(1, 4, 1, 1)}},
          {'%',
           {gfx::Rect(0, 0, 1, 1), gfx::Rect(2, 1, 1, 1), gfx::Rect(1, 2, 1, 1),
            gfx::Rect(0, 3, 1, 1), gfx::Rect(2, 4, 1, 1)}},
          {'+',
           {gfx::Rect(1, 1, 1, 1), gfx::Rect(1, 3, 1, 1),
            gfx::Rect(0, 2, 3, 1)}},
          {'-', {gfx::Rect(0, 2, 3, 1)}},
          {'.', {gfx::Rect(1, 4, 1, 1)}},
          {'0',
           {gfx::Rect(0, 0, 3, 1), gfx::Rect(0, 1, 1, 3), gfx::Rect(2, 1, 1, 3),
            gfx::Rect(0, 4, 3, 1)}},
          {'1', {gfx::Rect(1, 0, 1, 5)}},
          {'2',
           {gfx::Rect(0, 0, 3, 1), gfx::Rect(2, 1, 1, 1), gfx::Rect(0, 2, 3, 1),
            gfx::Rect(0, 3, 1, 1), gfx::Rect(0, 4, 3, 1)}},
          {'3',
           {gfx::Rect(0, 0, 3, 1), gfx::Rect(2, 1, 1, 1), gfx::Rect(0, 2, 3, 1),
            gfx::Rect(2, 3, 1, 1), gfx::Rect(0, 4, 3, 1)}},
          {'4',
           {gfx::Rect(0, 0, 1, 2), gfx::Rect(2, 0, 1, 5),
            gfx::Rect(0, 2, 2, 1)}},
          {'5',
           {gfx::Rect(0, 0, 3, 1), gfx::Rect(0, 1, 1, 1), gfx::Rect(0, 2, 3, 1),
            gfx::Rect(2, 3, 1, 1), gfx::Rect(0, 4, 3, 1)}},
          {'6',
           {gfx::Rect(1, 0, 2, 1), gfx::Rect(0, 1, 1, 1), gfx::Rect(0, 2, 3, 1),
            gfx::Rect(0, 3, 1, 1), gfx::Rect(2, 3, 1, 1),
            gfx::Rect(0, 4, 3, 1)}},
          {'7',
           {gfx::Rect(0, 0, 3, 1), gfx::Rect(2, 1, 1, 2),
            gfx::Rect(1, 3, 1, 2)}},
          {'8',
           {gfx::Rect(0, 0, 3, 1), gfx::Rect(0, 1, 1, 1), gfx::Rect(2, 1, 1, 1),
            gfx::Rect(0, 2, 3, 1), gfx::Rect(0, 3, 1, 1), gfx::Rect(2, 3, 1, 1),
            gfx::Rect(0, 4, 3, 1)}},
          {'9',
           {gfx::Rect(0, 0, 3, 1), gfx::Rect(0, 1, 1, 1), gfx::Rect(2, 1, 1, 1),
            gfx::Rect(0, 2, 3, 1), gfx::Rect(2, 3, 1, 1),
            gfx::Rect(0, 4, 2, 1)}},
          {':', {gfx::Rect(1, 1, 1, 1), gfx::Rect(1, 3, 1, 1)}},
          {'e',
           {gfx::Rect(0, 0, 3, 1), gfx::Rect(0, 1, 1, 1), gfx::Rect(0, 2, 2, 1),
            gfx::Rect(0, 3, 1, 1), gfx::Rect(0, 4, 3, 1)}},
          {'x',
           {gfx::Rect(0, 1, 1, 1), gfx::Rect(2, 1, 1, 1), gfx::Rect(1, 2, 1, 1),
            gfx::Rect(0, 3, 1, 1), gfx::Rect(2, 3, 1, 1)}},
      });

  return *kCharacterRenderMap;
}

scoped_refptr<VideoFrame> CopyVideoFrame(scoped_refptr<VideoFrame> source) {
  // Currently Cast only supports I420, and converts NV12 frames before passing
  // them to the VideoSender.
  CHECK_EQ(source->format(), media::PIXEL_FORMAT_I420);

  // Allocate a new frame, identical in configuration to `source`, then copy
  // over all data and metadata.
  const scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
      media::PIXEL_FORMAT_I420, source->coded_size(), source->visible_rect(),
      source->natural_size(), source->timestamp());
  if (!frame) {
    return nullptr;
  }

  // Copy the contents of the VideoFrame over.
  libyuv::I420Copy(source->data(media::VideoFrame::Plane::kY),
                   source->stride(media::VideoFrame::Plane::kY),
                   source->data(media::VideoFrame::Plane::kU),
                   source->stride(media::VideoFrame::Plane::kU),
                   source->data(media::VideoFrame::Plane::kV),
                   source->stride(media::VideoFrame::Plane::kV),
                   frame->writable_data(media::VideoFrame::Plane::kY),
                   frame->stride(media::VideoFrame::Plane::kY),
                   frame->writable_data(media::VideoFrame::Plane::kU),
                   frame->stride(media::VideoFrame::Plane::kU),
                   frame->writable_data(media::VideoFrame::Plane::kV),
                   frame->stride(media::VideoFrame::Plane::kV),
                   source->coded_size().width(), source->coded_size().height());

  frame->metadata().MergeMetadataFrom(source->metadata());

  // Important: After all consumers are done with the frame, copy-back the
  // changed/new metadata to the source frame, as it contains feedback signals
  // that need to propagate back up the video stack. The destruction callback
  // for the `frame` holds a ref-counted reference to the source frame to ensure
  // the source frame has the right metadata before its destruction observers
  // are invoked.
  frame->AddDestructionObserver(base::BindOnce(
      [](const VideoFrameMetadata& sent_frame_metadata,
         scoped_refptr<VideoFrame> source_frame) {
        source_frame->set_metadata(sent_frame_metadata);
      },
      frame->metadata(), std::move(source)));

  return frame;
}

// For each pixel in the `rect` (logical coordinates), either decrease the
// intensity or increase it so that the resulting pixel has a perceivably
// different value than it did before.  `p_ul` is a pointer to the pixel at
// coordinate (0,0) in a single-channel 8bpp bitmap.  `stride` is the number
// of bytes per row in the output bitmap.
void DivergePixels(const gfx::Rect& rect,
                   base::span<uint8_t> p_ul,
                   int stride) {
  CHECK_GT(stride, 0);

  // These constants and heuristics were chosen based on experimenting with a
  // wide variety of content, and converging on a readable result.
  // Intensity value are changed as follows:
  //
  //    [16,63] --> [64,111]   (always a difference of +48)
  //   [64,235] --> [16,187]  (always a difference of -48)
  constexpr int kDivergeDownThreshold = 64;
  constexpr int kDivergeDownAmount = 48;
  constexpr int kDivergeUpAmount = 48;

  const gfx::Rect scaled_rect = gfx::ScaleToRoundedRect(rect, kScale);
  CHECK_GE(scaled_rect.y(), 0);
  CHECK_GE(scaled_rect.x(), 0);

  for (int y = scaled_rect.y(); y < scaled_rect.bottom(); ++y) {
    base::span<uint8_t> p_l = p_ul.subspan(static_cast<size_t>(y * stride));
    for (int x = scaled_rect.x(); x < scaled_rect.right(); ++x) {
      int intensity = p_l[x];
      if (intensity >= kDivergeDownThreshold) {
        intensity = intensity - kDivergeDownAmount;
      } else {
        intensity += kDivergeUpAmount;
      }
      p_l[x] = static_cast<uint8_t>(intensity);
    }
  }
}

// Render `line` into `frame` at physical pixel row `top` and aligned to the
// right edge.  Only number digits and a smattering of punctuation characters
// will be rendered.
void RenderLineOfText(const std::string& line, int top, VideoFrame& frame) {
  // Compute number of physical pixels wide the rendered `line` would be,
  // including padding.
  const int line_width =
      (((kCharacterWidth + kCharacterSpacing) * static_cast<int>(line.size())) +
       kCharacterSpacing) *
      kScale;

  // Determine if any characters would render past the left edge of the frame,
  // and compute the index of the first character to be rendered.
  const size_t first_idx =
      (line_width < frame.visible_rect().width())
          ? 0u
          : static_cast<size_t>(
                ((line_width - frame.visible_rect().width()) / kPixelsPerChar) +
                1);

  // Compute the pointer to the pixel at the upper-left corner of the first
  // character to be rendered.
  const int stride = frame.stride(kPlane);
  base::span<uint8_t> p_ul =
      // Start at the first pixel in the first row...
      frame.GetWritableVisiblePlaneData(kPlane).subspan(
          stride * top
          // ...now move to the right edge of the visible part of the frame...
          + frame.visible_rect().width()
          // ...now move left to where line[0] would be rendered...
          - line_width
          // ...now move right to where line[first_idx] would be rendered.
          + first_idx * kPixelsPerChar);

  // Render each character.
  static const auto& kCharacterRenderMap = GetCharacterRenderMap();
  for (size_t i = first_idx; i < line.size(); ++i) {
    p_ul = p_ul.subspan(static_cast<size_t>(kPixelsPerChar));
    auto it = kCharacterRenderMap.find(base::ToLowerASCII(line[i]));
    if (it != kCharacterRenderMap.end()) {
      for (const auto& rect : it->second) {
        DivergePixels(rect, p_ul, stride);
      }
    }
  }
}

}  // namespace

scoped_refptr<VideoFrame> RenderPerformanceMetricsOverlay(
    base::TimeDelta target_playout_delay,
    bool in_low_latency_mode,
    int target_bitrate,
    int frames_ago,
    double encoder_utilization,
    double lossiness,
    scoped_refptr<VideoFrame> source) {
  if (VideoFrame::PlaneHorizontalBitsPerPixel(source->format(), kPlane) != 8) {
    DLOG(WARNING) << "Cannot render overlay: Plane " << kPlane << " not 8bpp.";
    return source;
  }

  // Can't read from unmappable memory (DmaBuf, CVPixelBuffer).
  if (!source->IsMappable()) {
    DVLOG(2) << "Cannot render overlay: frame uses unmappable memory.";
    return source;
  }

  // Compute the physical pixel top row for the bottom-most line of text.
  int top = source->visible_rect().height() - kLineHeight;
  if (top < 0) {
    // No pixels would change: Return source frame.
    return source;
  }

  // Generally speaking, we unfortunately cannot modify the video frame in place
  // so need to create a copy for modification. Allocate a new frame, identical
  // in configuration to `source` and copy over all data and metadata.
  scoped_refptr<VideoFrame> frame = CopyVideoFrame(source);
  if (!frame) {
    return source;  // Allocation failure: Return source frame.
  }

  // Line 3: Frame duration, resolution, and timestamp.
  int frame_duration_ms = 0;
  int frame_duration_ms_frac = 0;
  if (frame->metadata().frame_duration.has_value()) {
    const int decimilliseconds = base::saturated_cast<int>(
        frame->metadata().frame_duration->InMicroseconds() / 100.0 + 0.5);
    frame_duration_ms = decimilliseconds / 10;
    frame_duration_ms_frac = decimilliseconds % 10;
  }
  base::TimeDelta rem = frame->timestamp();
  const int minutes = rem.InMinutes();
  rem -= base::Minutes(minutes);
  const int seconds = static_cast<int>(rem.InSeconds());
  rem -= base::Seconds(seconds);
  const int hundredth_seconds = static_cast<int>(rem.InMilliseconds() / 10);
  RenderLineOfText(
      base::StringPrintf("%d.%01d %dx%d %d:%02d.%02d", frame_duration_ms,
                         frame_duration_ms_frac, frame->visible_rect().width(),
                         frame->visible_rect().height(), minutes, seconds,
                         hundredth_seconds),
      top, *frame);

  // Move up one line's worth of pixels.
  top -= kLineHeight;
  if (top < 0) {
    return frame;
  }

  // Line 2: Capture duration, target playout delay, low-latency mode, and
  // target bitrate.
  int capture_duration_ms = 0;
  if (frame->metadata().capture_begin_time &&
      frame->metadata().capture_end_time) {
    capture_duration_ms =
        base::saturated_cast<int>((*frame->metadata().capture_end_time -
                                   *frame->metadata().capture_begin_time)
                                      .InMillisecondsF() +
                                  0.5);
  }
  const int target_playout_delay_ms =
      static_cast<int>(target_playout_delay.InMillisecondsF() + 0.5);
  const int target_kbits = target_bitrate / 1000;
  RenderLineOfText(
      base::StringPrintf("%d %4.1d%c %4.1d", capture_duration_ms,
                         target_playout_delay_ms,
                         in_low_latency_mode ? '!' : '.', target_kbits),
      top, *frame);

  // Move up one line's worth of pixels.
  top -= kLineHeight;
  if (top < 0) {
    return frame;
  }

  // Line 1: Recent utilization metrics.
  const int encoder_pct =
      base::saturated_cast<int>(encoder_utilization * 100.0 + 0.5);
  const int lossy_pct = base::saturated_cast<int>(lossiness * 100.0 + 0.5);
  RenderLineOfText(base::StringPrintf("%d %3.1d%% %3.1d%%", frames_ago,
                                      encoder_pct, lossy_pct),
                   top, *frame);

  return frame;
}

}  // namespace media::cast
