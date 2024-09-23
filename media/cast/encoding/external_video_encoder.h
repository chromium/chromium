// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_ENCODING_EXTERNAL_VIDEO_ENCODER_H_
#define MEDIA_CAST_ENCODING_EXTERNAL_VIDEO_ENCODER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string_view>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "media/cast/cast_environment.h"
#include "media/cast/encoding/size_adaptable_video_encoder_base.h"
#include "media/cast/encoding/video_encoder.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class VideoEncoderMetricsProvider;

namespace cast {

// Cast MAIN thread proxy to the internal media::VideoEncodeAccelerator
// implementation running on a separate thread.  Encodes media::VideoFrames and
// emits media::cast::EncodedFrames.
class ExternalVideoEncoder final : public VideoEncoder {
 public:
  // Returns true if the current platform and system configuration supports
  // using ExternalVideoEncoder with the given |video_config|.
  static bool IsSupported(const FrameSenderConfig& video_config);

  ExternalVideoEncoder(
      const scoped_refptr<CastEnvironment>& cast_environment,
      const FrameSenderConfig& video_config,
      VideoEncoderMetricsProvider& metrics_provider,
      const gfx::Size& frame_size,
      FrameId first_frame_id,
      StatusChangeCallback status_change_cb,
      const CreateVideoEncodeAcceleratorCallback& create_vea_cb);

  ExternalVideoEncoder(const ExternalVideoEncoder&) = delete;
  ExternalVideoEncoder& operator=(const ExternalVideoEncoder&) = delete;

  ~ExternalVideoEncoder() final;

  // VideoEncoder implementation.
  bool EncodeVideoFrame(scoped_refptr<media::VideoFrame> video_frame,
                        base::TimeTicks reference_time,
                        FrameEncodedCallback frame_encoded_callback) final;
  void SetBitRate(int new_bit_rate) final;
  void GenerateKeyFrame() final;

 private:
  class VEAClientImpl;

  // Called from the destructor (or earlier on error), to schedule destruction
  // of |client_| via the encoder task runner.
  void DestroyClientSoon();

  void SetErrorToMetricsProvider(const media::EncoderStatus& encoder_status);

  // Method invoked by the CreateVideoEncodeAcceleratorCallback to construct a
  // VEAClientImpl to own and interface with a new |vea|.  Upon return,
  // |client_| holds a reference to the new VEAClientImpl.
  void OnCreateVideoEncodeAccelerator(
      const FrameSenderConfig& video_config,
      FrameId first_frame_id,
      const StatusChangeCallback& status_change_cb,
      scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner,
      std::unique_ptr<media::VideoEncodeAccelerator> vea);

  const scoped_refptr<CastEnvironment> cast_environment_;

  raw_ref<VideoEncoderMetricsProvider> metrics_provider_;

  // The size of the visible region of the video frames to be encoded.
  const gfx::Size frame_size_;

  int bit_rate_;
  bool key_frame_requested_ = false;

  scoped_refptr<VEAClientImpl> client_;

  // Provides a weak pointer for the OnCreateVideoEncoderAccelerator() callback.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<ExternalVideoEncoder> weak_factory_{this};
};

// An implementation of SizeAdaptableVideoEncoderBase to proxy for
// ExternalVideoEncoder instances.
class SizeAdaptableExternalVideoEncoder final
    : public SizeAdaptableVideoEncoderBase {
 public:
  SizeAdaptableExternalVideoEncoder(
      const scoped_refptr<CastEnvironment>& cast_environment,
      const FrameSenderConfig& video_config,
      std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider,
      StatusChangeCallback status_change_cb,
      const CreateVideoEncodeAcceleratorCallback& create_vea_cb);

  SizeAdaptableExternalVideoEncoder(const SizeAdaptableExternalVideoEncoder&) =
      delete;
  SizeAdaptableExternalVideoEncoder& operator=(
      const SizeAdaptableExternalVideoEncoder&) = delete;

  ~SizeAdaptableExternalVideoEncoder() final;

 protected:
  std::unique_ptr<VideoEncoder> CreateEncoder() final;

 private:
  // Special callbacks needed by media::cast::ExternalVideoEncoder.
  const CreateVideoEncodeAcceleratorCallback create_vea_cb_;
};

// A utility class for examining the sequence of frames sent to an external
// encoder, and returning an estimate of the what the software VP8 encoder would
// have used for a quantizer value when encoding each frame.  The quantizer
// value is related to the complexity of the content of the frame.
class QuantizerEstimator {
 public:
  static constexpr int NO_RESULT = -1;
  static constexpr int MIN_VPX_QUANTIZER = 4;
  static constexpr int MAX_VPX_QUANTIZER = 63;

  QuantizerEstimator();

  QuantizerEstimator(const QuantizerEstimator&) = delete;
  QuantizerEstimator& operator=(const QuantizerEstimator&) = delete;

  ~QuantizerEstimator();

  // Discard any state related to the processing of prior frames.
  void Reset();

  // Examine |frame| and estimate and return the quantizer value the software
  // VP8 encoder would have used when encoding the frame, in the range
  // [4.0,63.0].  If |frame| is not in planar YUV format, or its size is empty,
  // this returns |NO_RESULT|.
  double EstimateForKeyFrame(const VideoFrame& frame);
  double EstimateForDeltaFrame(const VideoFrame& frame);

 private:
  // Returns true if the frame is in planar YUV format.
  static bool CanExamineFrame(const VideoFrame& frame);

  // Returns a value in the range [0,log2(num_buckets)], the Shannon Entropy
  // based on the probabilities of values falling within each of the buckets of
  // the given |histogram|.
  static double ComputeEntropyFromHistogram(const int* histogram,
                                            size_t histogram_size,
                                            int num_samples);

  // Map the |shannon_entropy| to its corresponding software VP8 quantizer.
  static double ToQuantizerEstimate(double shannon_entropy);

  // A cache of a subset of rows of pixels from the last frame examined.  This
  // is used to compute the entropy of the difference between frames, which in
  // turn is used to compute the entropy and quantizer.
  std::unique_ptr<uint8_t[]> last_frame_pixel_buffer_;
  gfx::Size last_frame_size_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_ENCODING_EXTERNAL_VIDEO_ENCODER_H_
