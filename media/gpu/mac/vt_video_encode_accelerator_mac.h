// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VT_VIDEO_ENCODE_ACCELERATOR_MAC_H_
#define MEDIA_GPU_MAC_VT_VIDEO_ENCODE_ACCELERATOR_MAC_H_

#include <memory>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "media/base/bitrate.h"
#include "media/base/mac/videotoolbox_helpers.h"
#include "media/base/video_codecs.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_encode_accelerator.h"
#include "third_party/webrtc/common_video/include/bitrate_adjuster.h"

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
  void RequestEncodingParametersChange(const Bitrate& bitrate,
                                       uint32_t framerate) override;
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

  // Encoding tasks to be run on |encoder_thread_|.
  void EncodeTask(scoped_refptr<VideoFrame> frame, bool force_keyframe);
  void UseOutputBitstreamBufferTask(
      std::unique_ptr<BitstreamBufferRef> buffer_ref);
  void RequestEncodingParametersChangeTask(const Bitrate& bitrate,
                                           uint32_t framerate);
  void DestroyTask();

  // Helper functions to set bitrate.
  void SetAdjustedConstantBitrate(uint32_t bitrate);
  void SetVariableBitrate(const Bitrate& bitrate);

  // Helper function to notify the client of an error on |client_task_runner_|.
  void NotifyError(VideoEncodeAccelerator::Error error);

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

  // Reset the encoder's compression session by destroying the existing one
  // using DestroyCompressionSession() and creating a new one. The new session
  // is configured using ConfigureCompressionSession().
  bool ResetCompressionSession(VideoCodec codec);

  // Create a compression session.
  bool CreateCompressionSession(VideoCodec codec, const gfx::Size& input_size);

  // Configure the current compression session using current encoder settings.
  bool ConfigureCompressionSession(VideoCodec codec);

  // Destroy the current compression session if any. Blocks until all pending
  // frames have been flushed out (similar to EmitFrames without doing any
  // encoding work).
  void DestroyCompressionSession();

  // Flushes the encoder. The flush callback won't be run until all pending
  // encodes have been completed.
  void FlushTask(FlushCallback flush_callback);
  void MaybeRunFlushCallback();

  base::ScopedCFTypeRef<VTCompressionSessionRef> compression_session_;

  gfx::Size input_visible_size_;
  size_t bitstream_buffer_size_ = 0;
  int32_t frame_rate_ = 0;
  int num_temporal_layers_ = 1;
  VideoCodecProfile profile_ = H264PROFILE_BASELINE;
  VideoCodec codec_ = VideoCodec::kH264;

  media::Bitrate bitrate_;

  // Bitrate adjuster is used only for constant bitrate mode. In variable
  // bitrate mode no adjustments are needed.
  // Bitrate adjuster used to fix VideoToolbox's inconsistent bitrate issues.
  webrtc::BitrateAdjuster bitrate_adjuster_;
  uint32_t target_bitrate_ = 0;       // User for CBR only
  uint32_t encoder_set_bitrate_ = 0;  // User for CBR only

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
  const scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  SEQUENCE_CHECKER(client_sequence_checker_);

  // To expose client callbacks from VideoEncodeAccelerator.
  // NOTE: all calls to this object *MUST* be executed on
  // |client_task_runner_|.
  base::WeakPtr<Client> client_;
  std::unique_ptr<base::WeakPtrFactory<Client>> client_ptr_factory_;

  // This thread services tasks posted from the VEA API entry points by the
  // GPU child thread and CompressionCallback() posted from device thread.
  scoped_refptr<base::SingleThreadTaskRunner> encoder_thread_task_runner_;

  // Tracking information for ensuring flushes aren't completed until all
  // pending encodes have been returned.
  int pending_encodes_ = 0;
  FlushCallback pending_flush_cb_;

  // Declared last to ensure that all weak pointers are invalidated before
  // other destructors run.
  base::WeakPtr<VTVideoEncodeAccelerator> encoder_weak_ptr_;
  base::WeakPtrFactory<VTVideoEncodeAccelerator> encoder_task_weak_factory_;
};

}  // namespace media

#endif  // MEDIA_GPU_MAC_VT_VIDEO_ENCODE_ACCELERATOR_MAC_H_
