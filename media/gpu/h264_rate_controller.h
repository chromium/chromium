// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_H264_RATE_CONTROLLER_H_
#define MEDIA_GPU_H264_RATE_CONTROLLER_H_

#include <vector>

#include "base/moving_window.h"
#include "media/gpu/frame_size_estimator.h"
#include "media/gpu/hrd_buffer.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

// H264RateController class implements a rate control algorithm for H.264 video
// encoder. The algorithm adjusts the QP for each frame, aiming to keep the
// video stream bitrate close to the target bitrate. The controller supports
// up to two temporal layers, each with its own HRD buffer. The HRD buffer
// stores the encoded frames from the current layer and all the lower layers
// that it depens on.
// The prediction of the QP parameter for intra encoded frames is based on the
// R-D model, using the expected size of the encoded frame as an input.
// For inter encoded frames, the QP is calculated based on the long-term and
// short-term statistics of the estamated QP and frame size, the prediction
// error of the frame size prediction for the previously encoded frames,
// and the HRD buffer fullness. (The algorithm doesn't support prediction for
// the inter encoded frames in the current implementation. This functionality
// will be provided in the next CL.)
class MEDIA_GPU_EXPORT H264RateController {
 public:
  class MEDIA_GPU_EXPORT Layer {
   public:
    struct MEDIA_GPU_EXPORT LayerSettings {
      LayerSettings() = default;
      ~LayerSettings() = default;

      LayerSettings(const LayerSettings&) = default;
      LayerSettings& operator=(const LayerSettings&) = default;

      // Average bitrate of the layer in bits per second. The bitrate includes
      // the bits from all lower layers.
      uint32_t avg_bitrate = 0u;

      // Peak transmission rate in bits per second.
      uint32_t peak_bitrate = 0u;

      // HRD buffer size in bytes.
      size_t hrd_buffer_size = 0u;

      // Minimum QP for the layer.
      int min_qp = 0;

      // Maximum QP for the layer.
      int max_qp = 0;

      // Layer frame rate.
      float frame_rate = 0.0f;
    };

    Layer(LayerSettings settings, float expected_fps);
    ~Layer();

    Layer(const Layer&) = delete;
    Layer& operator=(const Layer&) = delete;

    // Shrinks HRD buffer according to the current frame timestamp.
    void ShrinkHRDBuffer(base::TimeDelta timestamp);

    // Adds the size of the encoded frame to the HRD buffer.
    void AddFrameBytes(size_t frame_bytes, base::TimeDelta frame_timestamp);

    // Adds the timestamp of the encoded frame to the frame rate estimator.
    void AddFrameTimestamp(base::TimeDelta frame_timestamp);

    // Reconfigures the HRD buffer with the new parameters.
    void SetBufferParameters(size_t buffer_size,
                             uint32_t avg_bitrate,
                             uint32_t peak_bitrate,
                             bool ease_hrd_reduction);

    // Returns the HRD buffer fullness at the specified time.
    size_t GetBufferBytesAtTime(base::TimeDelta timestamp) const;

    // Returns the remaining space in HRD buffer at the given time.
    size_t GetBufferBytesRemainingAtTime(base::TimeDelta timestamp) const;

    // Returns the mean frame rate.
    float GetFrameRateMean() const;

    // Returns true if the HRD buffer for the temporal layer is full.
    bool is_buffer_full() const { return hrd_buffer_.frame_overshooting(); }

    // Returns the current HRD buffer size.
    size_t buffer_size() const { return hrd_buffer_.buffer_size(); }

    // Returns the current HRD buffer size.
    size_t average_bitrate() const { return hrd_buffer_.average_bitrate(); }

    // The size of the last encoded frame.
    size_t last_frame_buffer_bytes() const {
      return hrd_buffer_.last_frame_buffer_bytes();
    }

   private:
    HRDBuffer hrd_buffer_;
    base::MovingMinMax<base::TimeDelta> src_frame_rate_;
    LayerSettings settings_;
    float expected_fps_;
  };

  struct MEDIA_GPU_EXPORT ControllerSettings {
    ControllerSettings();
    ~ControllerSettings();

    ControllerSettings(const ControllerSettings&);
    ControllerSettings& operator=(const ControllerSettings&);

    // Frame size.
    gfx::Size frame_size;

    // Fixed delta QP between layers.
    bool fixed_delta_qp = false;

    // Maximum source frame rate.
    float frame_rate_max = 0.0f;

    // Number of temporal layers.
    size_t num_temporal_layers = 0u;

    // Content type of the video source.
    VideoEncodeAccelerator::Config::ContentType content_type =
        VideoEncodeAccelerator::Config::ContentType::kCamera;

    // Layer settings for each temporal layer.
    std::vector<Layer::LayerSettings> layers;
  };

  explicit H264RateController(ControllerSettings settings);
  ~H264RateController();

  H264RateController(const H264RateController& other) = delete;
  H264RateController& operator=(const H264RateController& other) = delete;

  // The method estimates the QP parameter for the next intra encoded frame
  // based on the current buffer fullness. It uses a rate-distortion model
  // that assumes the following:
  //
  // - q_step - Quantizer step size
  //   q_step = 5 / 8 * 2^(qp / 6)
  //
  // - mad is the Mean Absolute Difference of the residuals in intra frame
  //   prediction. Since this value cannot be retrieved from the Media
  //   Foundation system, it is approximated by a constant value calculated for
  //   the average frame content complexity.
  //
  // - bpp - Bits per pixel
  //   bpp = frame_size_in_bits / (frame_width * frame_height)
  //
  // We assume that the binary logarithm of the bits per pixel value is linearly
  // dependent on the binary logarithm of the ratio between MAD and Q step.
  //
  // log2(bpp) = a * log2(mad / q_step) + b
  //
  // When a = 2^b, bpp can expressed as
  //
  // bpp = a * (mad / q_step)^m, and q_step is
  //
  // q_step = mad / ( (bpp/a)^(1/m) )
  uint32_t EstimateIntraFrameQP(base::TimeDelta picture_timestamp) const;

  // Returns a temporal layer referenced by the index.
  Layer& temporal_layers(size_t index) {
    CHECK_LT(index, temporal_layers_.size());
    return *temporal_layers_[index];
  }

 private:
  // Returns the target bytes for the intra encoded frame used for the
  // estimation of the QP value. The calculation of the target bytes is based on
  // the remaining HRD buffer size and the available budget per frame.
  size_t GetTargetBytesForIntraFrame(base::TimeDelta picture_timestamp) const;

  ControllerSettings settings_;
  std::vector<std::unique_ptr<Layer>> temporal_layers_;
};

}  // namespace media

#endif  // MEDIA_GPU_H264_RATE_CONTROLLER_H_
