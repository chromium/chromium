// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_STATELESS_V4L2_STATELESS_VIDEO_DECODER_H_
#define MEDIA_GPU_V4L2_STATELESS_V4L2_STATELESS_VIDEO_DECODER_H_

#include <optional>
#include <queue>
#include <vector>

#include "base/atomic_ref_count.h"
#include "base/containers/lru_cache.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder.h"
#include "media/base/media_log.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_decoder.h"
#include "media/base/waiting.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/v4l2/stateless/queue.h"
#include "media/gpu/v4l2/stateless/stateless_decode_surface_handler.h"
#include "media/gpu/v4l2/stateless/stateless_device.h"

namespace media {

// V4L2 Stateless Video Decoder implements the Request API for decoding video
// using a memory to memory interface.
// https://www.kernel.org/doc/html/latest/userspace-api/media/mediactl/request-api.html
// https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/dev-stateless-decoder.html
class MEDIA_GPU_EXPORT V4L2StatelessVideoDecoder
    : public VideoDecoderMixin,
      public StatelessDecodeSurfaceHandler {
 public:
  static std::unique_ptr<VideoDecoderMixin> Create(
      std::unique_ptr<MediaLog> media_log,
      scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
      base::WeakPtr<VideoDecoderMixin::Client> client);

  // VideoDecoderMixin implementation, VideoDecoder part.
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const PipelineOutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure reset_cb) override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;
  int GetMaxDecodeRequests() const override;
  VideoDecoderType GetDecoderType() const override;
  bool IsPlatformDecoder() const override;
  // VideoDecoderMixin implementation, specific part.
  void ApplyResolutionChange() override;
  size_t GetMaxOutputFramePoolSize() const override;

  // StatelessDecodeSurfaceHandler implementation.
  scoped_refptr<StatelessDecodeSurface> CreateSurface() override;
  void SurfaceReady(scoped_refptr<StatelessDecodeSurface> dec_surface,
                    int32_t bitstream_id,
                    const gfx::Rect& visible_rect,
                    const VideoColorSpace& color_space) override;
  bool SubmitFrame(void* ctrls,
                   const uint8_t* data,
                   size_t size,
                   scoped_refptr<StatelessDecodeSurface> dec_surface) override;

  static int GetMaxNumDecoderInstancesForTesting() {
    return GetMaxNumDecoderInstances();
  }

 private:
  V4L2StatelessVideoDecoder(
      std::unique_ptr<MediaLog> media_log,
      scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
      base::WeakPtr<VideoDecoderMixin::Client> client,
      scoped_refptr<StatelessDevice> device);
  ~V4L2StatelessVideoDecoder() override;

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

  // Create a codec specific decoder. When successful this decoder is stored in
  // the |decoder_| member variable.
  bool CreateDecoder();

  // Allocate and configure buffers necessary for the current video bitstream.
  void ConfigureInputQueue();

  // Prepare the output queue with the correct uncompressed format.
  bool ConfigureOutputQueue(void* ctrls);

  // The uncompressed format that the driver produces is setup by the
  // |output_queue_|. This format then needs to be passed further down the
  // pipeline.
  CroStatus SetupOutputFormatForPipeline();

  // Callbacks used to handle buffers that have been dequeued.
  void DequeueBuffers(bool success);

  // Callback for frame destructor observer that will return the output
  // buffer back to the queue for future use.
  void ReturnDecodedOutputBuffer(uint64_t frame_id);

  // Empty out the |decode_request_queue_| and |display_queue_|.
  void ClearPendingRequests(DecoderStatus status);

  // Match up frames that have been decoded and are sitting in the
  // |output_queue_| with |display_queue_| which holds the frames in display
  // order.
  void ServiceDisplayQueue();

  // Service the queue of outstanding decode request. The client can send
  // multiple compressed frames without waiting for a callback. These frames
  // need to be queued up as there may not be free input buffers available.
  void ServiceDecodeRequestQueue();

  // Pages with multiple decoder instances might run out of memory (e.g.
  // b/170870476) or crash (e.g. crbug.com/1109312). this class method provides
  // that number to prevent that erroneous behaviour during Initialize().
  static int GetMaxNumDecoderInstances();
  // Tracks the number of decoder instances globally in the process.
  static base::AtomicRefCount num_decoder_instances_;

  SEQUENCE_CHECKER(decoder_sequence_checker_);

  const scoped_refptr<StatelessDevice> device_;

  // Callback obtained from Initialize() to be called after every frame
  // has finished decoding and is ready for the client to display.
  PipelineOutputCB output_cb_ GUARDED_BY_CONTEXT(decoder_sequence_checker_);

  // Hold the callback that came in with the EOS signal until the rest of the
  // frames have finished decoding.
  DecodeCB flush_cb_ GUARDED_BY_CONTEXT(decoder_sequence_checker_);

  // Video decoder used to parse stream headers by software.
  std::unique_ptr<AcceleratedVideoDecoder> decoder_;

  // Queue to hold compressed bitstream buffers to be submitted to the hardware
  std::unique_ptr<InputQueue> input_queue_;

  // Queue to hold uncompressed image buffers returned by the hardware
  std::unique_ptr<OutputQueue> output_queue_;

  // Queue for interfacing with the request api
  std::unique_ptr<RequestQueue> request_queue_;

  // Surfaces enqueued to V4L2 device. Since we are stateless, they are
  // guaranteed to be proceeded in FIFO order.
  base::queue<scoped_refptr<StatelessDecodeSurface>> surfaces_queued_;

  // Store configuration information from when Initialize() was called.
  VideoAspectRatio aspect_ratio_;
  VideoCodecProfile profile_;
  VideoColorSpace color_space_info_;

  // Int32 safe ID generator, starting at 0. Generated IDs are used to uniquely
  // identify a Decode() request for stateless backends. BitstreamID is just
  // a "phantom type" (see StrongAlias), essentially just a name.
  struct BitstreamID {};
  base::IdType32<BitstreamID>::Generator bitstream_id_generator_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);

  // Unique enough identifier so that all outstanding reference frames have a
  // unique identifier
  struct FrameID {};
  base::IdTypeU64<FrameID>::Generator frame_id_generator_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);

  // On an EOS the decoder must make sure that all the frames have made it
  // through the queue. To do that the |last_frame_id_generated_| holds the id
  // of the last input buffer while |last_frame_id_dequeued_| holds the id of
  // the last output buffer.
  uint64_t last_frame_id_generated_ = 0;
  uint64_t last_frame_id_dequeued_ = 0;

  base::LRUCache<int32_t, base::TimeDelta> bitstream_id_to_timestamp_;

  // Queue of pending decode request.
  std::queue<DecodeRequest> decode_request_queue_;

  // The decode request decode loop needs to keep this alive.
  std::optional<DecodeRequest> current_decode_request_;

  // Queue holding surfaces in display order.
  std::queue<scoped_refptr<StatelessDecodeSurface>> display_queue_;

  // Indicates that a resolution change has been signaled by the |decoder_|,
  // but the queues have not yet been configured for the new resolution.
  bool resolution_changing_ = false;

  // High priority task runner that can block. This task runner is to be used
  // for synchronizing queue tasks.
  scoped_refptr<base::SequencedTaskRunner> queue_task_runner_;

  // Weak factories associated with the main thread
  // (|decoder_sequence_checker_|).
  base::WeakPtrFactory<V4L2StatelessVideoDecoder> weak_ptr_factory_for_events_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_STATELESS_V4L2_STATELESS_VIDEO_DECODER_H_
