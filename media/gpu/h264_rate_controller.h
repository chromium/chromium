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
//
// The prediction of the QP parameter for intra encoded frames is based on the
// R-D model, using the expected size of the encoded frame as an input.
// For inter encoded frames, the QP is calculated based on the long-term and
// short-term statistics of the estamated QP and frame size, the prediction
// error of the frame size prediction for the previously encoded frames,
// and the HRD buffer fullness.
//
// The QP values used for encoding the inter prediced frames (P frames) are
// estimated from the statistics of the previous frames and the expected frame
// size. The estimation engine holds the short-term and long-term statistics for
// each temporal layer. The QP is further modified according to the HRD buffer
// fullness and the limits of the QP range. If rate controller is configured for
// the fixed delta QP between layers (Fixed Delta QP mode), the QP for the
// current layer is calculated by adding a constant value to the previous
// layer's QP.
class MEDIA_GPU_EXPORT H264RateController {
 public:
  enum class FrameSizeEstimatorType { kShortTerm, kLongTerm };

  enum class FrameType { kPFrame, kIFrame };

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

      // HRD buffer size in bytes.
      size_t hrd_buffer_size = 0u;

      // Minimum QP for the layer.
      uint32_t min_qp = 0;

      // Maximum QP for the layer.
      uint32_t max_qp = 0;

      // Layer frame rate.
      float frame_rate = 0.0f;
    };

    Layer(LayerSettings settings,
          float expected_fps,
          base::TimeDelta short_term_window_size,
          base::TimeDelta long_term_window_size);
    ~Layer();

    Layer(const Layer&) = delete;
    Layer& operator=(const Layer&) = delete;

    uint32_t curr_frame_qp() const { return curr_frame_qp_; }
    void update_curr_frame_qp(uint32_t qp) { curr_frame_qp_ = qp; }

    uint32_t last_frame_qp() const { return last_frame_qp_; }
    void update_last_frame_qp(uint32_t qp) { last_frame_qp_ = qp; }

    uint32_t long_term_qp() const { return long_term_qp_; }
    void update_long_term_qp(uint32_t qp) { long_term_qp_ = qp; }

    uint32_t min_qp() const { return min_qp_; }
    uint32_t max_qp() const { return max_qp_; }

    // Returns true if the HRD buffer for the temporal layer is full.
    bool is_buffer_full() const { return hrd_buffer_.frame_overshooting(); }

    // Returns the current HRD buffer size.
    size_t buffer_size() const { return hrd_buffer_.buffer_size(); }

    // Returns the current HRD buffer bitrate.
    uint32_t average_bitrate() const { return hrd_buffer_.average_bitrate(); }

    FrameType last_frame_type() const { return last_frame_type_; }
    void update_last_frame_type(FrameType frame_type) {
      last_frame_type_ = frame_type;
    }

    size_t last_frame_buffer_bytes() const {
      return hrd_buffer_.last_frame_buffer_bytes();
    }

    base::TimeDelta previous_frame_timestamp() const {
      return previous_frame_timestamp_;
    }
    void update_previous_frame_timestamp(base::TimeDelta timestamp) {
      previous_frame_timestamp_ = timestamp;
    }

    size_t pred_p_frame_size() const { return pred_p_frame_size_; }
    void update_pred_p_frame_size(size_t size) { pred_p_frame_size_ = size; }

    size_t last_frame_size_target_for_testing() const {
      return last_frame_size_target_;
    }
    void update_last_frame_size_target(size_t size) {
      last_frame_size_target_ = size;
    }

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

    // Estimates the expected frame size for the next P frame using the
    // short-term statistics from the preceding frames.
    size_t EstimateShortTermFrameSize(uint32_t qp, uint32_t qp_prev) const;

    // Returns the standard deviation of the estimated size error for the
    // previous frames. The filter window matches the size of the long-term
    // window.
    size_t GetFrameSizeEstimatorError() const;

    // Updates the estimators with the QP and actual encoded size of the current
    // frame.
    void UpdateFrameSizeEstimator(size_t frame_bytes,
                                  uint32_t qp,
                                  uint32_t qp_prev,
                                  base::TimeDelta elapsed_time);

   private:
    // Returns the initial size correction for the estimators.
    float GetInitialSizeCorrection(LayerSettings settings) const;

    // HRD buffer for the layer.
    HRDBuffer hrd_buffer_;

    // Moving min-max filter for the source frame rate estimation.
    base::MovingMinMax<base::TimeDelta> src_frame_rate_;

    // Expected frame rate for the layer.
    float expected_fps_;

    // Current frame QP.
    uint32_t curr_frame_qp_ = 0;

    // Last frame QP.
    uint32_t last_frame_qp_ = 0;

    // Estimated average QP for future frames.
    uint32_t long_term_qp_ = 0;

    // Minimum and maximum QPs for the layer.
    uint32_t min_qp_ = 0;
    uint32_t max_qp_ = 0;

    // Frame type of last non-dropped frame.
    FrameType last_frame_type_ = FrameType::kPFrame;

    // Timestamp of the previous frame.
    base::TimeDelta previous_frame_timestamp_ = base::Microseconds(-1);

    // Predicted frame size using current frame QP.
    size_t pred_p_frame_size_ = 0u;

    // Frame size estimators for short-term and long-term frame size prediction.
    FrameSizeEstimator short_term_estimator_;
    FrameSizeEstimator long_term_estimator_;

    // Predicted vs actual encoded frame size.
    ExponentialMovingAverage estimator_error_;

    // Target frame size for the next inter encoded frame. This value is stored
    // for the testing purposes.
    size_t last_frame_size_target_ = 0u;
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

    // Maximum GOP duration. 0 for infinite.
    base::TimeDelta gop_max_duration;

    // Content type of the video source.
    VideoEncodeAccelerator::Config::ContentType content_type =
        VideoEncodeAccelerator::Config::ContentType::kCamera;

    // Layer settings for each temporal layer.
    std::vector<Layer::LayerSettings> layer_settings;
  };

  explicit H264RateController(ControllerSettings settings);
  ~H264RateController();

  H264RateController(const H264RateController& other) = delete;
  H264RateController& operator=(const H264RateController& other) = delete;

  // Returns a temporal layer referenced by the index.
  Layer& temporal_layers(size_t index) {
    CHECK_LT(index, temporal_layers_.size());
    return *temporal_layers_[index];
  }

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
  void EstimateIntraFrameQP(base::TimeDelta frame_timestamp);

  // Estimates Quantization Parameter for inter encoded frames. The estimation
  // procedure has the following steps:
  //
  // 1. Calculate the target frame size for the current frame
  //   To calculate the target frame size, we first obtain the following
  //   parameters:
  //     - frame_size_long_term - the estimated frame size based on long-term
  //     parameters and short-term stats;
  //     - frame_size_budget - the average budget in the HRD buffer for one
  //     frame.
  //
  //    These expression are used to calculate the initial target frame size:
  //
  //    frame_size_deviation = abs(frame_size_long_term - frame_size_budget) /
  //                           frame_size_budget
  //
  //    frame_size_compress - the value is derived from frame_size_deviation by
  //    applying clamped linear interpolation from range [0.5, 3] to range
  //    [0.1, 0.9].
  //
  //    frame_size_target = frame_size_budget + (1 - frame_size_compress) *
  //        (frame_size_long_term - frame_size_budget)
  //
  //    The target frame size is corrected to keep the buffer fullness within
  //    predefined limits. The remaining time to the end of the GOP affects the
  //    correction, so that a smaller frame size is estimated when the IDR
  //    frame is about to occur soon. We use these calculations to obtain the
  //    correction:
  //
  //    buffer_target_low = frame_size_budget
  //    buffer_target_high = 0.2 * buffer_size
  //
  //    size_correction_window - remaining time to the end of the GOP is
  //    transformed from the range [0, 2000] to the range [200, 800]
  //
  //    frame_duration = 1 / frame_rate
  //
  //    In overflow case
  //        owerflow_size =
  //            frame_size_target + buffer_level_current - buffer_target_high
  //        size_correction =
  //            -overflow_size * frame_duration / size_correction_window
  //
  //    In underflow case
  //        underflow_size =
  //            buffer_target_low - (frame_size_target + buffer_level_current)
  //        size_correction =
  //            underflow_size * frame_duration / size_correction_window
  //
  //    frame_size_target += size_correction
  //
  //    In the final step, the instantaneous buffer undershoot and overshoot are
  //    prevented.
  //
  //    Undershoot prevention
  //    buf_level_pre_fill_next_frame is the buffer level just before the next
  //    frame is encoded.
  //    buf_level_pre_fill_next_frame =
  //        buffer_level_current + frame_size_target - frame_size_budget
  //    If the possibility for undershoot is detected, the frame size is
  //    frame_size_target +=
  //        frame_size_error - buf_level_pre_fill_next_frame
  //
  //    Overshoot prevention
  //    buf_level_post_fill is the buffer level right after adding the current
  //    frame to the buffer.
  //    buf_level_post_fill = buffer_level_current + frame_size_target
  //    If there is a possibility for overshoot, the frame size is
  //    frame_size_target -=
  //        buf_level_post_fill - buffer_size + frame_size_error
  //
  //    frame_size_error is obtained from the stats of the difference between
  //    the predicted and the actual frame size.
  //
  // 2. Estimate long-term QP based on stats.
  //
  // 3. Calculate the current QP from the target frame size and the
  //    long-term QP.
  //
  // 4. Clip the current QP to fulfill the HRD buffer fullness requirements.
  void EstimateInterFrameQP(size_t temporal_id,
                            base::TimeDelta frame_timestamp);

  // Adds the encoded bytes the HRD buffers and updates the layer data.
  void FinishIntraFrame(size_t access_unit_bytes,
                        base::TimeDelta frame_timestamp);

  // The encoded frame size is used to update the HRD buffers. Other layer data
  // is updated. The frame size estimators update their short-term and
  // long-term stats with the frame size. The frame size estimation error
  // is computed and added to the error stats.
  void FinishInterFrame(size_t temporal_id,
                        size_t access_unit_bytes,
                        base::TimeDelta frame_timestamp);

  // The array passed as a parameter stores the HRD buffer fullness for each
  // temporal layer as a percentage of the HRD buffer size.
  void GetHRDBufferFullness(base::span<int> buffer_fullness,
                            base::TimeDelta picture_timestamp) const;

 private:
  // The HRD buffers are updated with the encoded frame size. Last frame QP and
  // last frame type for the current layer are updated. The method updates all
  // HRD buffers for the layers that depend on the current layer.
  void FinishLayerData(size_t temporal_id,
                       size_t frame_bytes,
                       base::TimeDelta frame_timestamp);

  // Updates the timestamp of the previous frame for the current layer.
  void FinishLayerPreviousFrameTimestamp(size_t temporal_id,
                                         base::TimeDelta frame_timestamp);

  // The method calculates the target bytes for the intra encoded frame, which
  // are used to estimate the QP value. The target bytes depend on the remaining
  // HRD buffer size and the available budget per frame.
  size_t GetTargetBytesForIntraFrame(base::TimeDelta frame_timestamp) const;

  // Returns the target bytes for the next inter encoded frame. The target bytes
  // depend on the fullness of the HRD buffer, the average bitrate for the
  // layer, and the remaining time to the end of the GOP.
  size_t GetTargetBytesForInterFrame(size_t temporal_id,
                                     uint32_t max_rate_bytes_per_sec,
                                     size_t buffer_size,
                                     int buffer_level_current,
                                     base::TimeDelta frame_timestamp) const;

  // Returns target FPS extracted from layer settings.
  float GetTargetFps(ControllerSettings settings) const;

  // Temporal layers configured for the current video stream.
  std::vector<std::unique_ptr<Layer>> temporal_layers_;

  // FPS that the rate controller recommends.
  const float target_fps_;

  // Frame size of the video stream.
  const gfx::Size frame_size_;

  // Indicates whether the Fixed Delta QP mode is enabled.
  const bool fixed_delta_qp_;

  // Number of temporal layers.
  const size_t num_temporal_layers_;

  // Maximum duration of the Group of Pictures.
  const base::TimeDelta gop_max_duration_;

  // Video content type: camera or display.
  const VideoEncodeAccelerator::Config::ContentType content_type_;

  // Timestamp of the latest frame which overshoots buffer.
  base::TimeDelta last_idr_timestamp_;

  // Current frame number.
  int frame_number_ = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_H264_RATE_CONTROLLER_H_
