// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_NDK_VIDEO_ENCODE_ACCELERATOR_H_
#define MEDIA_GPU_ANDROID_NDK_VIDEO_ENCODE_ACCELERATOR_H_

#include <android/native_window.h>
#include <media/NdkMediaCodec.h>
#include <stdint.h>

#include <array>
#include <memory>
#include <vector>

#include "base/android/requires_api.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/bitrate.h"
#include "media/base/media_log.h"
#include "media/base/video_encoder.h"
#include "media/gpu/android/ndk_media_codec_wrapper.h"
#include "media/gpu/android/video_frame_gl_surface_renderer.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gl/android/scoped_a_native_window.h"

namespace media {

class BitstreamBuffer;
class TemporalScalabilityIdExtractor;
class VEAEncodingLatencyMetricsHelper;

class REQUIRES_ANDROID_API(NDK_MEDIA_CODEC_MIN_API) MEDIA_GPU_EXPORT
    NdkVideoEncodeAccelerator final : public VideoEncodeAccelerator,
                                      public NdkMediaCodecWrapper::Client {
 public:
  // |runner| - a task runner that will be used for all callbacks and external
  // calls to this instance.
  explicit NdkVideoEncodeAccelerator(
      scoped_refptr<base::SequencedTaskRunner> runner);

  NdkVideoEncodeAccelerator(const NdkVideoEncodeAccelerator&) = delete;
  NdkVideoEncodeAccelerator& operator=(const NdkVideoEncodeAccelerator&) =
      delete;
  ~NdkVideoEncodeAccelerator() override;

  // VideoEncodeAccelerator implementation.
  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles() override;
  EncoderStatus Initialize(const Config& config,
                           VideoEncodeAccelerator::Client* client,
                           std::unique_ptr<MediaLog> media_log) override;
  void Encode(scoped_refptr<VideoFrame> frame, bool force_keyframe) override;
  void Encode(scoped_refptr<VideoFrame> frame,
              const VideoEncoder::EncodeOptions& options) override;
  void UseOutputBitstreamBuffer(BitstreamBuffer buffer) override;
  void RequestEncodingParametersChange(
      const Bitrate& bitrate,
      uint32_t framerate,
      const std::optional<gfx::Size>& size) override;
  void Destroy() override;
  bool IsFlushSupported() override;
  void SetCommandBufferHelperCB(
      base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()>
          get_command_buffer_helper_cb,
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner) override;

  // MediaCodecWrapper::Client implementation.
  void OnInputAvailable() override;
  void OnOutputAvailable() override;
  void OnError(media_status_t error) override;

 private:
  struct FrameTimestampInfo {
    // The original timestamp of the input VideoFrame, it's used for
    // assigning timestamps to the outputs.
    base::TimeDelta real_timestamp;
    // The wall-clock time when the frame is sent to the encoder, it's used
    // for latency calculation.
    base::TimeTicks encode_start_time;
  };

  enum class SyncState {
    kReadyForEncoding,
    kNeedsSync,
    kSyncInProgress,
  };

  struct PendingEncode {
    PendingEncode(scoped_refptr<VideoFrame> frame,
                  const VideoEncoder::EncodeOptions& options);
    ~PendingEncode();
    PendingEncode(PendingEncode&&);
    PendingEncode& operator=(PendingEncode&&);

    scoped_refptr<VideoFrame> frame;
    VideoEncoder::EncodeOptions options;

    // The synchronization state of this frame.
    SyncState sync_state = SyncState::kReadyForEncoding;
  };

  // Ask MediaCodec what input buffer layout it prefers and set values of
  // |input_buffer_stride_| and |input_buffer_yplane_height_|. If the codec
  // does not provide these values, sets up |aligned_size_| such that encoded
  // frames are cropped to the nearest 16x16 alignment.
  bool SetInputBufferLayout(const gfx::Size& configured_size);

  // Reads a frame from `pending_frames_` (if it has any) does some checks and
  // and prep work and calls either `FeedInputBuffer()` or `FeedGLSurface()`
  void FeedInput();

  // Called when the sync token for a shared image frame has been waited on.
  void OnSyncDone(VideoFrame::ID frame_id);

  // Renders the `frame` onto the encoder's input surface using the
  // `gl_renderer_` and passes the `timestamp` to the encoder.
  void FeedGLSurface(scoped_refptr<VideoFrame> frame,
                     base::TimeDelta timestamp);
  // Copies the `frame` into an available MediaCodec input buffer and
  // queues it for the encoder with the given `timestamp`.
  void FeedInputBuffer(scoped_refptr<VideoFrame> frame,
                       base::TimeDelta timestamp);
  media_status_t SendEndOfStream();

  // Read encoded data from |media_codec_output_buffers_| copy it to a buffer
  // available in |available_bitstream_buffers_| and tell |client_ptr_factory_|
  // that encoded data is ready.
  void DrainOutput();

  // Read config data from |media_codec_output_buffers_| and copy it to
  // |config_data_|. |config_data_| is later propagated to key-frame encoded
  // chunks.
  bool DrainConfig();

  void NotifyMediaCodecError(EncoderStatus encoder_status,
                             media_status_t media_codec_status,
                             std::string message);
  void NotifyErrorStatus(EncoderStatus status);

  // Generates a monotonically increasing timestamp to be used when feeding
  // input to the MediaCodec. Stores the original `real_timestamp` and the
  // current time as the encoding start time in a map
  // Returns the generated monotonic timestamp.
  base::TimeDelta RecordFrameTimestamps(base::TimeDelta real_timestamp);

  // Retrieves and removes the FrameTimestampInfo associated with the given
  // `monotonic_timestamp` from the map.
  std::optional<FrameTimestampInfo> RetrieveFrameTimestamps(
      base::TimeDelta monotonic_timestamp);

  EncoderStatus ResetMediaCodec();

  void SetEncoderColorSpace();

  void NotifyEncoderInfo();

  void OnCommandBufferHelperAvailable(
      scoped_refptr<CommandBufferHelper> command_buffer_helper);

  SEQUENCE_CHECKER(sequence_checker_);

  // VideoEncodeAccelerator::Client callbacks go here.  Invalidated once any
  // error triggers.
  std::unique_ptr<base::WeakPtrFactory<VideoEncodeAccelerator::Client>>
      client_ptr_factory_;

  std::unique_ptr<NdkMediaCodecWrapper> media_codec_;

  Config config_;

  bool error_occurred_ = false;

  uint32_t effective_framerate_ = 0;
  Bitrate effective_bitrate_;

  // Y and UV plane strides in the encoder's input buffer
  int32_t input_buffer_stride_ = 0;

  // Y-plane height in the encoder's input
  int32_t input_buffer_yplane_height_ = 0;

  // A runner all for callbacks and externals calls to public methods.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Frames waiting to be passed to the codec, queued until these conditions are
  // met:
  // - input buffer is available (if we use buffers)
  // - pending color space change is applied
  // - shared image sync is done
  base::circular_deque<PendingEncode> pending_frames_;

  // Bitstream buffers waiting to be populated & returned to the client.
  std::vector<BitstreamBuffer> available_bitstream_buffers_;

  // Monotonically-growing timestamp that will be assigned to the next frame
  base::TimeDelta next_timestamp_;

  // Map from artificial monotonically-growing to real frame timestamp.
  base::flat_map<base::TimeDelta, FrameTimestampInfo>
      generated_to_real_timestamp_map_;

  std::unique_ptr<MediaLog> log_;

  // SPS and PPS NALs etc.
  std::vector<uint8_t> config_data_;

  // Required for encoders which are missing stride information.
  std::optional<gfx::Size> aligned_size_;

  // Currently configured color space.
  std::optional<gfx::ColorSpace> encoder_color_space_;

  // Pending color space to be set on the MediaCodec after flushing.
  std::optional<gfx::ColorSpace> pending_color_space_;

  // Number of layers for temporal scalable encoding
  int num_temporal_layers_ = 1;

  // Counter of inputs which is used to assign temporal layer indexes
  // according to the corresponding layer pattern. Reset for every key frame.
  uint32_t input_since_keyframe_count_ = 0;

  // This helper is used for parsing bitstream and assign SVC metadata.
  std::unique_ptr<TemporalScalabilityIdExtractor> svc_parser_;

  // True if any frames have been sent to the encoder.
  bool have_encoded_frames_ = false;

  // Fields related to sending frame to the encoder via the Surface instead of
  // input buffers.
  const bool use_surface_as_input_;
  std::unique_ptr<VideoFrameGLSurfaceRenderer> gl_renderer_;
  gl::ScopedANativeWindow input_surface_;

  media::VideoEncoderInfo encoder_info_;

  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  scoped_refptr<CommandBufferHelper> command_buffer_helper_;

  std::unique_ptr<VEAEncodingLatencyMetricsHelper> metrics_helper_;

  base::WeakPtrFactory<NdkVideoEncodeAccelerator> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_NDK_VIDEO_ENCODE_ACCELERATOR_H_
