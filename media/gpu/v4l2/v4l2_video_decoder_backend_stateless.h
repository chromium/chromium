// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_BACKEND_STATELESS_H_
#define MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_BACKEND_STATELESS_H_

#include "base/containers/lru_cache.h"
#include "base/containers/queue.h"
#include "base/containers/small_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/id_type.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_status.h"
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
      VideoCodecProfile profile,
      const VideoColorSpace& color_space,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      CdmContext* cdm_context);

  V4L2StatelessVideoDecoderBackend(const V4L2StatelessVideoDecoderBackend&) =
      delete;
  V4L2StatelessVideoDecoderBackend& operator=(
      const V4L2StatelessVideoDecoderBackend&) = delete;

  ~V4L2StatelessVideoDecoderBackend() override;

  // V4L2VideoDecoderBackend implementation
  bool Initialize() override;
  void EnqueueDecodeTask(scoped_refptr<DecoderBuffer> buffer,
                         VideoDecoder::DecodeCB decode_cb) override;
  void OnOutputBufferDequeued(V4L2ReadableBufferRef buffer) override;
  void OnStreamStopped(bool stop_input_queue) override;
  bool ApplyResolution(const gfx::Size& pic_size,
                       const gfx::Rect& visible_rect) override;
  void OnChangeResolutionDone(CroStatus status) override;
  void ClearPendingRequests(DecoderStatus status) override;
  bool StopInputQueueOnResChange() const override;
  size_t GetNumOUTPUTQueueBuffers(bool secure_mode) const override;

  // V4L2DecodeSurfaceHandler implementation.
  scoped_refptr<V4L2DecodeSurface> CreateSurface() override;
  scoped_refptr<V4L2DecodeSurface> CreateSecureSurface(
      uint64_t secure_handle) override;
  bool SubmitSlice(V4L2DecodeSurface* dec_surface,
                   const uint8_t* data,
                   size_t size) override;
  void DecodeSurface(scoped_refptr<V4L2DecodeSurface> dec_surface) override;
  void SurfaceReady(scoped_refptr<V4L2DecodeSurface> dec_surface,
                    int32_t bitstream_id,
                    const gfx::Rect& visible_rect,
                    const VideoColorSpace& color_space) override;
  void ResumeDecoding() override;

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

    DecodeRequest(const DecodeRequest&) = delete;
    DecodeRequest& operator=(const DecodeRequest&) = delete;

    // Allow move, but not copy
    DecodeRequest(DecodeRequest&&);
    DecodeRequest& operator=(DecodeRequest&&);

    ~DecodeRequest();
  };

  // The reason the decoding is paused.
  enum class PauseReason {
    // Not stopped, decoding normally.
    kNone,
    // Cannot create a new V4L2 surface. Waiting for surfaces to be released.
    kRanOutOfSurfaces,
  };

  // Callback which is called when the output buffer is not used anymore.
  static void ReuseOutputBufferThunk(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::optional<base::WeakPtr<V4L2StatelessVideoDecoderBackend>> weak_this,
      V4L2ReadableBufferRef buffer);
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
  void ChangeResolution();

  // Returns whether |profile| is supported by a v4l2 stateless decoder driver.
  bool IsSupportedProfile(VideoCodecProfile profile);

  // Create codec-specific AcceleratedVideoDecoder and reset related variables.
  bool CreateDecoder();

  // Video profile we are decoding.
  VideoCodecProfile profile_;

  // Video color space we are decoding.
  VideoColorSpace color_space_;

  // Video coded size we are decoding.
  gfx::Size pic_size_;

  // Video decoder used to parse stream headers by software.
  std::unique_ptr<AcceleratedVideoDecoder> decoder_;

  // The decode request which is currently processed.
  std::optional<DecodeRequest> current_decode_request_;
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
  base::LRUCache<int32_t, base::TimeDelta> bitstream_id_to_timestamp_;

  // The task runner we are running on, for convenience.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Callbacks of EOS buffer passed from Decode().
  VideoDecoder::DecodeCB flush_cb_;

  // VideoCodecProfiles supported by a v4l2 stateless decoder driver.
  std::vector<VideoCodecProfile> supported_profiles_;

  // Reference to request queue to get free requests.
  raw_ptr<V4L2RequestsQueue> requests_queue_;

  // Map of enqueuing timestamps to wall clock, for histogramming purposes.
  base::small_map<std::map<int64_t, base::TimeTicks>> enqueuing_timestamps_;
  // Same but with ScopedDecodeTrace for chrome:tracing purposes.
  base::small_map<std::map<base::TimeDelta, std::unique_ptr<ScopedDecodeTrace>>>
      buffer_tracers_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Int32 safe ID generator, starting at 0. Generated IDs are used to uniquely
  // identify a Decode() request for stateless backends. BitstreamID is just
  // a "phantom type" (see StrongAlias), essentially just a name.
  struct BitstreamID {};
  base::IdType32<BitstreamID>::Generator bitstream_id_generator_
      GUARDED_BY_CONTEXT(sequence_checker_);

  raw_ptr<CdmContext> cdm_context_;

  base::WeakPtr<V4L2StatelessVideoDecoderBackend> weak_this_;
  base::WeakPtrFactory<V4L2StatelessVideoDecoderBackend> weak_this_factory_{
      this};
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_BACKEND_STATELESS_H_
