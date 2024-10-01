// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_VIDEO_ENCODE_ACCELERATOR_ADAPTER_H_
#define MEDIA_VIDEO_VIDEO_ENCODE_ACCELERATOR_ADAPTER_H_

#include <memory>
#include <optional>

#include "base/containers/circular_deque.h"
#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_pool.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/video_encoder.h"
#include "media/base/video_frame_converter.h"
#include "media/media_buildflags.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SequencedTaskRunner;
}

namespace media {
class GpuVideoAcceleratorFactories;
class MediaLog;
class H264AnnexBToAvcBitstreamConverter;
#if BUILDFLAG(ENABLE_PLATFORM_HEVC) && \
    BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
class H265AnnexBToHevcBitstreamConverter;
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC) &&
        // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

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
      std::unique_ptr<MediaLog> media_log,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      VideoEncodeAccelerator::Config::EncoderType required_encoder_type =
          VideoEncodeAccelerator::Config::EncoderType::kHardware);
  ~VideoEncodeAcceleratorAdapter() override;

  enum class InputBufferKind { Any, GpuMemBuf, CpuMemBuf };
  // A way to force a certain way of submitting frames to VEA.
  void SetInputBufferPreferenceForTesting(InputBufferKind type);

  // VideoEncoder implementation.
  void Initialize(VideoCodecProfile profile,
                  const Options& options,
                  EncoderInfoCB info_cb,
                  OutputCB output_cb,
                  EncoderStatusCB done_cb) override;
  void Encode(scoped_refptr<VideoFrame> frame,
              const EncodeOptions& encode_options,
              EncoderStatusCB done_cb) override;
  void ChangeOptions(const Options& options,
                     OutputCB output_cb,
                     EncoderStatusCB done_cb) override;
  void Flush(EncoderStatusCB done_cb) override;

  // VideoEncodeAccelerator::Client implementation
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;

  void BitstreamBufferReady(int32_t buffer_id,
                            const BitstreamBufferMetadata& metadata) override;

  void NotifyErrorStatus(const EncoderStatus& status) override;

  void NotifyEncoderInfoChange(const VideoEncoderInfo& info) override;

  // For async disposal by AsyncDestroyVideoEncoder
  static void DestroyAsync(std::unique_ptr<VideoEncodeAcceleratorAdapter> self);

 private:
  class GpuMemoryBufferVideoFramePool;
  class ReadOnlyRegionPool;
  enum class State {
    kNotInitialized,
    kWaitingForFirstFrame,
    kInitializing,
    kReadyToEncode,
    kFlushing,
    kReconfiguring
  };
  struct PendingOp {
    PendingOp();
    ~PendingOp();

    EncoderStatusCB done_callback;
    base::TimeDelta timestamp;
    gfx::ColorSpace color_space;
  };

  void FlushCompleted(bool success);
  void InitCompleted(EncoderStatus status);
  void InitializeOnAcceleratorThread(VideoCodecProfile profile,
                                     const Options& options,
                                     EncoderInfoCB info_cb,
                                     OutputCB output_cb,
                                     EncoderStatusCB done_cb);
  void InitializeInternalOnAcceleratorThread();
  void EncodeOnAcceleratorThread(scoped_refptr<VideoFrame> frame,
                                 EncodeOptions encode_options,
                                 EncoderStatusCB done_cb);
  void FlushOnAcceleratorThread(EncoderStatusCB done_cb);
  void ChangeOptionsOnAcceleratorThread(const Options options,
                                        OutputCB output_cb,
                                        EncoderStatusCB done_cb);

  template <class T>
  T WrapCallback(T cb);
  EncoderStatus::Or<scoped_refptr<VideoFrame>> PrepareGpuFrame(
      scoped_refptr<VideoFrame> src_frame);
  EncoderStatus::Or<scoped_refptr<VideoFrame>> PrepareCpuFrame(
      scoped_refptr<VideoFrame> src_frame);

  scoped_refptr<ReadOnlyRegionPool> input_pool_;
  scoped_refptr<base::UnsafeSharedMemoryPool> output_pool_;
  std::vector<std::unique_ptr<base::UnsafeSharedMemoryPool::Handle>>
      output_buffer_handles_;
  scoped_refptr<GpuMemoryBufferVideoFramePool> gmb_frame_pool_;

  std::unique_ptr<VideoEncodeAccelerator> accelerator_;
  raw_ptr<GpuVideoAcceleratorFactories> gpu_factories_;
  std::unique_ptr<MediaLog> media_log_;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  // If |h264_converter_| is null, we output in annexb format. Otherwise, we
  // output in avc format.
  std::unique_ptr<H264AnnexBToAvcBitstreamConverter> h264_converter_;
#if BUILDFLAG(ENABLE_PLATFORM_HEVC) && \
    BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  // If |h265_converter_| is null, we output in annexb format. Otherwise, we
  // output in hevc format.
  std::unique_ptr<H265AnnexBToHevcBitstreamConverter> h265_converter_;
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC) &&
        // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  // These are encodes that have been sent to the accelerator but have not yet
  // had their encoded data returned via BitstreamBufferReady().
  base::circular_deque<std::unique_ptr<PendingOp>> active_encodes_;

  // Color space associated w/ the last frame sent to accelerator for encoding.
  gfx::ColorSpace last_frame_color_space_;

  std::unique_ptr<PendingOp> pending_flush_;

  // For calling accelerator_ methods
  scoped_refptr<base::SequencedTaskRunner> accelerator_task_runner_;
  SEQUENCE_CHECKER(accelerator_sequence_checker_);

  // For calling user provided callbacks
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  State state_ = State::kNotInitialized;
  std::optional<bool> flush_support_;

  // True if underlying instance of VEA can handle GPU backed frames with a
  // size different from what VEA was configured for.
  bool gpu_resize_supported_ = false;

  // These are encodes that have not been sent to the accelerator.
  std::vector<std::unique_ptr<PendingEncode>> pending_encodes_;

  VideoPixelFormat format_;
  InputBufferKind input_buffer_preference_ = InputBufferKind::Any;
  VideoFrameConverter frame_converter_;

  VideoCodecProfile profile_ = VIDEO_CODEC_PROFILE_UNKNOWN;
  VideoEncodeAccelerator::SupportedRateControlMode supported_rc_modes_ =
      VideoEncodeAccelerator::kNoMode;
  std::vector<VideoPixelFormat> gpu_supported_pixel_formats_;
  Options options_;
  EncoderInfoCB info_cb_;
  OutputCB output_cb_;
  EncoderStatusCB reconfigure_cb_;

  gfx::Size input_coded_size_;

  VideoEncodeAccelerator::Config::EncoderType required_encoder_type_ =
      VideoEncodeAccelerator::Config::EncoderType::kHardware;
  bool supports_frame_size_change_ = false;
};

}  // namespace media
#endif  // MEDIA_VIDEO_VIDEO_ENCODE_ACCELERATOR_ADAPTER_H_
