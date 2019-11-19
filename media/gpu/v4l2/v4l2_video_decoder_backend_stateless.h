// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_BACKEND_STATELESS_H_
#define MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_BACKEND_STATELESS_H_

#include "base/containers/mru_cache.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "media/base/decode_status.h"
#include "media/base/video_decoder.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/v4l2/v4l2_decode_surface_handler.h"
#include "media/gpu/v4l2/v4l2_video_decoder_backend.h"

namespace media {

class AcceleratedVideoDecoder;

class V4L2StatelessVideoDecoderBackend : public V4L2VideoDecoderBackend,
                                         public V4L2DecodeSurfaceHandler {
 public:
  // Constructor for the stateless backend. Arguments are:
  // |client| the decoder we will be backing.
  // |device| the V4L2 decoder device.
  // |frame_pool| pool from which to get backing memory for decoded frames.
  // |profile| profile of the codec we will decode.
  // |task_runner| the decoder task runner, to which we will post our tasks.
  V4L2StatelessVideoDecoderBackend(
      Client* const client,
      scoped_refptr<V4L2Device> device,
      DmabufVideoFramePool* const frame_pool,
      VideoCodecProfile profile,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  ~V4L2StatelessVideoDecoderBackend() override;

  // V4L2VideoDecoderBackend implementation
  bool Initialize() override;
  void EnqueueDecodeTask(scoped_refptr<DecoderBuffer> buffer,
                         VideoDecoder::DecodeCB decode_cb,
                         int32_t bitstream_id) override;
  void OnOutputBufferDequeued(V4L2ReadableBufferRef buffer) override;
  void OnStreamStopped() override;
  void ClearPendingRequests(DecodeStatus status) override;

  // V4L2DecodeSurfaceHandler implementation.
  scoped_refptr<V4L2DecodeSurface> CreateSurface() override;
  bool SubmitSlice(const scoped_refptr<V4L2DecodeSurface>& dec_surface,
                   const uint8_t* data,
                   size_t size) override;
  void DecodeSurface(
      const scoped_refptr<V4L2DecodeSurface>& dec_surface) override;
  void SurfaceReady(const scoped_refptr<V4L2DecodeSurface>& dec_surface,
                    int32_t bitstream_id,
                    const gfx::Rect& visible_rect,
                    const VideoColorSpace& color_space) override;

 private:
  // Request for displaying the surface or calling the decode callback.
  struct OutputRequest;

  // Request for decoding buffer. Every EnqueueDecodeTask() call generates 1
  // DecodeRequest.
  struct DecodeRequest {
    // The decode buffer passed to EnqueueDecodeTask().
    scoped_refptr<DecoderBuffer> buffer;
    // The callback function passed to EnqueueDecodeTask().
    VideoDecoder::DecodeCB decode_cb;
    // The identifier for the decoder buffer.
    int32_t bitstream_id;

    DecodeRequest(scoped_refptr<DecoderBuffer> buf,
                  VideoDecoder::DecodeCB cb,
                  int32_t id);

    // Allow move, but not copy
    DecodeRequest(DecodeRequest&&);
    DecodeRequest& operator=(DecodeRequest&&);

    ~DecodeRequest();

    DISALLOW_COPY_AND_ASSIGN(DecodeRequest);
  };

  // The reason the decoding is paused.
  enum class PauseReason {
    // Not stopped, decoding normally.
    kNone,
    // Cannot create a new V4L2 surface. Waiting for surfaces to be released.
    kRanOutOfSurfaces,
    // A VP9 superframe contains multiple subframes. Before decoding the next
    // subframe, we need to wait for previous subframes decoded and update the
    // context.
    kWaitSubFrameDecoded,
  };

  // Callback which is called when V4L2 surface is destroyed.
  void ReuseOutputBuffer(V4L2ReadableBufferRef buffer);

  // Try to advance the decoding work.
  void DoDecodeWork();
  // Try to decode buffer from the pending decode request queue.
  // This method stops decoding when:
  // - Run out of surface
  // - Flushing or changing resolution
  // Invoke this method again when these situation ends.
  bool PumpDecodeTask();
  // Try to output surface from |output_request_queue_|.
  // This method stops outputting surface when the first surface is not dequeued
  // from the V4L2 device. Invoke this method again when any surface is
  // dequeued from the V4L2 device.
  void PumpOutputSurfaces();
  // Setup the format of V4L2 output buffer, and allocate new buffer set.
  bool ChangeResolution();

  // Check whether request api is supported or not.
  bool CheckRequestAPISupport();
  // Allocate necessary request buffers is request api is supported.
  bool AllocateRequests();

  // Video frame pool provided by the decoder.
  DmabufVideoFramePool* const frame_pool_;

  // Video profile we will be decoding.
  const VideoCodecProfile profile_;

  // Video decoder used to parse stream headers by software.
  std::unique_ptr<AcceleratedVideoDecoder> avd_;

  // The decode request which is currently processed.
  base::Optional<DecodeRequest> current_decode_request_;
  // Surfaces enqueued to V4L2 device. Since we are stateless, they are
  // guaranteed to be proceeded in FIFO order.
  base::queue<scoped_refptr<V4L2DecodeSurface>> surfaces_at_device_;

  // Queue of pending decode request.
  base::queue<DecodeRequest> decode_request_queue_;

  // Queue of pending output request.
  base::queue<OutputRequest> output_request_queue_;

  // Indicates why decoding is currently paused.
  PauseReason pause_reason_ = PauseReason::kNone;

  // The time at which each buffer decode operation started. Not each decode
  // operation leads to a frame being output and frames might be reordered, so
  // we don't know when it's safe to drop a timestamp. This means we need to use
  // a cache here, with a size large enough to account for frame reordering.
  base::MRUCache<int32_t, base::TimeDelta> bitstream_id_to_timestamp_;

  // The task runner we are running on, for convenience.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Callbacks of EOS buffer passed from Decode().
  VideoDecoder::DecodeCB flush_cb_;

  // Set to true during Initialize() if the codec driver supports request API.
  bool supports_requests_ = false;
  // FIFO queue of requests, only used if supports_requests_ is true.
  base::queue<base::ScopedFD> requests_;
  // Stores the media file descriptor, only used if supports_requests_ is true.
  base::ScopedFD media_fd_;

  base::WeakPtr<V4L2StatelessVideoDecoderBackend> weak_this_;
  base::WeakPtrFactory<V4L2StatelessVideoDecoderBackend> weak_this_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(V4L2StatelessVideoDecoderBackend);
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_BACKEND_STATELESS_H_
