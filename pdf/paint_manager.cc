// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/paint_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"

PaintManager::ReadyRect::ReadyRect() = default;

PaintManager::ReadyRect::ReadyRect(const pp::Rect& r,
                                   const pp::ImageData& i,
                                   bool f)
    : rect(r), image_data(i), flush_now(f) {}

PaintManager::ReadyRect::ReadyRect(const ReadyRect& that) = default;

PaintManager::PaintManager(pp::Instance* instance,
                           Client* client,
                           bool is_always_opaque)
    : instance_(instance),
      client_(client),
      is_always_opaque_(is_always_opaque),
      callback_factory_(nullptr),
      manual_callback_pending_(false),
      flush_pending_(false),
      flush_requested_(false),
      has_pending_resize_(false),
      graphics_need_to_be_bound_(false),
      pending_device_scale_(1.0),
      device_scale_(1.0),
      in_paint_(false),
      first_paint_(true),
      view_size_changed_waiting_for_paint_(false) {
  // Set the callback object outside of the initializer list to avoid a
  // compiler warning about using "this" in an initializer list.
  callback_factory_.Initialize(this);

  // You can not use a NULL client pointer.
  DCHECK(client);
}

PaintManager::~PaintManager() = default;

// static
pp::Size PaintManager::GetNewContextSize(const pp::Size& current_context_size,
                                         const pp::Size& plugin_size) {
  // The amount of additional space in pixels to allocate to the right/bottom of
  // the context.
  constexpr int kBufferSize = 50;

  // Default to returning the same size.
  pp::Size result = current_context_size;

  // The minimum size of the plugin before resizing the context to ensure we
  // aren't wasting too much memory. We deduct twice the kBufferSize from the
  // current context size which gives a threshhold that is kBufferSize below
  // the plugin size when the context size was last computed.
  pp::Size min_size(
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
    result = pp::Size(plugin_size.width() + kBufferSize,
                      plugin_size.height() + kBufferSize);
  }

  return result;
}

void PaintManager::Initialize(pp::Instance* instance,
                              Client* client,
                              bool is_always_opaque) {
  DCHECK(!instance_ && !client_);  // Can't initialize twice.
  instance_ = instance;
  client_ = client;
  is_always_opaque_ = is_always_opaque;
}

void PaintManager::SetSize(const pp::Size& new_size, float device_scale) {
  if (GetEffectiveSize() == new_size &&
      GetEffectiveDeviceScale() == device_scale)
    return;

  has_pending_resize_ = true;
  pending_size_ = new_size;
  pending_device_scale_ = device_scale;

  view_size_changed_waiting_for_paint_ = true;

  Invalidate();
}

void PaintManager::SetTransform(float scale,
                                const pp::Point& origin,
                                const pp::Point& translate,
                                bool schedule_flush) {
  if (graphics_.is_null())
    return;

  graphics_.SetLayerTransform(scale, origin, translate);

  if (!schedule_flush)
    return;

  if (flush_pending_) {
    flush_requested_ = true;
    return;
  }
  Flush();
}

void PaintManager::ClearTransform() {
  SetTransform(1.f, pp::Point(), pp::Point(), false);
}

void PaintManager::Invalidate() {
  if (graphics_.is_null() && !has_pending_resize_)
    return;

  EnsureCallbackPending();
  aggregator_.InvalidateRect(pp::Rect(GetEffectiveSize()));
}

void PaintManager::InvalidateRect(const pp::Rect& rect) {
  DCHECK(!in_paint_);

  if (graphics_.is_null() && !has_pending_resize_)
    return;

  // Clip the rect to the device area.
  pp::Rect clipped_rect = rect.Intersect(pp::Rect(GetEffectiveSize()));
  if (clipped_rect.IsEmpty())
    return;  // Nothing to do.

  EnsureCallbackPending();
  aggregator_.InvalidateRect(clipped_rect);
}

void PaintManager::ScrollRect(const pp::Rect& clip_rect,
                              const pp::Point& amount) {
  DCHECK(!in_paint_);

  if (graphics_.is_null() && !has_pending_resize_)
    return;

  EnsureCallbackPending();

  aggregator_.ScrollRect(clip_rect, amount);
}

pp::Size PaintManager::GetEffectiveSize() const {
  return has_pending_resize_ ? pending_size_ : plugin_size_;
}

float PaintManager::GetEffectiveDeviceScale() const {
  return has_pending_resize_ ? pending_device_scale_ : device_scale_;
}

void PaintManager::EnsureCallbackPending() {
  // The best way for us to do the next update is to get a notification that
  // a previous one has completed. So if we're already waiting for one, we
  // don't have to do anything differently now.
  if (flush_pending_)
    return;

  // If no flush is pending, we need to do a manual call to get back to the
  // main thread. We may have one already pending, or we may need to schedule.
  if (manual_callback_pending_)
    return;

  pp::Module::Get()->core()->CallOnMainThread(
      0, callback_factory_.NewCallback(&PaintManager::OnManualCallbackComplete),
      0);
  manual_callback_pending_ = true;
}

void PaintManager::DoPaint() {
  in_paint_ = true;

  std::vector<ReadyRect> ready_rects;
  std::vector<pp::Rect> pending_rects;

  DCHECK(aggregator_.HasPendingUpdate());

  // Apply any pending resize. Setting the graphics to this class must happen
  // before asking the plugin to paint in case it requests invalides or resizes.
  // However, the bind must not happen until afterward since we don't want to
  // have an unpainted device bound. The needs_binding flag tells us whether to
  // do this later.
  if (has_pending_resize_) {
    plugin_size_ = pending_size_;
    // Only create a new graphics context if the current context isn't big
    // enough or if it is far too big. This avoids creating a new context if
    // we only resize by a small amount.
    pp::Size new_size = GetNewContextSize(graphics_.size(), pending_size_);
    if (graphics_.size() != new_size) {
      graphics_ = pp::Graphics2D(instance_, new_size, is_always_opaque_);
      graphics_need_to_be_bound_ = true;

      // Since we're binding a new one, all of the callbacks have been canceled.
      manual_callback_pending_ = false;
      flush_pending_ = false;
      callback_factory_.CancelAll();
    }

    if (pending_device_scale_ != 1.0)
      graphics_.SetScale(1.0 / pending_device_scale_);
    device_scale_ = pending_device_scale_;

    // This must be cleared before calling into the plugin since it may do
    // additional invalidation or sizing operations.
    has_pending_resize_ = false;
    pending_size_ = pp::Size();
  }

  PaintAggregator::PaintUpdate update = aggregator_.GetPendingUpdate();
  client_->OnPaint(update.paint_rects, &ready_rects, &pending_rects);

  if (ready_rects.empty() && pending_rects.empty()) {
    in_paint_ = false;
    return;  // Nothing was painted, don't schedule a flush.
  }

  std::vector<PaintAggregator::ReadyRect> ready_now;
  if (pending_rects.empty()) {
    std::vector<PaintAggregator::ReadyRect> temp_ready;
    temp_ready.insert(temp_ready.end(), ready_rects.begin(), ready_rects.end());
    aggregator_.SetIntermediateResults(temp_ready, pending_rects);
    ready_now = aggregator_.GetReadyRects();
    aggregator_.ClearPendingUpdate();

    // Apply any scroll first.
    if (update.has_scroll)
      graphics_.Scroll(update.scroll_rect, update.scroll_delta);

    view_size_changed_waiting_for_paint_ = false;
  } else {
    std::vector<PaintAggregator::ReadyRect> ready_later;
    for (const auto& ready_rect : ready_rects) {
      // Don't flush any part (i.e. scrollbars) if we're resizing the browser,
      // as that'll lead to flashes.  Until we flush, the browser will use the
      // previous image, but if we flush, it'll revert to using the blank image.
      // We make an exception for the first paint since we want to show the
      // default background color instead of the pepper default of black.
      if (ready_rect.flush_now &&
          (!view_size_changed_waiting_for_paint_ || first_paint_)) {
        ready_now.push_back(ready_rect);
      } else {
        ready_later.push_back(ready_rect);
      }
    }
    // Take the rectangles, except the ones that need to be flushed right away,
    // and save them so that everything is flushed at once.
    aggregator_.SetIntermediateResults(ready_later, pending_rects);

    if (ready_now.empty()) {
      in_paint_ = false;
      EnsureCallbackPending();
      return;
    }
  }

  for (const auto& ready_rect : ready_now) {
    graphics_.PaintImageData(ready_rect.image_data, ready_rect.offset,
                             ready_rect.rect);
  }

  Flush();

  in_paint_ = false;
  first_paint_ = false;

  if (graphics_need_to_be_bound_) {
    instance_->BindGraphics(graphics_);
    graphics_need_to_be_bound_ = false;
  }
}

void PaintManager::Flush() {
  flush_requested_ = false;

  int32_t result = graphics_.Flush(
      callback_factory_.NewCallback(&PaintManager::OnFlushComplete));

  // If you trigger this assertion, then your plugin has called Flush()
  // manually. When using the PaintManager, you should not call Flush, it will
  // handle that for you because it needs to know when it can do the next paint
  // by implementing the flush callback.
  //
  // Another possible cause of this assertion is re-using devices. If you
  // use one device, swap it with another, then swap it back, we won't know
  // that we've already scheduled a Flush on the first device. It's best to not
  // re-use devices in this way.
  DCHECK(result != PP_ERROR_INPROGRESS);

  if (result == PP_OK_COMPLETIONPENDING) {
    flush_pending_ = true;
  } else {
    DCHECK(result == PP_OK);  // Catch all other errors in debug mode.
  }
}

void PaintManager::OnFlushComplete(int32_t) {
  DCHECK(flush_pending_);
  flush_pending_ = false;

  // If more paints were enqueued while we were waiting for the flush to
  // complete, execute them now.
  if (aggregator_.HasPendingUpdate())
    DoPaint();

  // If there was another flush request while flushing we flush again.
  if (flush_requested_) {
    Flush();
  }
}

void PaintManager::OnManualCallbackComplete(int32_t) {
  DCHECK(manual_callback_pending_);
  manual_callback_pending_ = false;

  // Just because we have a manual callback doesn't mean there are actually any
  // invalid regions. Even though we only schedule this callback when something
  // is pending, a Flush callback could have come in before this callback was
  // executed and that could have cleared the queue.
  if (aggregator_.HasPendingUpdate())
    DoPaint();
}
