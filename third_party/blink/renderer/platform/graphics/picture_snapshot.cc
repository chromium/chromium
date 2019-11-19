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

#include <memory>
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/logging_canvas.h"
#include "third_party/blink/renderer/platform/graphics/profiling_canvas.h"
#include "third_party/blink/renderer/platform/graphics/replaying_canvas.h"
#include "third_party/blink/renderer/platform/graphics/skia/image_pixel_locker.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/image_frame.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"

namespace blink {

PictureSnapshot::PictureSnapshot(sk_sp<const SkPicture> picture)
    : picture_(std::move(picture)) {}

scoped_refptr<PictureSnapshot> PictureSnapshot::Load(
    const Vector<scoped_refptr<TilePictureStream>>& tiles) {
  DCHECK(!tiles.IsEmpty());
  Vector<sk_sp<SkPicture>> pictures;
  pictures.ReserveCapacity(tiles.size());
  FloatRect union_rect;
  for (const auto& tile_stream : tiles) {
    sk_sp<SkPicture> picture = std::move(tile_stream->picture);
    if (!picture)
      return nullptr;
    FloatRect cull_rect(picture->cullRect());
    cull_rect.MoveBy(tile_stream->layer_offset);
    union_rect.Unite(cull_rect);
    pictures.push_back(std::move(picture));
  }
  if (tiles.size() == 1)
    return base::AdoptRef(new PictureSnapshot(std::move(pictures[0])));
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(union_rect.Width(),
                                             union_rect.Height(), nullptr, 0);
  for (size_t i = 0; i < pictures.size(); ++i) {
    canvas->save();
    canvas->translate(tiles[i]->layer_offset.X() - union_rect.X(),
                      tiles[i]->layer_offset.Y() - union_rect.Y());
    pictures[i]->playback(canvas, nullptr);
    canvas->restore();
  }
  return base::AdoptRef(
      new PictureSnapshot(recorder.finishRecordingAsPicture()));
}

bool PictureSnapshot::IsEmpty() const {
  return picture_->cullRect().isEmpty();
}

Vector<uint8_t> PictureSnapshot::Replay(unsigned from_step,
                                        unsigned to_step,
                                        double scale) const {
  const SkIRect bounds = picture_->cullRect().roundOut();
  int width = ceil(scale * bounds.width());
  int height = ceil(scale * bounds.height());

  // TODO(fmalita): convert this to SkSurface/SkImage, drop the intermediate
  // SkBitmap.
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(width, height));
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
  bool peekResult = bitmap.peekPixels(&src);
  DCHECK(peekResult);

  SkPngEncoder::Options options;
  options.fFilterFlags = SkPngEncoder::FilterFlag::kSub;
  options.fZLibLevel = 3;
  if (!ImageEncoder::Encode(&encoded_image, src, options))
    return Vector<uint8_t>();

  return encoded_image;
}

Vector<Vector<base::TimeDelta>> PictureSnapshot::Profile(
    unsigned min_repeat_count,
    base::TimeDelta min_duration,
    const FloatRect* clip_rect) const {
  Vector<Vector<base::TimeDelta>> timings;
  timings.ReserveInitialCapacity(min_repeat_count);
  const SkIRect bounds = picture_->cullRect().roundOut();
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(bounds.width(), bounds.height()));
  bitmap.eraseARGB(0, 0, 0, 0);

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks stop_time = now + min_duration;
  for (unsigned step = 0; step < min_repeat_count || now < stop_time; ++step) {
    Vector<base::TimeDelta> current_timings;
    if (!timings.IsEmpty())
      current_timings.ReserveInitialCapacity(timings.front().size());
    ProfilingCanvas canvas(bitmap);
    if (clip_rect) {
      canvas.clipRect(SkRect::MakeXYWH(clip_rect->X(), clip_rect->Y(),
                                       clip_rect->Width(),
                                       clip_rect->Height()));
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
