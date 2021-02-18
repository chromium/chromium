// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_H_
#define MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_H_

#include <linux/videodev2.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/mru_cache.h"
#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/base/cdm_context.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_types.h"
#include "media/gpu/chromeos/gpu_buffer_layout.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_video_decoder_backend.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class DmabufVideoFramePool;

class MEDIA_GPU_EXPORT V4L2VideoDecoder
    : public DecoderInterface,
      public V4L2VideoDecoderBackend::Client {
 public:
  // Create V4L2VideoDecoder instance. The success of the creation doesn't
  // ensure V4L2VideoDecoder is available on the device. It will be
  // determined in Initialize().
  static std::unique_ptr<DecoderInterface> Create(
      scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
      base::WeakPtr<DecoderInterface::Client> client);

  static SupportedVideoDecoderConfigs GetSupportedConfigs();

  // DecoderInterface implementation.
  void Initialize(const VideoDecoderConfig& config,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Reset(base::OnceClosure closure) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void ApplyResolutionChange() override;

  // V4L2VideoDecoderBackend::Client implementation
  void OnBackendError() override;
  bool IsDecoding() const override;
  void InitiateFlush() override;
  void CompleteFlush() override;
  void ChangeResolution(gfx::Size pic_size,
                        gfx::Rect visible_rect,
                        size_t num_output_frames) override;
  void OutputFrame(scoped_refptr<VideoFrame> frame,
                   const gfx::Rect& visible_rect,
                   base::TimeDelta timestamp) override;
  DmabufVideoFramePool* GetVideoFramePool() const override;

 private:
  friend class V4L2VideoDecoderTest;

  V4L2VideoDecoder(scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
                   base::WeakPtr<DecoderInterface::Client> client,
                   scoped_refptr<V4L2Device> device);
  ~V4L2VideoDecoder() override;

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

  // Setup format for output queue. This function sets output format on output
  // queue that is supported by a v4l2 driver, can be allocatable by
  // VideoFramePool and can be composited by chrome. This also updates format
  // in VideoFramePool. Return true if the setup is successful.
  bool SetupOutputFormat(const gfx::Size& size, const gfx::Rect& visible_rect);

  // Start streaming V4L2 input and (if |start_output_queue| is true) output
  // queues. Attempt to start |device_poll_thread_| after streaming starts.
  bool StartStreamV4L2Queue(bool start_output_queue);
  // Stop streaming V4L2 output and (if |stop_input_queue| is true) input
  // queues. Stop |device_poll_thread_| before stopping streaming.
  bool StopStreamV4L2Queue(bool stop_input_queue);
  // Try to dequeue input and output buffers from device.
  void ServiceDeviceTask(bool event);

  // After the pipeline finished flushing frames, reconfigure the resolution
  // setting of V4L2 device and the frame pool.
  void ContinueChangeResolution(const gfx::Size& pic_size,
                                const gfx::Rect& visible_rect,
                                const size_t num_output_frames);

  // Change the state and check the state transition is valid.
  void SetState(State new_state);

  // Pages with multiple V4L2VideoDecoder instances might run out of memory
  // (e.g. b/170870476) or crash (e.g. crbug.com/1109312). To avoid that and
  // while the investigation goes on, limit the maximum number of simultaneous
  // decoder instances for now. |num_instances_| tracks the number of
  // simultaneous decoders. |can_use_decoder_| is true iff we haven't reached
  // the maximum number of instances at the time this decoder is created.
  static constexpr int kMaxNumOfInstances = 32;
  static base::AtomicRefCount num_instances_;
  const bool can_use_decoder_;

  // The V4L2 backend, i.e. the part of the decoder that sends
  // decoding jobs to the kernel.
  std::unique_ptr<V4L2VideoDecoderBackend> backend_;

  // V4L2 device in use.
  scoped_refptr<V4L2Device> device_;

  // Callback to change resolution, called after the pipeline is flushed.
  base::OnceClosure continue_change_resolution_cb_;

  // State of the instance.
  State state_ = State::kUninitialized;

  // Number of output frames requested to |frame_pool_|.
  // The default value is only used at the first time of
  // DmabufVideoFramePool::Initialize() during Initialize().
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
  base::WeakPtr<V4L2VideoDecoder> weak_this_;
  base::WeakPtrFactory<V4L2VideoDecoder> weak_this_factory_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_H_
