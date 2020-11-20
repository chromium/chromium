// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_VIDEO_ENCODE_ACCELERATOR_ADAPTER_H_
#define MEDIA_VIDEO_VIDEO_ENCODE_ACCELERATOR_ADAPTER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/circular_deque.h"
#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "media/base/media_export.h"
#include "media/base/video_encoder.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/size.h"

namespace media {
class GpuVideoAcceleratorFactories;
class H264AnnexBToAvcBitstreamConverter;

// This class is a somewhat complex adapter from VideoEncodeAccelerator
// to VideoEncoder, it takes cares of such things as
// - managing and copying GPU-shared memory buffers
// - managing hops between task runners, for VEA and callbacks
// - keeping track of the state machine. Forbiding encodes during flush etc.
class MEDIA_EXPORT VideoEncodeAcceleratorAdapter
    : public VideoEncoder,
      public media::VideoEncodeAccelerator::Client {
 public:
  VideoEncodeAcceleratorAdapter(
      media::GpuVideoAcceleratorFactories* gpu_factories,
      scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner);
  ~VideoEncodeAcceleratorAdapter() override;

  // VideoEncoder implementation.
  void Initialize(VideoCodecProfile profile,
                  const Options& options,
                  OutputCB output_cb,
                  StatusCB done_cb) override;
  void Encode(scoped_refptr<VideoFrame> frame,
              bool key_frame,
              StatusCB done_cb) override;
  void ChangeOptions(const Options& options, StatusCB done_cb) override;
  void Flush(StatusCB done_cb) override;

  // VideoEncodeAccelerator::Client implementation
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;

  void BitstreamBufferReady(int32_t buffer_id,
                            const BitstreamBufferMetadata& metadata) override;

  void NotifyError(media::VideoEncodeAccelerator::Error error) override;

  void NotifyEncoderInfoChange(const VideoEncoderInfo& info) override;

  // For async disposal by AsyncDestroyVideoEncoder
  static void DestroyAsync(std::unique_ptr<VideoEncodeAcceleratorAdapter> self);

 private:
  class SharedMemoryPool;
  enum class State {
    kNotInitialized,
    kInitializing,
    kReadyToEncode,
    kFlushing
  };
  struct PendingOp {
    PendingOp();
    ~PendingOp();

    StatusCB done_callback;
    base::TimeDelta timestamp;
  };

  void FlushCompleted(bool success);
  void InitCompleted(Status status);
  void InitializeOnAcceleratorThread(VideoCodecProfile profile,
                                     const Options& options,
                                     OutputCB output_cb,
                                     StatusCB done_cb);
  void EncodeOnAcceleratorThread(scoped_refptr<VideoFrame> frame,
                                 bool key_frame,
                                 StatusCB done_cb);
  void FlushOnAcceleratorThread(StatusCB done_cb);

  template <class T>
  T WrapCallback(T cb);

  scoped_refptr<SharedMemoryPool> output_pool_;
  scoped_refptr<SharedMemoryPool> input_pool_;
  std::unique_ptr<VideoEncodeAccelerator> accelerator_;
  media::GpuVideoAcceleratorFactories* gpu_factories_;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  std::unique_ptr<H264AnnexBToAvcBitstreamConverter> h264_converter_;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  base::circular_deque<std::unique_ptr<PendingOp>> pending_encodes_;
  std::unique_ptr<PendingOp> pending_flush_;
  std::unique_ptr<PendingOp> pending_init_;

  // For calling accelerator_ methods
  scoped_refptr<base::SingleThreadTaskRunner> accelerator_task_runner_;

  // For calling user provided callbacks
  scoped_refptr<base::SingleThreadTaskRunner> callback_task_runner_;

  State state_ = State::kNotInitialized;
  bool flush_support_ = false;

  Options options_;
  OutputCB output_cb_;
};

}  // namespace media
#endif  // MEDIA_VIDEO_VIDEO_ENCODE_ACCELERATOR_ADAPTER_H_