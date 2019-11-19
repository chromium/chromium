// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_SLICE_VIDEO_DECODER_H_
#define MEDIA_GPU_V4L2_V4L2_SLICE_VIDEO_DECODER_H_

#include <linux/videodev2.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/containers/mru_cache.h"
#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "media/gpu/chromeos/gpu_buffer_layout.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_video_decoder_backend.h"
#include "media/video/supported_video_decoder_config.h"

namespace media {

class DmabufVideoFramePool;

class MEDIA_GPU_EXPORT V4L2SliceVideoDecoder
    : public VideoDecoderPipeline::DecoderInterface,
      public V4L2VideoDecoderBackend::Client {
 public:
  using GetFramePoolCB = base::RepeatingCallback<DmabufVideoFramePool*()>;

  // Create V4L2SliceVideoDecoder instance. The success of the creation doesn't
  // ensure V4L2SliceVideoDecoder is available on the device. It will be
  // determined in Initialize().
  static std::unique_ptr<VideoDecoderPipeline::DecoderInterface> Create(
      scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
      GetFramePoolCB get_pool_cb);

  static SupportedVideoDecoderConfigs GetSupportedConfigs();

  // VideoDecoderPipeline::DecoderInterface implementation.
  void Initialize(const VideoDecoderConfig& config,
                  InitCB init_cb,
                  const OutputCB& output_cb) override;
  void Reset(base::OnceClosure closure) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;

  // V4L2VideoDecoderBackend::Client implementation
  void OnBackendError() override;
  bool IsDecoding() const override;
  void InitiateFlush() override;
  void CompleteFlush() override;
  bool ChangeResolution(gfx::Size pic_size,
                        gfx::Rect visible_rect,
                        size_t num_output_frames) override;
  void OutputFrame(scoped_refptr<VideoFrame> frame,
                   const gfx::Rect& visible_rect,
                   base::TimeDelta timestamp) override;

 private:
  friend class V4L2SliceVideoDecoderTest;

  V4L2SliceVideoDecoder(
      scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
      scoped_refptr<V4L2Device> device,
      GetFramePoolCB get_pool_cb);
  ~V4L2SliceVideoDecoder() override;

  enum class State {
    // Initial state. Transitions to |kDecoding| if Initialize() is successful,
    // |kError| otherwise.
    kUninitialized,
    // Transitions to |kFlushing| when flushing or changing resolution,
    // |kError| if any unexpected error occurs.
    kDecoding,
    // Transitions to |kDecoding| when flushing or changing resolution is
    // finished. Do not process new input buffer in this state.
    kFlushing,
    // Error state. Cannot transition to other state anymore.
    kError,
  };

  class BitstreamIdGenerator {
   public:
    BitstreamIdGenerator() { DETACH_FROM_SEQUENCE(sequence_checker_); }
    int32_t GetNextBitstreamId() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      next_bitstream_buffer_id_ = (next_bitstream_buffer_id_ + 1) & 0x7FFFFFFF;
      return next_bitstream_buffer_id_;
    }

   private:
    int32_t next_bitstream_buffer_id_ = 0;
    SEQUENCE_CHECKER(sequence_checker_);
  };

  // Setup format for input queue.
  bool SetupInputFormat(uint32_t input_format_fourcc);

  // Set the coded size on the input queue.
  // Return true if the successful, false otherwise.
  bool SetCodedSizeOnInputQueue(const gfx::Size& size);

  // Setup format for output queue. This function sets output format on output
  // queue that is supported by a v4l2 driver, can be allocatable by
  // VideoFramePool and can be composited by chrome. This also updates format
  // in VideoFramePool. The returned VideoFrameLayout is one of VideoFrame that
  // VideoFramePool will allocate. Returns base::nullopt on failure of if there
  // is no format that satisfies the above conditions.
  base::Optional<GpuBufferLayout> SetupOutputFormat(
      const gfx::Size& size,
      const gfx::Rect& visible_rect);
  // Update the format of frames in |frame_pool_| with |output_format_fourcc|,
  // |size| and |visible_rect|.
  base::Optional<GpuBufferLayout> UpdateVideoFramePoolFormat(
      uint32_t output_format_fourcc,
      const gfx::Size& size,
      const gfx::Rect& visible_rect);

  // Start streaming V4L2 input and output queues. Attempt to start
  // |device_poll_thread_| before starting streaming.
  bool StartStreamV4L2Queue();
  // Stop streaming V4L2 input and output queues. Stop |device_poll_thread_|
  // before stopping streaming.
  bool StopStreamV4L2Queue();
  // Try to dequeue input and output buffers from device.
  void ServiceDeviceTask(bool event);

  // Change the state and check the state transition is valid.
  void SetState(State new_state);

  // The V4L2 backend, i.e. the part of the decoder that sends
  // decoding jobs to the kernel.
  std::unique_ptr<V4L2VideoDecoderBackend> backend_;

  // V4L2 device in use.
  scoped_refptr<V4L2Device> device_;
  // VideoFrame manager used to allocate and recycle video frame.
  GetFramePoolCB get_pool_cb_;
  DmabufVideoFramePool* frame_pool_ = nullptr;

  // Decoder task runner. All public methods of
  // VideoDecoderPipeline::DecoderInterface are executed at this task runner.
  const scoped_refptr<base::SequencedTaskRunner> decoder_task_runner_;

  // State of the instance.
  State state_ = State::kUninitialized;

  // Parameters for generating output VideoFrame.
  base::Optional<VideoFrameLayout> frame_layout_;
  // Number of output frames requested to |frame_pool_|.
  // The default value is only used at the first time of
  // DmabufVideoFramePool::RequestFrames() during Initialize().
  size_t num_output_frames_ = 1;
  // Ratio of natural_size to visible_rect of the output frame.
  double pixel_aspect_ratio_ = 0.0;

  // Callbacks passed from Initialize().
  OutputCB output_cb_;

  // V4L2 input and output queue.
  scoped_refptr<V4L2Queue> input_queue_;
  scoped_refptr<V4L2Queue> output_queue_;

  BitstreamIdGenerator bitstream_id_generator_;

  SEQUENCE_CHECKER(decoder_sequence_checker_);

  // |weak_this_| must be dereferenced and invalidated on
  // |decoder_task_runner_|.
  base::WeakPtr<V4L2SliceVideoDecoder> weak_this_;
  base::WeakPtrFactory<V4L2SliceVideoDecoder> weak_this_factory_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_SLICE_VIDEO_DECODER_H_
