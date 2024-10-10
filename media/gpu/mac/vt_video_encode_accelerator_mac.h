// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VT_VIDEO_ENCODE_ACCELERATOR_MAC_H_
#define MEDIA_GPU_MAC_VT_VIDEO_ENCODE_ACCELERATOR_MAC_H_

#include <memory>

#include "base/apple/scoped_cftyperef.h"
#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/bitrate.h"
#include "media/base/mac/videotoolbox_helpers.h"
#include "media/base/video_codecs.h"
#include "media/gpu/media_gpu_export.h"
#include "media/media_buildflags.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/color_space.h"

namespace media {

class MediaLog;

// VideoToolbox.framework implementation of the VideoEncodeAccelerator
// interface for MacOSX. VideoToolbox makes no guarantees that it is thread
// safe, so this object is pinned to the thread on which it is constructed.
class MEDIA_GPU_EXPORT VTVideoEncodeAccelerator
    : public VideoEncodeAccelerator {
 public:
  VTVideoEncodeAccelerator();

  VTVideoEncodeAccelerator(const VTVideoEncodeAccelerator&) = delete;
  VTVideoEncodeAccelerator& operator=(const VTVideoEncodeAccelerator&) = delete;

  // VideoEncodeAccelerator implementation.
  SupportedProfiles GetSupportedProfiles() override;

  bool Initialize(const Config& config,
                  Client* client,
                  std::unique_ptr<MediaLog> media_log = nullptr) override;
  void Encode(scoped_refptr<VideoFrame> frame, bool force_keyframe) override;
  void UseOutputBitstreamBuffer(BitstreamBuffer buffer) override;
  void RequestEncodingParametersChange(
      const Bitrate& bitrate,
      uint32_t framerate,
      const std::optional<gfx::Size>& size) override;
  void Destroy() override;
  void Flush(FlushCallback flush_callback) override;
  bool IsFlushSupported() override;

 private:
  // Holds the associated data of a video frame being processed.
  struct InProgressFrameEncode;

  // Holds output buffers coming from the encoder.
  struct EncodeOutput;

  // Holds output buffers coming from the client ready to be filled.
  struct BitstreamBufferRef;

  ~VTVideoEncodeAccelerator() override;

  // Compression session callback function to handle compressed frames.
  static void CompressionCallback(void* encoder_opaque,
                                  void* request_opaque,
                                  OSStatus status,
                                  VTEncodeInfoFlags info,
                                  CMSampleBufferRef sbuf);
  void CompressionCallbackTask(OSStatus status,
                               std::unique_ptr<EncodeOutput> encode_output);

  // Copy CMSampleBuffer into a BitstreamBuffer and return it to the |client_|.
  void ReturnBitstreamBuffer(
      std::unique_ptr<EncodeOutput> encode_output,
      std::unique_ptr<VTVideoEncodeAccelerator::BitstreamBufferRef> buffer_ref);

  // Get the supported H.264 profiles.
  SupportedProfiles GetSupportedH264Profiles();
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  // Get the supported HEVC profiles.
  SupportedProfiles GetSupportedHEVCProfiles();
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

  // Reset the encoder's compression session by destroying the existing one and
  // creating a new one. The new session is configured using
  // ConfigureCompressionSession().
  bool ResetCompressionSession();

  // Configure the current compression session using current encoder settings.
  bool ConfigureCompressionSession(VideoCodec codec);

  // Flushes the encoder. The flush callback won't be run until all pending
  // encodes have been completed.
  void MaybeRunFlushCallback();

  void SetEncoderColorSpace();

  void NotifyErrorStatus(EncoderStatus status);

  base::TimeDelta AssignMonotonicTimestamp();

  video_toolbox::ScopedVTCompressionSessionRef compression_session_;

  gfx::Size input_visible_size_;
  size_t bitstream_buffer_size_ = 0;
  int32_t frame_rate_ = 0;
  int num_temporal_layers_ = 1;
  VideoCodecProfile profile_ = H264PROFILE_BASELINE;
  VideoCodec codec_ = VideoCodec::kH264;

  media::Bitrate bitrate_;

  // If True, the encoder fails initialization if setting of session's property
  // kVTCompressionPropertyKey_MaxFrameDelayCount returns an error.
  // Encoder can work even after if MaxFrameDelayCount fails, but it'll
  // have larger latency on low resolutions, and it's bad for RTC.
  // Context: https://crbug.com/1195177 https://crbug.com/webrtc/7304
  bool require_low_delay_ = true;

  // Used to control selection of OS software encoders.
  Config::EncoderType required_encoder_type_ = Config::EncoderType::kHardware;

  // Bitstream buffers ready to be used to return encoded output as a FIFO.
  base::circular_deque<std::unique_ptr<BitstreamBufferRef>>
      bitstream_buffer_queue_;

  // EncodeOutput needs to be copied into a BitstreamBufferRef as a FIFO.
  base::circular_deque<std::unique_ptr<EncodeOutput>> encoder_output_queue_;

  // Our original calling task runner for the child thread.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // To expose client callbacks from VideoEncodeAccelerator.
  raw_ptr<Client> client_ = nullptr;

  std::unique_ptr<MediaLog> media_log_;

  // Tracking information for ensuring flushes aren't completed until all
  // pending encodes have been returned.
  int pending_encodes_ = 0;
  FlushCallback pending_flush_cb_;

  // Color space of the first frame sent to Encode().
  std::optional<gfx::ColorSpace> encoder_color_space_;
  bool can_set_encoder_color_space_ = true;

  // Monotonically-growing timestamp that will be assigned to the next frame
  base::TimeDelta next_timestamp_;

  // Declared last to ensure that all weak pointers are invalidated before
  // other destructors run.
  base::WeakPtr<VTVideoEncodeAccelerator> encoder_weak_ptr_;
  base::WeakPtrFactory<VTVideoEncodeAccelerator> encoder_weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_MAC_VT_VIDEO_ENCODE_ACCELERATOR_MAC_H_
