// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/paint_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "pdf/paint_manager_simd.h"
#include "pdf/paint_ready_rect.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/core/SkSamplingOptions.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace chrome_pdf {
namespace {

void SkiaBufferReleaseProc(const void* data, void* ctx) {
  std::unique_ptr<PaintManager::BufferData> buffer_data(
      static_cast<PaintManager::BufferData*>(ctx));
  CHECK_EQ(buffer_data->allocation.data(), data);

  if (base::SingleThreadTaskRunner::GetMainThreadDefault()
          ->RunsTasksInCurrentSequence()) {
    // Happens when a buffer gets returned in the destructor of cc::PaintImage
    // on UpdateSnapshot. Otherwise it gets returned at some unknowable point in
    // the future from the Compositor thread.
    if (buffer_data->owner) {
      buffer_data->owner->BufferFinishedOnMainThread(std::move(buffer_data));
    }
    return;
  }
  // Happens when the compositor gives us back a buffer some other time.
  base::SingleThreadTaskRunner::GetMainThreadDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PaintManager::BufferFinishedOnMainThread,
                                buffer_data->owner, std::move(buffer_data)));
}

}  // namespace

PaintManager::BufferData::BufferData(size_t size,
                                     base::WeakPtr<PaintManager> owner)
    : allocation(base::HeapArray<uint8_t>::Uninit(size)), owner(owner) {}

PaintManager::BufferData::~BufferData() = default;

PaintManager::PaintManager(Client* client) : client_(client) {
  DCHECK(client_);
}

PaintManager::~PaintManager() {
  buffer_return_weak_factory_.InvalidateWeakPtrsAndDoom();
}

// static
gfx::Size PaintManager::GetNewContextSize(const gfx::Size& current_context_size,
                                          const gfx::Size& plugin_size) {
  // The amount of additional space in pixels to allocate to the right/bottom of
  // the context.
  constexpr int kBufferSize = 50;

  // Default to returning the same size.
  gfx::Size result = current_context_size;

  // The minimum size of the plugin before resizing the context to ensure we
  // aren't wasting too much memory. We deduct twice the kBufferSize from the
  // current context size which gives a threshhold that is kBufferSize below
  // the plugin size when the context size was last computed.
  gfx::Size min_size(
      std::max(current_context_size.width() - 2 * kBufferSize, 0),
      std::max(current_context_size.height() - 2 * kBufferSize, 0));

  // If the plugin size is bigger than the current context size, we need to
  // resize the context. If the plugin size is smaller than the current
  // context size by a given threshhold then resize the context so that we
  // aren't wasting too much memory.
  if (plugin_size.width() > current_context_size.width() ||
      plugin_size.height() > current_context_size.height() ||
      plugin_size.width() < min_size.width() ||
      plugin_size.height() < min_size.height()) {
    // Create a larger context than needed so that if we only resize by a
    // small margin, we don't need a new context.
    result = gfx::Size(plugin_size.width() + kBufferSize,
                       plugin_size.height() + kBufferSize);
  }

  return result;
}

void PaintManager::BufferFinishedOnMainThread(
    std::unique_ptr<BufferData> data) {
  // image_info_ could have changed since posting the task.
  if (data->allocation.size() >= image_info_.computeMinByteSize()) {
    free_buffers_.emplace_back(std::move(data));
  }
}

void PaintManager::SetSize(const gfx::Size& new_size,
                           float device_scale,
                           SkAlphaType alpha_type) {
  if (GetEffectiveSize() == new_size &&
      GetEffectiveDeviceScale() == device_scale &&
      image_info_.alphaType() == alpha_type) {
    return;
  }

  gfx::Size old_image_size = gfx::SkISizeToSize(image_info_.dimensions());
  gfx::Size padded_new_size = GetNewContextSize(old_image_size, new_size);
  static_assert(SkColorType::kN32_SkColorType ==
                SkColorType::kBGRA_8888_SkColorType);

  plugin_size_ = new_size;
  device_scale_ = device_scale;

  if (old_image_size != padded_new_size ||
      image_info_.alphaType() != alpha_type || !engine_buffer_) {
    image_info_ = SkImageInfo::MakeN32(padded_new_size.width(),
                                       padded_new_size.height(), alpha_type);
    weak_factory_.InvalidateWeakPtrs();
    manual_callback_pending_ = false;

    // Free the old `engine_buffer_` if it exists. If the old buffer fits the
    // new data, this is a no-op, as the next GetBuffer() call will return the
    // same buffer.
    if (engine_buffer_) {
      BufferFinishedOnMainThread(std::move(engine_buffer_));
    }
    if (draw_buffer_) {
      BufferFinishedOnMainThread(std::move(draw_buffer_));
    }
    surface_.reset();
    engine_buffer_ = GetBuffer();
    engine_bitmap_ =
        client_->InstallBuffer(image_info_, engine_buffer_->allocation);

    previous_frame_.reset();
    flush_pending_ = false;
  }

  client_->UpdateScale(1.0f / device_scale_);

  view_size_changed_waiting_for_paint_ = true;
  aggregator_.InvalidateRect(gfx::Rect(new_size));
  EnsureCallbackPending();
}

void PaintManager::SetTransform(float scale,
                                const gfx::Point& origin,
                                const gfx::Vector2d& translate) {
  CHECK_GT(scale, 0.0f);
  // translate_with_origin = origin - scale * origin - translate
  gfx::Vector2dF translate_with_origin = origin.OffsetFromOrigin();
  translate_with_origin.Scale(1.0f - scale);
  translate_with_origin.Subtract(translate);

  // TODO(crbug.com/40203030): Should update be deferred until `Flush()`?
  client_->UpdateLayerTransform(scale, translate_with_origin);
}

void PaintManager::ClearTransform() {
  SetTransform(1.f, gfx::Point(), gfx::Vector2d());
}

void PaintManager::Invalidate() {
  EnsureCallbackPending();
  aggregator_.InvalidateRect(gfx::Rect(GetEffectiveSize()));
}

void PaintManager::InvalidateRect(const gfx::Rect& rect) {
  DCHECK(!in_paint_);

  // Clip the rect to the device area.
  gfx::Rect clipped_rect =
      gfx::IntersectRects(rect, gfx::Rect(GetEffectiveSize()));
  if (clipped_rect.IsEmpty()) {
    return;  // Nothing to do.
  }

  EnsureCallbackPending();
  aggregator_.InvalidateRect(clipped_rect);
}

void PaintManager::ScrollRect(const gfx::Rect& clip_rect,
                              const gfx::Vector2d& amount) {
  DCHECK(!in_paint_);

  EnsureCallbackPending();

  aggregator_.ScrollRect(clip_rect, amount);
}

gfx::Size PaintManager::GetEffectiveSize() const {
  return plugin_size_;
}

float PaintManager::GetEffectiveDeviceScale() const {
  return device_scale_;
}

void PaintManager::EnsureCallbackPending() {
  // The best way for us to do the next update is to get a notification that
  // a previous one has completed. So if we're already waiting for one, we
  // don't have to do anything differently now.
  if (flush_pending_) {
    return;
  }

  // If no flush is pending, we need to do a manual call to get back to the
  // main thread. We may have one already pending, or we may need to schedule.
  if (manual_callback_pending_) {
    return;
  }

  base::SingleThreadTaskRunner::GetMainThreadDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PaintManager::OnManualCallbackComplete,
                                weak_factory_.GetWeakPtr()));
  manual_callback_pending_ = true;
}

std::unique_ptr<PaintManager::BufferData> PaintManager::GetBuffer() {
  while (free_buffers_.size()) {
    std::unique_ptr<BufferData> free_buf = std::move(free_buffers_.back());
    free_buffers_.pop_back();
    if (free_buf->allocation.size() >= image_info_.computeMinByteSize()) {
      return free_buf;
    }
  }
  // There was no free buffer large enough to fit the current image.
  //
  // We shouldn't use a smarter container, because skia owns this memory for
  // most of its lifetime, and skia deals in void*. The freeing of this memory
  // happens in BufferFinishedOnMainThread() if it is too small for our current
  // viewport, or if we vanish into the void, or, in the case of a future
  // resize, at the start of DoPaint().
  return std::make_unique<BufferData>(image_info_.computeMinByteSize(),
                                      buffer_return_weak_factory_.GetWeakPtr());
}

void PaintManager::DoPaint() {
  base::AutoReset<bool> auto_reset_in_paint(&in_paint_, true);

  DCHECK(aggregator_.HasPendingUpdate());

  // We may have entered the client and not drawn anything previously and
  // never flushed the frame. We can reuse that frame, if that is the case,
  // except if a resize occurred.
  if (!draw_buffer_) {
    draw_buffer_ = GetBuffer();
    CHECK(draw_buffer_);

    // TODO(crbug.com/40222665): Can we guarantee repainting some other way?
    client_->InvalidatePluginContainer();

    surface_ =
        SkSurfaces::WrapPixels(image_info_, draw_buffer_->allocation.data(),
                               image_info_.minRowBytes());
  }

  PaintAggregator::PaintUpdate update = aggregator_.GetPendingUpdate();

  // Set a timer here to check how long it takes to do all the paint operations
  // below, except for the flush.
  const base::TimeTicks begin_time = base::TimeTicks::Now();

  std::vector<PaintReadyRect> ready_rects;
  std::vector<gfx::Rect> pending_rects;
  client_->OnPaint(update.paint_rects, ready_rects, pending_rects);

  if (ready_rects.empty() && pending_rects.empty()) {
    return;  // Nothing was painted, don't schedule a flush.
  }

  SkIRect bottom_buffer_area = SkIRect::MakeLTRB(
      0, plugin_size_.height(), image_info_.width(), image_info_.height());
  SkIRect right_buffer_area = SkIRect::MakeLTRB(
      plugin_size_.width(), 0, image_info_.width(), plugin_size_.height());

  // Blank the buffer area because Skia might read it during compositing.
  SkPaint paint;
  paint.setColor(SK_ColorTRANSPARENT);
  paint.setBlendMode(SkBlendMode::kSrc);
  surface_->getCanvas()->drawRect(SkRect::Make(bottom_buffer_area), paint);
  surface_->getCanvas()->drawRect(SkRect::Make(right_buffer_area), paint);

  // Draw the parts of the frame which weren't updated from the previous one.
  if (previous_frame_.has_value()) {
    std::vector<SkIRect> all_draws;
    for (auto& ready_rect : ready_rects) {
      all_draws.push_back(gfx::RectToSkIRect(ready_rect.rect()));
    }
    // Do not include the padding area (see GetNewContextSize()), which hasn't
    // been drawn into.
    all_draws.push_back(right_buffer_area);
    all_draws.push_back(bottom_buffer_area);

    SkRegion draw_region;
    draw_region.setRects(all_draws.data(), all_draws.size());

    surface_->getCanvas()->clipRegion(draw_region, SkClipOp::kDifference);
    surface_->getCanvas()->drawImage(*previous_frame_, 0, 0);
    surface_->getCanvas()->restore();
  }
  surface_->notifyContentWillChange(SkSurface::kDiscard_ContentChangeMode);

  std::vector<PaintReadyRect> ready_now;
  if (pending_rects.empty()) {
    aggregator_.SetIntermediateResults(std::move(ready_rects),
                                       std::move(pending_rects));
    ready_now = aggregator_.TakeReadyRects();
    aggregator_.ClearPendingUpdate();

    // First, apply any scroll amount less than the surface's size.
    if (update.has_scroll &&
        std::abs(update.scroll_delta.x()) < surface_->width() &&
        std::abs(update.scroll_delta.y()) < surface_->height()) {
      if (previous_frame_.has_value()) {
        // Draw the scroll if there is one via a rect offset from the previous
        // frame.
        SkRect destination_rect = gfx::RectToSkRect(gfx::IntersectRects(
            (update.scroll_rect + update.scroll_delta), update.scroll_rect));
        SkRect source_rect = gfx::RectToSkRect(gfx::IntersectRects(
            (update.scroll_rect - update.scroll_delta), update.scroll_rect));
        surface_->getCanvas()->drawImageRect(
            *previous_frame_, source_rect, destination_rect,
            SkSamplingOptions(), nullptr, SkCanvas::kFast_SrcRectConstraint);
      }
    }

    view_size_changed_waiting_for_paint_ = false;
  } else {
    std::vector<PaintReadyRect> ready_later;
    for (auto& ready_rect : ready_rects) {
      // Don't flush any part (i.e. scrollbars) if we're resizing the browser,
      // as that'll lead to flashes.  Until we flush, the browser will use the
      // previous image, but if we flush, it'll revert to using the blank image.
      // We make an exception for the first paint since we want to show the
      // default background color instead of the pepper default of black.
      if (ready_rect.flush_now() &&
          (!view_size_changed_waiting_for_paint_ || first_paint_)) {
        ready_now.push_back(std::move(ready_rect));
      } else {
        ready_later.push_back(std::move(ready_rect));
      }
    }
    // Take the rectangles, except the ones that need to be flushed right away,
    // and save them so that everything is flushed at once.
    aggregator_.SetIntermediateResults(std::move(ready_later),
                                       std::move(pending_rects));

    if (ready_now.empty()) {
      EnsureCallbackPending();
      return;
    }
  }

  CHECK(draw_buffer_);
  CHECK(engine_buffer_);
  for (const auto& ready_rect : ready_now) {
    SkRect skia_rect = gfx::RectToSkRect(ready_rect.rect());
    skia_rect.intersect(SkRect::MakeWH(surface_->width(), surface_->height()));

    // Copy (possibly with blending into the white page background) the memory
    // from the engine bitmap into the surfact pixmap, making sure to do it
    // line by line because GetNewContextSize() adds padding (which leads to
    // them having a different rowBytes() value).
    //
    // SAFETY: We do a bounds check before performing the copy. We are forced
    // into doing this because pdfium directly modifies the pixmap rather than
    // using skia as outlined below. These objects share an `image_info_`, and
    // so no color space translation needs to be done, though blending with
    // white may be required, depending.
    //
    // There is no Skia-supported way to copy directly a subset of a pixmap to
    // another pixmap or from a pixmap to a canvas. For some reason,
    // drawImageRect() (or a similar copy-pixels-for-a-subset-of-the-frame
    // procedure) is only implemented for `SkCanvas`es, and we can't draw to
    // an SkCanvas without using an SkImage, which isn't possible, since the
    // engine's pixmap is mutable and SkImages are immutable, and so this
    // would require making a new copy of the engine bitmap every frame,
    // something we can't do for performance reasons.
    SkPixmap draw_pixmap;
    CHECK(surface_->getCanvas()->peekPixels(&draw_pixmap));
    // Should be some kind of BGRx. The blending algorithm implemented below
    // will not work if the alpha channel isn't in the MSB or if there are
    // more than 8 bits per channel.
    CHECK_EQ(image_info_.bytesPerPixel(), 4);
    size_t copy_size = skia_rect.width() * image_info_.bytesPerPixel();
    for (int row_i = 0; row_i < skia_rect.height(); ++row_i) {
      size_t curr_engine_offset = image_info_.computeOffset(
          skia_rect.x(), skia_rect.y() + row_i, engine_bitmap_->rowBytes());
      size_t curr_draw_offset = image_info_.computeOffset(
          skia_rect.x(), skia_rect.y() + row_i, draw_pixmap.rowBytes());
      UNSAFE_BUFFERS({
        uint8_t* src_ptr =
            engine_buffer_->allocation.data() + curr_engine_offset;
        uint8_t* dest_ptr = draw_buffer_->allocation.data() + curr_draw_offset;
        CHECK_GE(src_ptr, engine_buffer_->allocation.data());
        CHECK_LE(src_ptr + copy_size, engine_buffer_->allocation.data() +
                                          engine_buffer_->allocation.size());
        CHECK_GE(dest_ptr, draw_buffer_->allocation.data());
        CHECK_LE(dest_ptr + copy_size, draw_buffer_->allocation.data() +
                                           draw_buffer_->allocation.size());

        // Depending on the alpha type of the image, it may need to be
        // blended with white. There is no blending with previous frames.
        switch (image_info_.alphaType()) {
          case kOpaque_SkAlphaType:
            memcpy(dest_ptr, src_ptr, copy_size);
            break;

          case kUnpremul_SkAlphaType:
            NonPremulBlend(src_ptr, dest_ptr, skia_rect.width());
            break;

          case kPremul_SkAlphaType:
            PremulBlend(src_ptr, dest_ptr, skia_rect.width());
            break;

          default:
            NOTREACHED();
        }
      });
    }
  }

  base::UmaHistogramMediumTimes("PDF.RenderAndPaintTime",
                                base::TimeTicks::Now() - begin_time);

  // release() the pointer to allow skia to own the memory rather than the
  // PaintManager, which could get destroyed before Skia is done with it. The
  // buffer is returned in SkiaBufferReleaseProc().
  BufferData* buf = draw_buffer_.release();
  sk_sp<SkData> flushing_buffer =
      SkData::MakeWithProc(buf->allocation.data(), buf->allocation.size(),
                           SkiaBufferReleaseProc, buf);

  draw_buffer_ = nullptr;

  Flush(std::move(flushing_buffer));

  base::UmaHistogramMediumTimes("PDF.RenderPaintAndFlushTime",
                                base::TimeTicks::Now() - begin_time);

  first_paint_ = false;
}

void PaintManager::Flush(sk_sp<SkData> flushing_buffer) {
  previous_frame_.emplace(SkImages::RasterFromData(
      image_info_, std::move(flushing_buffer), image_info_.minRowBytes()));
  client_->UpdateSnapshot(*previous_frame_);

  flush_pending_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PaintManager::OnFlushComplete,
                                weak_factory_.GetWeakPtr()));
}

void PaintManager::OnFlushComplete() {
  DCHECK(flush_pending_);
  flush_pending_ = false;

  // If more paints were enqueued while we were waiting for the flush to
  // complete, execute them now.
  if (aggregator_.HasPendingUpdate()) {
    DoPaint();
  }
}

void PaintManager::OnManualCallbackComplete() {
  DCHECK(manual_callback_pending_);
  manual_callback_pending_ = false;

  // Just because we have a manual callback doesn't mean there are actually any
  // invalid regions. Even though we only schedule this callback when something
  // is pending, a Flush callback could have come in before this callback was
  // executed and that could have cleared the queue.
  if (aggregator_.HasPendingUpdate()) {
    DoPaint();
  }
}

}  // namespace chrome_pdf
