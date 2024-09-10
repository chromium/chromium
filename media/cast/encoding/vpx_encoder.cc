// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/cast/encoding/vpx_encoder.h"

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/base/video_encoder_metrics_provider.h"
#include "media/base/video_frame.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/constants.h"
#include "media/cast/encoding/encoding_util.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8cx.h"
#include "third_party/openscreen/src/cast/streaming/public/encoded_frame.h"

using Dependency = openscreen::cast::EncodedFrame::Dependency;

namespace media {
namespace cast {

namespace {

// After a pause in the video stream, what is the maximum duration amount to
// pass to the encoder for the next frame (in terms of 1/max_fps sized periods)?
// This essentially controls the encoded size of the first frame that follows a
// pause in the video stream.
const int kRestartFramePeriods = 3;

// The following constants are used to automactically tune the encoder
// parameters: |cpu_used| and |min_quantizer|.

// The |half-life| of the encoding speed accumulator.
// The smaller, the shorter of the time averaging window.
const int kEncodingSpeedAccHalfLife = 120000;  // 0.12 second.

// The target encoder utilization signal. This is a trade-off between quality
// and less CPU usage. The range of this value is [0, 1]. Higher the value,
// better the quality and higher the CPU usage.
//
// For machines with more than two encoding threads.
const double kHiTargetEncoderUtilization = 0.7;
// For machines with two encoding threads.
const double kMidTargetEncoderUtilization = 0.6;
// For machines with single encoding thread.
const double kLoTargetEncoderUtilization = 0.5;

// This is the equivalent change on encoding speed for the change on each
// quantizer step.
const double kEquivalentEncodingSpeedStepPerQpStep = 1 / 20.0;

// Highest/lowest allowed encoding speed set to the encoder. The valid range
// is [4, 16]. Experiments show that with speed higher than 12, the saving of
// the encoding time is not worth the dropping of the quality. And with speed
// lower than 6, the increasing of quality is not worth the increasing of
// encoding time.
const int kHighestEncodingSpeed = 12;
const int kLowestEncodingSpeed = 6;

bool HasSufficientFeedback(
    const FeedbackSignalAccumulator<base::TimeDelta>& accumulator) {
  const base::TimeDelta amount_of_history =
      accumulator.update_time() - accumulator.reset_time();
  return amount_of_history.InMicroseconds() >= 250000;  // 0.25 second.
}

}  // namespace

VpxEncoder::VpxEncoder(
    const FrameSenderConfig& video_config,
    std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider)
    : cast_config_(video_config),
      codec_params_(cast_config_.video_codec_params.value()),
      target_encoder_utilization_(
          codec_params_->number_of_encode_threads > 2
              ? kHiTargetEncoderUtilization
              : (codec_params_->number_of_encode_threads > 1
                     ? kMidTargetEncoderUtilization
                     : kLoTargetEncoderUtilization)),
      metrics_provider_(std::move(metrics_provider)),
      key_frame_requested_(true),
      bitrate_kbit_(cast_config_.start_bitrate / 1000),
      next_frame_id_(FrameId::first()),
      encoding_speed_acc_(base::Microseconds(kEncodingSpeedAccHalfLife)),
      encoding_speed_(kHighestEncodingSpeed) {
  config_.g_timebase.den = 0;  // Not initialized.
  DCHECK_LE(codec_params_->min_qp, codec_params_->max_cpu_saver_qp);
  DCHECK_LE(codec_params_->max_cpu_saver_qp, codec_params_->max_qp);

  DETACH_FROM_THREAD(thread_checker_);
}

VpxEncoder::~VpxEncoder() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_initialized())
    vpx_codec_destroy(&encoder_);
}

void VpxEncoder::Initialize() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_initialized());
  // The encoder will be created/configured when the first frame encode is
  // requested.
}

void VpxEncoder::ConfigureForNewFrameSize(const gfx::Size& frame_size) {
  if (is_initialized()) {
    // NOTE: Do we need this workaround for VP9?
    // Workaround for VP8 bug: If the new size is strictly less-than-or-equal to
    // the old size, in terms of area, the existing encoder instance can
    // continue.  Otherwise, completely tear-down and re-create a new encoder to
    // avoid a shutdown crash.
    if (frame_size.GetArea() <= gfx::Size(config_.g_w, config_.g_h).GetArea()) {
      DVLOG(1) << "Continuing to use existing encoder at smaller frame size: "
               << gfx::Size(config_.g_w, config_.g_h).ToString() << " --> "
               << frame_size.ToString();
      config_.g_w = frame_size.width();
      config_.g_h = frame_size.height();
      config_.rc_min_quantizer = codec_params_->min_qp;
      if (vpx_codec_enc_config_set(&encoder_, &config_) == VPX_CODEC_OK)
        return;
      DVLOG(1) << "libvpx rejected the attempt to use a smaller frame size in "
                  "the current instance.";
    }

    DVLOG(1) << "Destroying/Re-Creating encoder for larger frame size: "
             << gfx::Size(config_.g_w, config_.g_h).ToString() << " --> "
             << frame_size.ToString();
    vpx_codec_destroy(&encoder_);
  } else {
    DVLOG(1) << "Creating encoder for the first frame; size: "
             << frame_size.ToString();
  }

  // Determine appropriate codec interface.
  vpx_codec_iface_t* ctx;
  if (codec_params_->codec == VideoCodec::kVP9) {
    ctx = vpx_codec_vp9_cx();
  } else {
    DCHECK(codec_params_->codec == VideoCodec::kVP8);
    ctx = vpx_codec_vp8_cx();
  }

  // Populate encoder configuration with default values.
  CHECK_EQ(vpx_codec_enc_config_default(ctx, &config_, 0), VPX_CODEC_OK);

  config_.g_threads = codec_params_->number_of_encode_threads;
  config_.g_w = frame_size.width();
  config_.g_h = frame_size.height();
  // Set the timebase to match that of base::TimeDelta.
  config_.g_timebase.num = 1;
  config_.g_timebase.den = base::Time::kMicrosecondsPerSecond;

  // |g_pass| and |g_lag_in_frames| must be "one pass" and zero, respectively,
  // in order for VPX to support changing frame sizes during encoding:
  config_.g_pass = VPX_RC_ONE_PASS;
  config_.g_lag_in_frames = 0;  // Immediate data output for each frame.

  // Rate control settings.
  config_.rc_dropframe_thresh = GetEncoderDropFrameThreshold();
  config_.rc_resize_allowed = 0;
  config_.rc_end_usage = VPX_CBR;
  config_.rc_target_bitrate = bitrate_kbit_;
  config_.rc_min_quantizer = codec_params_->min_qp;
  config_.rc_max_quantizer = codec_params_->max_qp;
  config_.rc_undershoot_pct = 100;
  config_.rc_overshoot_pct = 15;
  config_.rc_buf_initial_sz = 500;
  config_.rc_buf_optimal_sz = 600;
  config_.rc_buf_sz = 1000;

  config_.kf_mode = VPX_KF_DISABLED;

  vpx_codec_flags_t flags = 0;
  metrics_provider_->Initialize(codec_params_->codec == VideoCodec::kVP9
                                    ? media::VP9PROFILE_MIN
                                    : media::VP8PROFILE_ANY,
                                frame_size, /*is_hardware_encoder=*/false);
  if (vpx_codec_err_t ret = vpx_codec_enc_init(&encoder_, ctx, &config_, flags);
      ret != VPX_CODEC_OK) {
    metrics_provider_->SetError(
        {media::EncoderStatus::Codes::kEncoderInitializationError,
         base::StrCat(
             {"libvpx failed to initialize: ", vpx_codec_err_to_string(ret)})});
  }

  // Raise the threshold for considering macroblocks as static.  The default is
  // zero, so this setting makes the encoder less sensitive to motion.  This
  // lowers the probability of needing to utilize more CPU to search for motion
  // vectors. The value is the same as WebRTC.
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/codecs/vp8/libvpx_vp8_encoder.cc
  CHECK_EQ(vpx_codec_control(&encoder_, VP8E_SET_STATIC_THRESHOLD, 100),
           VPX_CODEC_OK);

  if (codec_params_->codec == VideoCodec::kVP9) {
    CHECK_EQ(vpx_codec_control(&encoder_, VP9E_SET_TUNE_CONTENT,
                               VP9E_CONTENT_SCREEN),
             VPX_CODEC_OK);
  } else {
    // A frame may be dropped by the encoder if VP8E_SET_SCREEN_CONTENT_MODE is
    // configured to 2 ("On with more aggressive rate control"). A frame is
    // never dropped if it is configured to 1 ("On").
    const unsigned int screen_content_mode =
        base::FeatureList::IsEnabled(kCastVideoEncoderFrameDrop) ? 2 : 1;
    CHECK_EQ(vpx_codec_control(&encoder_, VP8E_SET_SCREEN_CONTENT_MODE,
                               screen_content_mode),
             VPX_CODEC_OK);
  }

  // This cpu_used setting is a trade-off between cpu usage and encoded video
  // quality. The default is zero, with increasingly less CPU to be used as the
  // value is more negative or more positive. The encoder does some automatic
  // adjust on encoding speed for positive values, however at least at this
  // stage the experiments show that this automatic behaviour is not reliable on
  // windows machines. We choose to set negative values instead to directly set
  // the encoding speed to the encoder. Starting with the highest encoding speed
  // to avoid large cpu usage from the beginning.
  encoding_speed_ = kHighestEncodingSpeed;
  CHECK_EQ(vpx_codec_control(&encoder_, VP8E_SET_CPUUSED, -encoding_speed_),
           VPX_CODEC_OK);
}

void VpxEncoder::Encode(scoped_refptr<media::VideoFrame> video_frame,
                        base::TimeTicks reference_time,
                        SenderEncodedFrame* encoded_frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(encoded_frame);

  // Note: This is used to compute the |encoder_utilization| and so it uses the
  // real-world clock instead of the CastEnvironment clock, the latter of which
  // might be simulated.
  const base::TimeTicks start_time = base::TimeTicks::Now();

  // Initialize on-demand.  Later, if the video frame size has changed, update
  // the encoder configuration.
  const gfx::Size frame_size = video_frame->visible_rect().size();
  if (!is_initialized() || gfx::Size(config_.g_w, config_.g_h) != frame_size)
    ConfigureForNewFrameSize(frame_size);

  // Wrapper for vpx_codec_encode() to access the YUV data in the |video_frame|.
  // Only the VISIBLE rectangle within |video_frame| is exposed to the codec.
  vpx_img_fmt_t vpx_format = video_frame->format() == PIXEL_FORMAT_NV12
                                 ? VPX_IMG_FMT_NV12
                                 : VPX_IMG_FMT_I420;
  vpx_image_t vpx_image;
  vpx_image_t* const result = vpx_img_wrap(
      &vpx_image, vpx_format, frame_size.width(), frame_size.height(), 1,
      const_cast<uint8_t*>(video_frame->visible_data(VideoFrame::Plane::kY)));
  DCHECK_EQ(result, &vpx_image);
  switch (vpx_format) {
    case VPX_IMG_FMT_I420:
      vpx_image.planes[VPX_PLANE_Y] = const_cast<uint8_t*>(
          video_frame->visible_data(VideoFrame::Plane::kY));
      vpx_image.planes[VPX_PLANE_U] = const_cast<uint8_t*>(
          video_frame->visible_data(VideoFrame::Plane::kU));
      vpx_image.planes[VPX_PLANE_V] = const_cast<uint8_t*>(
          video_frame->visible_data(VideoFrame::Plane::kV));
      vpx_image.stride[VPX_PLANE_Y] =
          video_frame->stride(VideoFrame::Plane::kY);
      vpx_image.stride[VPX_PLANE_U] =
          video_frame->stride(VideoFrame::Plane::kU);
      vpx_image.stride[VPX_PLANE_V] =
          video_frame->stride(VideoFrame::Plane::kV);
      break;
    case VPX_IMG_FMT_NV12:
      vpx_image.planes[VPX_PLANE_Y] = const_cast<uint8_t*>(
          video_frame->visible_data(VideoFrame::Plane::kY));
      // In libvpx, the UV plane of NV12 frames is represented by two planes
      // with the same stride, shifted by one byte.
      vpx_image.planes[VPX_PLANE_U] = const_cast<uint8_t*>(
          video_frame->visible_data(VideoFrame::Plane::kUV));
      vpx_image.planes[VPX_PLANE_V] = vpx_image.planes[VPX_PLANE_U] + 1;
      vpx_image.stride[VPX_PLANE_Y] =
          video_frame->stride(VideoFrame::Plane::kY);
      vpx_image.stride[VPX_PLANE_U] =
          video_frame->stride(VideoFrame::Plane::kUV);
      vpx_image.stride[VPX_PLANE_V] =
          video_frame->stride(VideoFrame::Plane::kUV);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  // The frame duration given to the VPX codecs affects a number of important
  // behaviors, including: per-frame bandwidth, CPU time spent encoding,
  // temporal quality trade-offs, and key/golden/alt-ref frame generation
  // intervals.  Bound the prediction to account for the fact that the frame
  // rate can be highly variable, including long pauses in the video stream.
  const base::TimeDelta minimum_frame_duration =
      base::Seconds(1.0 / cast_config_.max_frame_rate);
  const base::TimeDelta maximum_frame_duration = base::Seconds(
      static_cast<double>(kRestartFramePeriods) / cast_config_.max_frame_rate);
  base::TimeDelta predicted_frame_duration =
      video_frame->metadata().frame_duration.value_or(base::TimeDelta());
  if (predicted_frame_duration <= base::TimeDelta()) {
    // The source of the video frame did not provide the frame duration.  Use
    // the actual amount of time between the current and previous frame as a
    // prediction for the next frame's duration.
    predicted_frame_duration = video_frame->timestamp() - last_frame_timestamp_;
  }
  predicted_frame_duration =
      std::max(minimum_frame_duration,
               std::min(maximum_frame_duration, predicted_frame_duration));
  last_frame_timestamp_ = video_frame->timestamp();

  // Encode the frame.  The presentation time stamp argument here is fixed to
  // zero to force the encoder to base its single-frame bandwidth calculations
  // entirely on |predicted_frame_duration| and the target bitrate setting being
  // micro-managed via calls to UpdateRates().
  if (vpx_codec_err_t ret = vpx_codec_encode(
          &encoder_, &vpx_image, 0, predicted_frame_duration.InMicroseconds(),
          key_frame_requested_ ? VPX_EFLAG_FORCE_KF : 0, VPX_DL_REALTIME);
      ret != VPX_CODEC_OK) {
    metrics_provider_->SetError(
        {media::EncoderStatus::Codes::kEncoderFailedEncode,
         base::StrCat(
             {"libvpx failed to encode: ", vpx_codec_err_to_string(ret), " - ",
              vpx_codec_error_detail(&encoder_)})});
    LOG(FATAL) << "BUG: Invalid arguments passed to vpx_codec_encode().";
  }

  // Pull data from the encoder, populating a new EncodedFrame.
  encoded_frame->frame_id = next_frame_id_;
  const vpx_codec_cx_pkt_t* pkt = NULL;
  vpx_codec_iter_t iter = NULL;
  while ((pkt = vpx_codec_get_cx_data(&encoder_, &iter)) != NULL) {
    if (pkt->kind != VPX_CODEC_CX_FRAME_PKT)
      continue;
    if (pkt->data.frame.flags & VPX_FRAME_IS_KEY) {
      // TODO(hubbe): Replace "dependency" with a "bool is_key_frame".
      encoded_frame->dependency = Dependency::kKeyFrame;
      encoded_frame->referenced_frame_id = encoded_frame->frame_id;
    } else {
      encoded_frame->dependency = Dependency::kDependent;
      // Frame dependencies could theoretically be relaxed by looking for the
      // VPX_FRAME_IS_DROPPABLE flag, but in recent testing (Oct 2014), this
      // flag never seems to be set.
      encoded_frame->referenced_frame_id = encoded_frame->frame_id - 1;
    }
    encoded_frame->rtp_timestamp =
        ToRtpTimeTicks(video_frame->timestamp(), kVideoFrequency);
    encoded_frame->reference_time = reference_time;
    encoded_frame->data.assign(
        static_cast<const uint8_t*>(pkt->data.frame.buf),
        static_cast<const uint8_t*>(pkt->data.frame.buf) + pkt->data.frame.sz);
    break;  // Done, since all data is provided in one CX_FRAME_PKT packet.
  }
  if (encoded_frame->data.empty()) {
    // Drop frame.
    return;
  }
  // Increment frame id only if the frame is encoded.
  next_frame_id_++;
  metrics_provider_->IncrementEncodedFrameCount();

  // Compute encoder utilization as the real-world time elapsed divided by the
  // frame duration.
  const base::TimeDelta processing_time = base::TimeTicks::Now() - start_time;
  encoded_frame->encoder_utilization =
      processing_time / predicted_frame_duration;

  // Compute lossy utilization.  The VPX encoder took an estimated guess at what
  // quantizer value would produce an encoded frame size as close to the target
  // as possible.  Now that the frame has been encoded and the number of bytes
  // is known, the perfect quantizer value (i.e., the one that should have been
  // used) can be determined.  This perfect quantizer is then normalized and
  // used as the lossy utilization.
  const double actual_bitrate =
      encoded_frame->data.size() * 8.0 / predicted_frame_duration.InSecondsF();
  encoded_frame->encoder_bitrate = actual_bitrate;
  const double target_bitrate = 1000.0 * config_.rc_target_bitrate;
  DCHECK_GT(target_bitrate, 0.0);
  const double bitrate_utilization = actual_bitrate / target_bitrate;
  int quantizer = -1;
  CHECK_EQ(vpx_codec_control(&encoder_, VP8E_GET_LAST_QUANTIZER_64, &quantizer),
           VPX_CODEC_OK);
  const double perfect_quantizer = bitrate_utilization * std::max(0, quantizer);
  // Side note: If it was possible for the encoder to encode within the target
  // number of bytes, the |perfect_quantizer| will be in the range [0.0,63.0].
  // If it was never possible, the value will be greater than 63.0.
  encoded_frame->lossiness = perfect_quantizer / 63.0;

  DVLOG(2) << "VPX encoded frame_id " << encoded_frame->frame_id
           << ", sized: " << encoded_frame->data.size()
           << ", encoder_utilization: " << encoded_frame->encoder_utilization
           << ", lossiness: " << encoded_frame->lossiness
           << " (quantizer chosen by the encoder was " << quantizer << ')';

  if (encoded_frame->dependency == Dependency::kKeyFrame) {
    key_frame_requested_ = false;
  }
  if (encoded_frame->dependency == Dependency::kKeyFrame) {
    encoding_speed_acc_.Reset(kHighestEncodingSpeed, video_frame->timestamp());
  } else {
    // Equivalent encoding speed considering both cpu_used setting and
    // quantizer.
    double actual_encoding_speed =
        encoding_speed_ + kEquivalentEncodingSpeedStepPerQpStep *
                              std::max(0, quantizer - codec_params_->min_qp);
    double adjusted_encoding_speed = actual_encoding_speed *
                                     encoded_frame->encoder_utilization /
                                     target_encoder_utilization_;
    encoding_speed_acc_.Update(adjusted_encoding_speed,
                               video_frame->timestamp());
  }

  if (HasSufficientFeedback(encoding_speed_acc_)) {
    // Predict |encoding_speed_| and |min_quantizer| for next frame.
    // When CPU is constrained, increase encoding speed and increase
    // |min_quantizer| if needed.
    double next_encoding_speed = encoding_speed_acc_.current();
    int next_min_qp;
    if (next_encoding_speed > kHighestEncodingSpeed) {
      double remainder = next_encoding_speed - kHighestEncodingSpeed;
      next_encoding_speed = kHighestEncodingSpeed;
      next_min_qp =
          static_cast<int>(remainder / kEquivalentEncodingSpeedStepPerQpStep +
                           codec_params_->min_qp + 0.5);
      next_min_qp = std::min(next_min_qp, codec_params_->max_cpu_saver_qp);
    } else {
      next_encoding_speed =
          std::max<double>(kLowestEncodingSpeed, next_encoding_speed) + 0.5;
      next_min_qp = codec_params_->min_qp;
    }
    if (encoding_speed_ != static_cast<int>(next_encoding_speed)) {
      encoding_speed_ = static_cast<int>(next_encoding_speed);
      CHECK_EQ(vpx_codec_control(&encoder_, VP8E_SET_CPUUSED, -encoding_speed_),
               VPX_CODEC_OK);
    }
    if (config_.rc_min_quantizer != static_cast<unsigned int>(next_min_qp)) {
      config_.rc_min_quantizer = static_cast<unsigned int>(next_min_qp);
      CHECK_EQ(vpx_codec_enc_config_set(&encoder_, &config_), VPX_CODEC_OK);
    }
  }
}

void VpxEncoder::UpdateRates(uint32_t new_bitrate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!is_initialized())
    return;

  uint32_t new_bitrate_kbit = new_bitrate / 1000;
  if (config_.rc_target_bitrate == new_bitrate_kbit)
    return;

  config_.rc_target_bitrate = bitrate_kbit_ = new_bitrate_kbit;

  // Update encoder context.
  if (vpx_codec_enc_config_set(&encoder_, &config_)) {
    NOTREACHED_IN_MIGRATION() << "Invalid return value";
  }

  VLOG(1) << "VPX new rc_target_bitrate: " << new_bitrate_kbit << " kbps";
}

void VpxEncoder::GenerateKeyFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  key_frame_requested_ = true;
}

}  // namespace cast
}  // namespace media
