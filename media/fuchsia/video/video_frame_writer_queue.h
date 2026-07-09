// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_VIDEO_VIDEO_FRAME_WRITER_QUEUE_H_
#define MEDIA_FUCHSIA_VIDEO_VIDEO_FRAME_WRITER_QUEUE_H_

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/sysmem2/cpp/fidl.h>

#include <optional>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/encoder_status.h"
#include "media/base/media_export.h"
#include "media/fuchsia/common/stream_processor_helper.h"
#include "media/fuchsia/common/vmo_buffer.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class VideoFrame;

// Stores a queue of VideoFrames to be copied to VmoBuffers. VideoFrames can
// be queued before VmoBuffers are available. Queue will not start processing
// before Initialize() is called.
class MEDIA_EXPORT VideoFrameWriterQueue final {
 public:
  using ProcessCB =
      base::RepeatingCallback<void(StreamProcessorHelper::IoPacket)>;

  VideoFrameWriterQueue();

  VideoFrameWriterQueue(const VideoFrameWriterQueue&) = delete;
  VideoFrameWriterQueue& operator=(const VideoFrameWriterQueue&) = delete;

  ~VideoFrameWriterQueue();

  // Enqueues a VideoFrame. Can be called before `Initialize()`. Immediately
  // processes `frame` if a VmoBuffer is available. Returns false if the frame
  // is invalid. Returning false will trigger a fatal error, closing the
  // encoder stream and returning an error to the JavaScript layer.
  bool Enqueue(scoped_refptr<VideoFrame> frame, bool force_keyframe);

  // Initializes the queue and starts processing if possible. `process_cb` is
  // called after each VideoFrame is copied. Returns EncoderStatus::Codes::kOk
  // on success, or an error status if initialization fails (e.g., if buffer
  // settings are invalid or buffers are too small). Returning a failure
  // status will trigger a fatal error, closing the encoder stream and
  // returning an error to the JavaScript layer.
  EncoderStatus Initialize(
      std::vector<VmoBuffer> buffers,
      fuchsia::sysmem2::SingleBufferSettings buffer_settings,
      fuchsia::media::FormatDetails initial_format_details,
      gfx::Size coded_size,
      ProcessCB process_cb);

 private:
  class FrameSize final {
   public:
    static std::optional<FrameSize> Create(
        const fuchsia::sysmem2::ImageFormatConstraints& constraints,
        const gfx::Size& coded_size);

    gfx::Size coded_size() const { return coded_size_; }
    int dst_y_stride() const { return dst_y_stride_; }
    int dst_uv_stride() const { return dst_uv_stride_; }

    // Guaranteed to be in the range of [0, INT32_MAX], but returned as size_t
    // to avoid static_cast when working with base::span.
    size_t dst_y_plane_size() const {
      return static_cast<size_t>(dst_y_plane_size_);
    }

    // Guaranteed to be in the range of [0, INT32_MAX], but returned as size_t
    // to avoid static_cast when working with base::span.
    size_t dst_uv_plane_size() const {
      return static_cast<size_t>(dst_uv_plane_size_);
    }

    // Guaranteed to be in the range of [0, INT32_MAX], but returned as size_t
    // to avoid static_cast when comparing with buffer sizes.
    size_t dst_size() const { return static_cast<size_t>(dst_size_); }

   private:
    FrameSize(gfx::Size coded_size,
              int dst_y_stride,
              int dst_uv_stride,
              int dst_y_plane_size,
              int dst_uv_plane_size,
              int dst_size);

    const gfx::Size coded_size_;
    const int dst_y_stride_;
    const int dst_uv_stride_;
    const int dst_y_plane_size_;
    const int dst_uv_plane_size_;
    const int dst_size_;
  };

  struct Item final {
    Item(scoped_refptr<VideoFrame> frame, bool force_keyframe);

    // Item is move-constructible for popping from the queue.
    Item(const Item&) = delete;
    Item& operator=(const Item&) = delete;

    Item(Item&&);
    Item& operator=(Item&&) = delete;

    ~Item();

    scoped_refptr<VideoFrame> frame;
    const bool force_keyframe;
  };

  void ProcessQueue();

  // Marks the VmoBuffer at `buffer_index` to be available for copying.
  void ReleaseBuffer(size_t buffer_index);

  // Copies a VideoFrame from `item` to VmoBuffer at `buffer_index`. Returns
  // true on success. The return value is for testing purposes only;
  // production code always asserts that the copy succeeded.
  bool CopyFrameToBuffer(const Item& item, size_t buffer_index);

  base::queue<Item> queue_;
  std::vector<VmoBuffer> buffers_;
  base::queue<size_t> free_buffer_indices_;
  fuchsia::media::FormatDetails format_details_;
  ProcessCB process_cb_;

  std::optional<FrameSize> frame_size_;

  base::WeakPtrFactory<VideoFrameWriterQueue> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_VIDEO_VIDEO_FRAME_WRITER_QUEUE_H_
