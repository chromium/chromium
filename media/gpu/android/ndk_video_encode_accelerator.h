// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_NDK_VIDEO_ENCODE_ACCELERATOR_H_
#define MEDIA_GPU_ANDROID_NDK_VIDEO_ENCODE_ACCELERATOR_H_
#include <stddef.h>
#include <stdint.h>

#include <media/NdkMediaCodec.h>
#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "media/base/bitrate.h"
#include "media/base/media_log.h"
#include "media/base/video_encoder.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

class BitstreamBuffer;
struct AMediaCodecDeleter {
  inline void operator()(AMediaCodec* ptr) const {
    if (ptr)
      AMediaCodec_delete(ptr);
  }
};

class MEDIA_GPU_EXPORT NdkVideoEncodeAccelerator final
    : public VideoEncodeAccelerator {
 public:
  // |runner| - a task runner that will be used for all callbacks and external
  // calls to this instance.
  NdkVideoEncodeAccelerator(scoped_refptr<base::SequencedTaskRunner> runner);

  NdkVideoEncodeAccelerator(const NdkVideoEncodeAccelerator&) = delete;
  NdkVideoEncodeAccelerator& operator=(const NdkVideoEncodeAccelerator&) =
      delete;
  ~NdkVideoEncodeAccelerator() override;

  static bool IsSupported();

  // VideoEncodeAccelerator implementation.
  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles() override;
  bool Initialize(const Config& config,
                  Client* client,
                  std::unique_ptr<MediaLog> media_log) override;
  void Encode(scoped_refptr<VideoFrame> frame, bool force_keyframe) override;
  void UseOutputBitstreamBuffer(BitstreamBuffer buffer) override;
  void RequestEncodingParametersChange(const Bitrate& bitrate,
                                       uint32_t framerate) override;
  void Destroy() override;

 private:
  // Called by MediaCodec when an input buffer becomes available.
  static void OnAsyncInputAvailable(AMediaCodec* codec,
                                    void* userdata,
                                    int32_t index);
  void OnInputAvailable(int32_t index);

  // Called by MediaCodec when an output buffer becomes available.
  static void OnAsyncOutputAvailable(AMediaCodec* codec,
                                     void* userdata,
                                     int32_t index,
                                     AMediaCodecBufferInfo* bufferInfo);
  void OnOutputAvailable(int32_t index, AMediaCodecBufferInfo bufferInfo);

  // Called by MediaCodec when the output format has changed.
  static void OnAsyncFormatChanged(AMediaCodec* codec,
                                   void* userdata,
                                   AMediaFormat* format) {}

  // Called when the MediaCodec encountered an error.
  static void OnAsyncError(AMediaCodec* codec,
                           void* userdata,
                           media_status_t error,
                           int32_t actionCode,
                           const char* detail);

  // Ask MediaCodec what input buffer layout it prefers and set values of
  // |input_buffer_stride_| and |input_buffer_yplane_height_|. If the codec
  // does not provide these values, sets up |aligned_size_| such that encoded
  // frames are cropped to the nearest 16x16 alignment.
  bool SetInputBufferLayout(const gfx::Size& configured_size);

  // Read a frame from |pending_frames_| put it into an input buffer
  // available in |media_codec_input_buffers_| and ask |media_codec_| to encode
  // it.
  void FeedInput();

  // Read encoded data from |media_codec_output_buffers_| copy it to a buffer
  // available in |available_bitstream_buffers_| and tell |client_ptr_factory_|
  // that encoded data is ready.
  void DrainOutput();

  // Read config data from |media_codec_output_buffers_| and copy it to
  // |config_data_|. |config_data_| is later propagated to key-frame encoded
  // chunks.
  bool DrainConfig();

  void NotifyMediaCodecError(std::string message, media_status_t status);
  void NotifyError(base::StringPiece message, Error code);

  base::TimeDelta AssignMonotonicTimestamp(base::TimeDelta real_timestamp);
  base::TimeDelta RetrieveRealTimestamp(base::TimeDelta monotonic_timestamp);

  SEQUENCE_CHECKER(sequence_checker_);

  // VideoDecodeAccelerator::Client callbacks go here.  Invalidated once any
  // error triggers.
  std::unique_ptr<base::WeakPtrFactory<Client>> client_ptr_factory_;

  using MediaCodecPtr = std::unique_ptr<AMediaCodec, AMediaCodecDeleter>;

  MediaCodecPtr media_codec_;

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

  // Indices of input buffers currently pending in media codec.
  base::circular_deque<size_t> media_codec_input_buffers_;

  // Info about output buffers currently pending in media codec.
  struct MCOutput {
    int32_t buffer_index;
    AMediaCodecBufferInfo info;
  };
  base::circular_deque<MCOutput> media_codec_output_buffers_;

  // Frames waiting to be passed to the codec, queued until an input buffer is
  // available.
  base::circular_deque<VideoEncoder::PendingEncode> pending_frames_;

  // Bitstream buffers waiting to be populated & returned to the client.
  std::vector<BitstreamBuffer> available_bitstream_buffers_;

  // Monotonically-growing timestamp that will be assigned to the next frame
  base::TimeDelta next_timestamp_;

  // Map from artificial monotonically-growing to real frame timestamp.
  base::flat_map<base::TimeDelta, base::TimeDelta>
      generated_to_real_timestamp_map_;

  std::unique_ptr<MediaLog> log_;

  // SPS and PPS NALs etc.
  std::vector<uint8_t> config_data_;

  // Required for encoders which are missing stride information.
  absl::optional<gfx::Size> aligned_size_;

  // Declared last to ensure that all weak pointers are invalidated before
  // other destructors run.
  base::WeakPtr<NdkVideoEncodeAccelerator> callback_weak_ptr_;
  base::WeakPtrFactory<NdkVideoEncodeAccelerator> callback_weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_NDK_VIDEO_ENCODE_ACCELERATOR_H_
