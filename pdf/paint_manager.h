// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PAINT_MANAGER_H_
#define PDF_PAINT_MANAGER_H_

#include <optional>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "pdf/paint_aggregator.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/size.h"

class SkImage;
class SkSurface;

namespace gfx {
class Point;
class Rect;
class Vector2d;
class Vector2dF;
}  // namespace gfx

namespace chrome_pdf {

// Custom PaintManager for the PDF plugin.  This is branched from the Pepper
// version.  The difference is that this supports progressive rendering of dirty
// rects, where multiple calls to the rendering engine are needed.  It also
// supports having higher-priority rects flushing right away, i.e. the
// scrollbars.
//
// The client's OnPaint
class PaintManager {
 public:
  class Client {
   public:
    // Invalidates the entire plugin container, scheduling a repaint.
    virtual void InvalidatePluginContainer() = 0;

    // Paints the given invalid area of the plugin to the given graphics
    // device. Returns true if anything was painted.
    //
    // You are given the list of rects to paint in `paint_rects`.  You can
    // combine painting into less rectangles if it's more efficient.  When a
    // rect is painted, information about that paint should be inserted into
    // `ready`.  Otherwise if a paint needs more work, add the rect to
    // `pending`.  If `pending` is not empty, your OnPaint function will get
    // called again.  Once OnPaint is called and it returns no pending rects,
    // all the previously ready rects will be flushed on screen.  The exception
    // is for ready rects that have `flush_now` set to true.  These will be
    // flushed right away.
    //
    // Do not call Flush() on the graphics device, this will be done
    // automatically if you return true from this function since the
    // PaintManager needs to handle the callback.
    //
    // Calling Invalidate/Scroll is not allowed while inside an OnPaint
    virtual void OnPaint(const std::vector<gfx::Rect>& paint_rects,
                         std::vector<PaintReadyRect>& ready,
                         std::vector<gfx::Rect>& pending) = 0;

    // Install the image buffer into the backing store used by PDFium for the
    // purposes of the PdfBufferedPaintManager experiment.
    virtual SkBitmap* InstallBuffer(SkImageInfo image_info, void* data) = 0;

    // Updates the client with the latest snapshot created by `Flush()`.
    virtual void UpdateSnapshot(sk_sp<SkImage> snapshot) = 0;

    // Updates the client with the latest output scale.
    virtual void UpdateScale(float scale) = 0;

    // Updates the client with the latest output layer transform.
    virtual void UpdateLayerTransform(float scale,
                                      const gfx::Vector2dF& translate) = 0;

   protected:
    // You shouldn't delete through this interface.
    ~Client() = default;
  };

  // The Client is a non-owning pointer and must remain valid (normally the
  // object implementing the Client interface will own the paint manager).
  //
  // You will need to call SetSize() before this class will do anything.
  // Normally you do this from UpdateGeometryOnViewChanged() of your plugin
  // instance.
  explicit PaintManager(Client* client);
  PaintManager(const PaintManager&) = delete;
  PaintManager& operator=(const PaintManager&) = delete;
  ~PaintManager();

  // Returns the size of the graphics context to allocate for a given plugin
  // size. We may allocated a slightly larger buffer than required so that we
  // don't have to resize the context when scrollbars appear/dissapear due to
  // zooming (which can result in flickering).
  static gfx::Size GetNewContextSize(const gfx::Size& current_context_size,
                                     const gfx::Size& plugin_size);

  // Sets the size of the plugin. If the size is the same as the previous call,
  // this will be a NOP. If the size has changed, a new device will be
  // allocated to the given size and a paint to that device will be scheduled.
  //
  // This is intended to be called from ViewChanged with the size of the
  // plugin. Since it tracks the old size and only allocates when the size
  // changes, you can always call this function without worrying about whether
  // the size changed or ViewChanged is called for another reason (like the
  // position changed).
  void SetSize(const gfx::Size& new_size,
               float new_device_scale,
               SkAlphaType alpha_type);

  // Invalidate the entire plugin.
  void Invalidate();

  // Invalidate the given rect.
  void InvalidateRect(const gfx::Rect& rect);

  // The given rect should be scrolled by the given amounts.
  void ScrollRect(const gfx::Rect& clip_rect, const gfx::Vector2d& amount);

  // Returns the size of the graphics context for the next paint operation.
  // This is the pending size if a resize is pending (the plugin has called
  // SetSize but we haven't actually painted it yet), or the current size of
  // no resize is pending.
  gfx::Size GetEffectiveSize() const;
  float GetEffectiveDeviceScale() const;

  // Set the transform for the graphics layer.
  // If `schedule_flush` is true, it ensures a flush will be scheduled for
  // this change. If `schedule_flush` is false, then the change will not take
  // effect until another change causes a flush.
  void SetTransform(float scale,
                    const gfx::Point& origin,
                    const gfx::Vector2d& translate,
                    bool schedule_flush);
  // Resets any transform for the graphics layer.
  // This does not schedule a flush.
  void ClearTransform();

  struct BufferData {
    BufferData(size_t size, base::WeakPtr<PaintManager> owner);
    ~BufferData();
    base::HeapArray<uint8_t> allocation;
    base::WeakPtr<PaintManager> owner;
  };

  // Called by skia to release a data buffer back to the PaintManager, for the
  // PdfBufferedPaintManager experiment. This is done to avoid the need for
  // synchronization. Must be called on main thread.
  void BufferFinishedOnMainThread(std::unique_ptr<BufferData> data);

 private:
  // Makes sure there is a callback that will trigger a paint at a later time.
  // This will be either a Flush callback telling us we're allowed to generate
  // more data, or, if there's no flush callback pending, a manual call back
  // to the message loop via ExecuteOnMainThread.
  void EnsureCallbackPending();

  // Does the client paint and executes a Flush if necessary.
  void DoPaint();

  // Executes a Flush.
  void Flush();

  // Executes a Flush when the PdfBufferedPaintManager experiment is
  // enabled.
  void BufferedFlush(sk_sp<SkData> flushing_buffer);

  // Callback for asynchronous completion of Flush.
  void OnFlushComplete();

  // Callback for manual scheduling of paints when there is no flush callback
  // pending.
  void OnManualCallbackComplete();

  // For the PdfBufferedPaintManager experiment, return an existing or newly
  // allocated buffer for storing pixel data at least large enough according to
  // `image_info_`.
  std::unique_ptr<BufferData> GetBuffer();

  // Non-owning pointer. See the constructor.
  const raw_ptr<Client> client_;

  // Backing Skia surface. If running PdfBufferedPaintManager, the surface's
  // backing buffer is swapped every frame.
  sk_sp<SkSurface> surface_;

  // The surface's ImageInfo, but also client_'s ImageInfo, as well as
  // `client_`'s `engine_`'s.
  SkImageInfo image_info_;

  // In the PdfBufferedPaintManager experiment, there is a rotating stack of
  // equally-sized buffers which are drawn into largely in sequence. They are
  // created on demand, given to skia when they are drawn into, and kept
  // afterwards for reuse, if possible. When buffers are passed to Skia, they
  // enter the nether realm and it is not knowable when they will return. When
  // they are passed back, we put them back into free_buffers_ only if
  // PaintManager is still alive and the buffer is large enough to fit the
  // current canvas size. UaFs if PaintManager is destroyed while Skia is
  // holding the buffers are prevented by invaliding WeakPtrs to the
  // PaintManager on destruction.
  //
  // The main buffer which is drawn into by PDFium. This buffer is popped from
  // the stack every reraster, installed in `client_` and `surface_`, and
  // directly modified. Only used in the PdfBufferedPaintManager experiment.
  std::unique_ptr<BufferData> draw_buffer_;
  // Buffers into which can currently be written. Popped from in DoPaint(), it
  // is handed off to the compositor in BufferedFlush(), and perhaps (assuming
  // PaintManager is alive and the buffer is not too small) put back once skia
  // calls BufferFinishedOnMainThread(). Only used in the
  // PdfBufferedPaintManager experiment.
  std::vector<std::unique_ptr<BufferData>> free_buffers_;

  // Pointer to the previous frame. Cleared when there is a resize. Used for
  // scrolling. Only used in the PdfBufferedPaintManager experiment.
  std::optional<sk_sp<SkImage>> previous_frame_;

  // Buffer that is currently in use by the engine. Changes on resize, but not
  // at any other time.
  raw_ptr<SkBitmap> engine_bitmap_;
  std::unique_ptr<BufferData> engine_buffer_;

  PaintAggregator aggregator_;

  // See comment for EnsureCallbackPending for more on how these work.
  bool manual_callback_pending_ = false;
  bool flush_pending_ = false;
  bool flush_requested_ = false;

  // When we get a resize, we don't do so right away (see `SetSize()`). The
  // `has_pending_resize_` tells us that we need to do a resize for the next
  // paint operation. When true, the new size is in `pending_size_`.
  bool has_pending_resize_ = false;
  gfx::Size pending_size_;
  gfx::Size plugin_size_;
  float pending_device_scale_ = 1.0f;
  float device_scale_ = 1.0f;

  // True iff we're in the middle of a paint.
  bool in_paint_ = false;

  // True if we haven't painted the plugin viewport yet.
  bool first_paint_ = true;

  // True when the view size just changed and we're waiting for a paint.
  bool view_size_changed_waiting_for_paint_ = false;

  base::WeakPtrFactory<PaintManager> weak_factory_{this};
  // We need a separate WeakPtrFactory which is only invalidated on destruction
  // rather than semantic callback cancellation.
  base::WeakPtrFactory<PaintManager> buffer_return_weak_factory_{this};
};

}  // namespace chrome_pdf

#endif  // PDF_PAINT_MANAGER_H_
