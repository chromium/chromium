// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_VIDEO_ENCODE_ACCELERATOR_ADAPTER_H_
#define MEDIA_VIDEO_VIDEO_ENCODE_ACCELERATOR_ADAPTER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "media/base/media_export.h"
#include "media/base/shared_memory_pool.h"
#include "media/base/video_encoder.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SequencedTaskRunner;
}

namespace media {
class GpuVideoAcceleratorFactories;
class H264AnnexBToAvcBitstreamConverter;

// This class is a somewhat complex adapter from VideoEncodeAccelerator
// to VideoEncoder, it takes cares of such things as
// - managing and copying GPU/shared memory buffers
// - managing hops between task runners, for VEA and callbacks
// - keeping track of the state machine. Forbiding encodes during flush etc.
class MEDIA_EXPORT VideoEncodeAcceleratorAdapter
    : public VideoEncoder,
      public VideoEncodeAccelerator::Client {
 public:
  VideoEncodeAcceleratorAdapter(
      GpuVideoAcceleratorFactories* gpu_factories,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner);
  ~VideoEncodeAcceleratorAdapter() override;

  enum class InputBufferKind { Any, GpuMemBuf, CpuMemBuf };
  // A way to force a certain way of submitting frames to VEA.
  void SetInputBufferPreferenceForTesting(InputBufferKind type);

  // VideoEncoder implementation.
  void Initialize(VideoCodecProfile profile,
                  const Options& options,
                  OutputCB output_cb,
                  StatusCB done_cb) override;
  void Encode(scoped_refptr<VideoFrame> frame,
              bool key_frame,
              StatusCB done_cb) override;
  void ChangeOptions(const Options& options,
                     OutputCB output_cb,
                     StatusCB done_cb) override;
  void Flush(StatusCB done_cb) override;

  // VideoEncodeAccelerator::Client implementation
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;

  void BitstreamBufferReady(int32_t buffer_id,
                            const BitstreamBufferMetadata& metadata) override;

  void NotifyError(VideoEncodeAccelerator::Error error) override;

  void NotifyEncoderInfoChange(const VideoEncoderInfo& info) override;

  // For async disposal by AsyncDestroyVideoEncoder
  static void DestroyAsync(std::unique_ptr<VideoEncodeAcceleratorAdapter> self);

 private:
  enum class State {
    kNotInitialized,
    kWaitingForFirstFrame,
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
  void InitializeInternalOnAcceleratorThread();
  void EncodeOnAcceleratorThread(scoped_refptr<VideoFrame> frame,
                                 bool key_frame,
                                 StatusCB done_cb);
  void FlushOnAcceleratorThread(StatusCB done_cb);
  void ChangeOptionsOnAcceleratorThread(const Options options,
                                        OutputCB output_cb,
                                        StatusCB done_cb);

  template <class T>
  T WrapCallback(T cb);
  StatusOr<scoped_refptr<VideoFrame>> PrepareGpuFrame(
      const gfx::Size& size,
      scoped_refptr<VideoFrame> src_frame);
  StatusOr<scoped_refptr<VideoFrame>> PrepareCpuFrame(
      const gfx::Size& size,
      scoped_refptr<VideoFrame> src_frame);

  scoped_refptr<SharedMemoryPool> output_pool_;
  scoped_refptr<SharedMemoryPool> input_pool_;
  std::unique_ptr<SharedMemoryPool::SharedMemoryHandle> output_handle_holder_;
  size_t input_buffer_size_;

  std::unique_ptr<VideoEncodeAccelerator> accelerator_;
  GpuVideoAcceleratorFactories* gpu_factories_;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  // If |h264_converter_| is null, we output in annexb format. Otherwise, we
  // output in avc format.
  std::unique_ptr<H264AnnexBToAvcBitstreamConverter> h264_converter_;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  // These are encodes that have been sent to the accelerator but have not yet
  // had their encoded data returned via BitstreamBufferReady().
  base::circular_deque<std::unique_ptr<PendingOp>> active_encodes_;

  std::unique_ptr<PendingOp> pending_flush_;

  // For calling accelerator_ methods
  scoped_refptr<base::SequencedTaskRunner> accelerator_task_runner_;
  SEQUENCE_CHECKER(accelerator_sequence_checker_);

  // For calling user provided callbacks
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  State state_ = State::kNotInitialized;
  base::Optional<bool> flush_support_;

  // True if underlying instance of VEA can handle GPU backed frames with a
  // size different from what VEA was configured for.
  bool gpu_resize_supported_ = false;

  struct PendingEncode {
    PendingEncode();
    ~PendingEncode();
    StatusCB done_callback;
    scoped_refptr<VideoFrame> frame;
    bool key_frame;
  };

  // These are encodes that have not been sent to the accelerator.
  std::vector<std::unique_ptr<PendingEncode>> pending_encodes_;

  VideoPixelFormat format_;
  InputBufferKind input_buffer_preference_ = InputBufferKind::Any;
  std::vector<uint8_t> resize_buf_;

  VideoCodecProfile profile_ = VIDEO_CODEC_PROFILE_UNKNOWN;
  Options options_;
  OutputCB output_cb_;
};

}  // namespace media
#endif  // MEDIA_VIDEO_VIDEO_ENCODE_ACCELERATOR_ADAPTER_H_
