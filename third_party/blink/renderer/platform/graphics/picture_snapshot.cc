/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/picture_snapshot.h"

#include <cmath>
#include <memory>

#include "base/numerics/checked_math.h"
#include "base/time/time.h"
#include "skia/ext/font_utils.h"
#include "third_party/blink/renderer/platform/graphics/logging_canvas.h"
#include "third_party/blink/renderer/platform/graphics/profiling_canvas.h"
#include "third_party/blink/renderer/platform/graphics/replaying_canvas.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/image_frame.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

PictureSnapshot::PictureSnapshot(sk_sp<const SkPicture> picture)
    : picture_(std::move(picture)) {}

sk_sp<SkPicture> PictureSnapshot::DeserializePicture(
    base::span<const uint8_t> data) {
  if (data.empty()) {
    return nullptr;
  }
  SkDeserialProcs procs{
      .fTypefaceStreamProc = [](SkStream& stream, void*) -> sk_sp<SkTypeface> {
        // Trace snapshots serialize typefaces with kDontIncludeData. Resolve
        // descriptor-only typefaces via the default font manager and reject
        // any embedded raw font stream data.
        return SkTypeface::MakeDeserialize(
            &stream, skia::DefaultFontMgr(),
            [](sk_sp<const SkData>) -> sk_sp<SkData> { return nullptr; });
      },
      .fAllowSkSL = false,
  };
  return SkPicture::MakeFromData(data.data(), data.size(), &procs);
}

scoped_refptr<PictureSnapshot> PictureSnapshot::Load(
    const Vector<scoped_refptr<TilePictureStream>>& tiles) {
  DCHECK(!tiles.empty());
  Vector<sk_sp<SkPicture>> pictures;
  pictures.reserve(tiles.size());
  gfx::RectF union_rect;
  for (const auto& tile_stream : tiles) {
    sk_sp<SkPicture> picture = std::move(tile_stream->picture);
    // Check the Skia rect directly: SkRectToRectF() clamps a NaN size to zero,
    // which would hide it from the IsFinite() check below.
    if (!picture || !picture->cullRect().isFinite()) {
      return nullptr;
    }
    gfx::RectF cull_rect = gfx::SkRectToRectF(picture->cullRect());
    cull_rect.Offset(tile_stream->layer_offset.OffsetFromOrigin());
    // Rejects non-finite tile offsets and edges that overflow float.
    if (!cull_rect.IsFinite()) {
      return nullptr;
    }
    union_rect.Union(cull_rect);
    pictures.push_back(std::move(picture));
  }
  // Each tile is finite, but their union can still overflow float.
  if (!union_rect.IsFinite()) {
    return nullptr;
  }
  if (tiles.size() == 1) {
    return base::AdoptRef(new PictureSnapshot(std::move(pictures[0])));
  }
  SkPictureRecorder recorder;
  SkCanvas* canvas =
      recorder.beginRecording(union_rect.width(), union_rect.height());
  for (wtf_size_t i = 0; i < pictures.size(); ++i) {
    canvas->save();
    canvas->translate(tiles[i]->layer_offset.x() - union_rect.x(),
                      tiles[i]->layer_offset.y() - union_rect.y());
    pictures[i]->playback(canvas, nullptr);
    canvas->restore();
  }
  return base::AdoptRef(
      new PictureSnapshot(recorder.finishRecordingAsPicture()));
}

bool PictureSnapshot::IsEmpty() const {
  // Also reject rects whose integer bounds would overflow int32 (see
  // SkIRect::isEmpty()).
  return picture_->cullRect().isEmpty() ||
         picture_->cullRect().roundOut().isEmpty();
}

Vector<uint8_t> PictureSnapshot::Replay(unsigned from_step,
                                        unsigned to_step,
                                        double scale) const {
  if (IsEmpty()) {
    return Vector<uint8_t>();
  }
  const SkIRect bounds = picture_->cullRect().roundOut();
  // A NaN, infinite, zero or negative `scale`, or a scaled size that does not
  // fit in an int, all end up as a non-positive size and are rejected below.
  const int width = base::CheckedNumeric<int>(std::ceil(scale * bounds.width()))
                        .ValueOrDefault(0);
  const int height =
      base::CheckedNumeric<int>(std::ceil(scale * bounds.height()))
          .ValueOrDefault(0);
  if (width <= 0 || height <= 0) {
    return Vector<uint8_t>();
  }

  // TODO(fmalita): convert this to SkSurface/SkImage, drop the intermediate
  // SkBitmap.
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(SkImageInfo::MakeN32Premul(width, height))) {
    return Vector<uint8_t>();
  }
  bitmap.eraseARGB(0, 0, 0, 0);
  {
    ReplayingCanvas canvas(bitmap, from_step, to_step);
    // Disable LCD text preemptively, because the picture opacity is unknown.
    // The canonical API involves SkSurface props, but since we're not
    // SkSurface-based at this point (see TODO above) we (ab)use saveLayer for
    // this purpose.
    SkAutoCanvasRestore auto_restore(&canvas, false);
    canvas.saveLayer(nullptr, nullptr);

    canvas.scale(scale, scale);
    canvas.ResetStepCount();
    picture_->playback(&canvas, &canvas);
  }
  Vector<uint8_t> encoded_image;

  SkPixmap src;
  bool peek_result = bitmap.peekPixels(&src);
  DCHECK(peek_result);

  if (!ImageEncoder::Encode(&encoded_image, src,
                            SkPngRustEncoder::CompressionLevel::kLow)) {
    return Vector<uint8_t>();
  }

  return encoded_image;
}

Vector<Vector<base::TimeDelta>> PictureSnapshot::Profile(
    unsigned min_repeat_count,
    base::TimeDelta min_duration,
    const gfx::RectF* clip_rect) const {
  Vector<Vector<base::TimeDelta>> timings;
  if (IsEmpty()) {
    return timings;
  }
  const SkIRect bounds = picture_->cullRect().roundOut();
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(
          SkImageInfo::MakeN32Premul(bounds.width(), bounds.height()))) {
    return timings;
  }
  bitmap.eraseARGB(0, 0, 0, 0);
  timings.ReserveInitialCapacity(min_repeat_count);

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks stop_time = now + min_duration;
  for (unsigned step = 0; step < min_repeat_count || now < stop_time; ++step) {
    Vector<base::TimeDelta> current_timings;
    if (!timings.empty()) {
      current_timings.ReserveInitialCapacity(timings.front().size());
    }
    ProfilingCanvas canvas(bitmap);
    if (clip_rect) {
      canvas.clipRect(SkRect::MakeXYWH(clip_rect->x(), clip_rect->y(),
                                       clip_rect->width(),
                                       clip_rect->height()));
      canvas.ResetStepCount();
    }
    canvas.SetTimings(&current_timings);
    picture_->playback(&canvas);
    timings.push_back(std::move(current_timings));
    now = base::TimeTicks::Now();
  }
  return timings;
}

std::unique_ptr<JSONArray> PictureSnapshot::SnapshotCommandLog() const {
  LoggingCanvas canvas;
  picture_->playback(&canvas);
  return canvas.Log();
}

}  // namespace blink
