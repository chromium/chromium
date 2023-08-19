// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/webm_muxer.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#include "base/check.h"
#include "base/containers/circular_deque.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_math.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_parameters.h"
#include "media/base/limits.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/formats/common/opus_constants.h"
#include "media/muxers/muxer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/libwebm/source/mkvmuxer.hpp"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

namespace av1 {
// CodecPrivate for AV1. See
// https://github.com/ietf-wg-cellar/matroska-specification/blob/master/codec/av1.md.
constexpr int high_bitdepth = 0;
constexpr int twelve_bit = 0;
constexpr int monochrome = 0;
constexpr int initial_presentation_delay_present = 0;
constexpr int initial_presentation_delay_minus_one = 0;
constexpr int chroma_sample_position = 0;
constexpr int seq_profile = 0;      // Main
constexpr int seq_level_idx_0 = 9;  // level 4.1 ~1920x1080@60fps
constexpr int seq_tier_0 = 0;
constexpr int chroma_subsampling_x = 1;
constexpr int chroma_subsampling_y = 1;
constexpr uint8_t codec_private[4] = {
    255,                                         //
    (seq_profile << 5) | seq_level_idx_0,        //
    (seq_tier_0 << 7) |                          //
        (high_bitdepth << 6) |                   //
        (twelve_bit << 5) |                      //
        (monochrome << 4) |                      //
        (chroma_subsampling_x << 3) |            //
        (chroma_subsampling_y << 2) |            //
        chroma_sample_position,                  //
    (initial_presentation_delay_present << 4) |  //
        initial_presentation_delay_minus_one     //
};

}  // namespace av1

// Force new clusters at a maximum rate of 10 Hz.
constexpr base::TimeDelta kMinimumForcedClusterDuration =
    base::Milliseconds(100);

void WriteOpusHeader(const AudioParameters& params, uint8_t* header) {
  // See https://wiki.xiph.org/OggOpus#ID_Header.
  // Set magic signature.
  std::string label = "OpusHead";
  memcpy(header + OPUS_EXTRADATA_LABEL_OFFSET, label.c_str(), label.size());
  // Set Opus version.
  header[OPUS_EXTRADATA_VERSION_OFFSET] = 1;
  // Set channel count.
  DCHECK_LE(params.channels(), 2);
  header[OPUS_EXTRADATA_CHANNELS_OFFSET] = params.channels();
  // Set pre-skip
  uint16_t skip = 0;
  memcpy(header + OPUS_EXTRADATA_SKIP_SAMPLES_OFFSET, &skip, sizeof(uint16_t));
  // Set original input sample rate in Hz.
  uint32_t sample_rate = params.sample_rate();
  memcpy(header + OPUS_EXTRADATA_SAMPLE_RATE_OFFSET, &sample_rate,
         sizeof(uint32_t));
  // Set output gain in dB.
  uint16_t gain = 0;
  memcpy(header + OPUS_EXTRADATA_GAIN_OFFSET, &gain, 2);

  header[OPUS_EXTRADATA_CHANNEL_MAPPING_OFFSET] = 0;
}

static double GetFrameRate(const Muxer::VideoParameters& params) {
  const double kZeroFrameRate = 0.0;
  const double kDefaultFrameRate = 30.0;

  double frame_rate = params.frame_rate;
  if (frame_rate <= kZeroFrameRate ||
      frame_rate > limits::kMaxFramesPerSecond) {
    frame_rate = kDefaultFrameRate;
  }
  return frame_rate;
}

static const char kH264CodecId[] = "V_MPEG4/ISO/AVC";
static const char kPcmCodecId[] = "A_PCM/FLOAT/IEEE";

static const char* MkvCodeIcForMediaVideoCodecId(VideoCodec video_codec) {
  switch (video_codec) {
    case VideoCodec::kVP8:
      return mkvmuxer::Tracks::kVp8CodecId;
    case VideoCodec::kVP9:
      return mkvmuxer::Tracks::kVp9CodecId;
    case VideoCodec::kAV1:
      return mkvmuxer::Tracks::kAv1CodecId;
    case VideoCodec::kH264:
      return kH264CodecId;
    default:
      NOTREACHED() << "Unsupported codec " << GetCodecName(video_codec);
      return "";
  }
}

absl::optional<mkvmuxer::Colour> ColorFromColorSpace(
    const gfx::ColorSpace& color) {
  using mkvmuxer::Colour;
  using MatrixID = gfx::ColorSpace::MatrixID;
  using RangeID = gfx::ColorSpace::RangeID;
  using TransferID = gfx::ColorSpace::TransferID;
  using PrimaryID = gfx::ColorSpace::PrimaryID;
  Colour colour;
  int matrix_coefficients;
  switch (color.GetMatrixID()) {
    case MatrixID::BT709:
      matrix_coefficients = Colour::kBt709;
      break;
    case MatrixID::BT2020_NCL:
      matrix_coefficients = Colour::kBt2020NonConstantLuminance;
      break;
    default:
      return absl::nullopt;
  }
  colour.set_matrix_coefficients(matrix_coefficients);
  int range;
  switch (color.GetRangeID()) {
    case RangeID::LIMITED:
      range = Colour::kBroadcastRange;
      break;
    case RangeID::FULL:
      range = Colour::kFullRange;
      break;
    default:
      return absl::nullopt;
  }
  colour.set_range(range);
  int transfer_characteristics;
  switch (color.GetTransferID()) {
    case TransferID::BT709:
      transfer_characteristics = Colour::kIturBt709Tc;
      break;
    case TransferID::SRGB:
      transfer_characteristics = Colour::kIec6196621;
      break;
    case TransferID::PQ:
      transfer_characteristics = Colour::kSmpteSt2084;
      break;
    default:
      return absl::nullopt;
  }
  colour.set_transfer_characteristics(transfer_characteristics);
  int primaries;
  switch (color.GetPrimaryID()) {
    case PrimaryID::BT709:
      primaries = Colour::kIturBt709P;
      break;
    case PrimaryID::BT2020:
      primaries = Colour::kIturBt2020;
      break;
    default:
      return absl::nullopt;
  }
  colour.set_primaries(primaries);
  return colour;
}

}  // anonymous namespace

// -----------------------------------------------------------------------------
// WebmMuxer::Delegate:

WebmMuxer::Delegate::Delegate() {
  // Creation can be done on a different sequence than main activities.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

WebmMuxer::Delegate::~Delegate() = default;

mkvmuxer::int32 WebmMuxer::Delegate::Write(const void* buf,
                                           mkvmuxer::uint32 len) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << __func__ << " len " << len;
  DCHECK(buf);

  last_data_output_timestamp_ = base::TimeTicks::Now();
  const auto result = DoWrite(buf, len);
  position_ += len;
  return result;
}

// -----------------------------------------------------------------------------
// WebmMuxer:

WebmMuxer::WebmMuxer(AudioCodec audio_codec,
                     bool has_video,
                     bool has_audio,
                     std::unique_ptr<Delegate> delegate)
    : audio_codec_(audio_codec),
      video_codec_(VideoCodec::kUnknown),
      video_track_index_(0),
      audio_track_index_(0),
      has_video_(has_video),
      has_audio_(has_audio),
      delegate_(std::move(delegate)),
      force_one_libwebm_error_(false) {
  DCHECK(has_video_ || has_audio_);
  DCHECK(delegate_);
  DCHECK(audio_codec == AudioCodec::kOpus || audio_codec == AudioCodec::kPCM)
      << " Unsupported audio codec: " << GetCodecName(audio_codec);

  delegate_->InitSegment(&segment_);

  mkvmuxer::SegmentInfo* const info = segment_.GetSegmentInfo();
  info->set_writing_app("Chrome");
  info->set_muxing_app("Chrome");

  // Creation can be done on a different sequence than main activities.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

WebmMuxer::~WebmMuxer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Flush();

  if (has_audio_ && !has_video_) {
    base::UmaHistogramBoolean(
        "Media.WebmMuxer.DidAdjustTimestamp.AudioOnly.Muxer",
        did_adjust_muxer_timestamp_);
    base::UmaHistogramBoolean(
        "Media.WebmMuxer.DidAdjustTimestamp.AudioOnly.Audio",
        did_adjust_audio_timestamp_);
  } else if (!has_audio_ && has_video_) {
    base::UmaHistogramBoolean(
        "Media.WebmMuxer.DidAdjustTimestamp.VideoOnly.Muxer",
        did_adjust_muxer_timestamp_);
    base::UmaHistogramBoolean(
        "Media.WebmMuxer.DidAdjustTimestamp.VideoOnly.Video",
        did_adjust_video_timestamp_);
  } else {
    base::UmaHistogramBoolean(
        "Media.WebmMuxer.DidAdjustTimestamp.AudioVideo.Muxer",
        did_adjust_muxer_timestamp_);
    base::UmaHistogramBoolean(
        "Media.WebmMuxer.DidAdjustTimestamp.AudioVideo.Audio",
        did_adjust_audio_timestamp_);
    base::UmaHistogramBoolean(
        "Media.WebmMuxer.DidAdjustTimestamp.AudioVideo.Video",
        did_adjust_video_timestamp_);
  }
}

void WebmMuxer::SetMaximumDurationToForceDataOutput(base::TimeDelta interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  max_data_output_interval_ = std::max(interval, kMinimumForcedClusterDuration);
}

bool WebmMuxer::OnEncodedVideo(
    const VideoParameters& params,
    std::string encoded_data,
    std::string encoded_alpha,
    absl::optional<media::AudioEncoder::CodecDescription> codec_description,
    base::TimeTicks timestamp,
    bool is_key_frame) {
  TRACE_EVENT2("media", __func__, "timestamp", timestamp - base::TimeTicks(),
               "is_key_frame", is_key_frame);
  DVLOG(2) << __func__ << " - " << encoded_data.size() << "B ts " << timestamp;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(params.codec == VideoCodec::kVP8 || params.codec == VideoCodec::kVP9 ||
         params.codec == VideoCodec::kH264 || params.codec == VideoCodec::kAV1)
      << " Unsupported video codec: " << GetCodecName(params.codec);
  DCHECK(video_codec_ == VideoCodec::kUnknown || video_codec_ == params.codec)
      << "Unsupported: codec switched, to: " << GetCodecName(params.codec);

  if (encoded_data.size() == 0u) {
    DLOG(WARNING) << __func__ << ": zero size encoded frame, skipping";
    // Some encoders give sporadic zero-size data, see https://crbug.com/716451.
    return true;
  }

  if (!video_track_index_) {
    // |track_index_|, cannot be zero (!), initialize WebmMuxer in that case.
    // http://www.matroska.org/technical/specs/index.html#Tracks
    video_codec_ = params.codec;
    AddVideoTrack(params.visible_rect_size, GetFrameRate(params),
                  params.color_space);

    // Add codec private for AV1.
    if (params.codec == VideoCodec::kAV1 &&
        !segment_.GetTrackByNumber(video_track_index_)
             ->SetCodecPrivate(av1::codec_private, sizeof(av1::codec_private)))
      LOG(ERROR) << __func__ << " failed to set CodecPrivate for AV1.";
  }

  // TODO(ajose): Support multiple tracks: http://crbug.com/528523
  if (has_audio_ && !audio_track_index_) {
    DVLOG(1) << __func__ << ": delaying until audio track ready.";
    if (is_key_frame)  // Upon Key frame reception, empty the encoded queue.
      video_frames_.clear();
  }

  // Compensate for time in pause spent before the first frame.
  auto timestamp_minus_paused = timestamp - total_time_in_pause_;
  if (!video_timestamp_source_.has_value()) {
    video_timestamp_source_.emplace(timestamp_minus_paused,
                                    did_adjust_video_timestamp_);
  }
  video_frames_.push_back(EncodedFrame{
      std::move(encoded_data), std::move(encoded_alpha),
      video_timestamp_source_->UpdateAndGetNext(timestamp_minus_paused),
      is_key_frame});
  return PartiallyFlushQueues();
}

bool WebmMuxer::OnEncodedAudio(
    const AudioParameters& params,
    std::string encoded_data,
    absl::optional<media::AudioEncoder::CodecDescription> codec_description,
    base::TimeTicks timestamp) {
  TRACE_EVENT1("media", __func__, "timestamp", timestamp - base::TimeTicks());
  DVLOG(2) << __func__ << " - " << encoded_data.size() << "B ts " << timestamp;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MaybeForceNewCluster();
  if (!audio_track_index_) {
    AddAudioTrack(params);
  }

  // Compensate for time in pause spent before the first frame.
  auto timestamp_minus_paused = timestamp - total_time_in_pause_;
  if (!audio_timestamp_source_.has_value()) {
    audio_timestamp_source_.emplace(timestamp_minus_paused,
                                    did_adjust_audio_timestamp_);
  }
  audio_frames_.push_back(EncodedFrame{
      encoded_data, std::string(),
      audio_timestamp_source_->UpdateAndGetNext(timestamp_minus_paused),
      /*is_keyframe=*/true});
  return PartiallyFlushQueues();
}

void WebmMuxer::SetLiveAndEnabled(bool track_live_and_enabled, bool is_video) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool& written_track_live_and_enabled =
      is_video ? video_track_live_and_enabled_ : audio_track_live_and_enabled_;
  if (written_track_live_and_enabled != track_live_and_enabled) {
    DVLOG(1) << __func__ << (is_video ? " video " : " audio ")
             << "track live-and-enabled changed to " << track_live_and_enabled;
  }
  written_track_live_and_enabled = track_live_and_enabled;
}

void WebmMuxer::Pause() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!elapsed_time_in_pause_)
    elapsed_time_in_pause_.emplace();
}

void WebmMuxer::Resume() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (elapsed_time_in_pause_) {
    total_time_in_pause_ += elapsed_time_in_pause_->Elapsed();
    elapsed_time_in_pause_.reset();
  }
}

bool WebmMuxer::Flush() {
  // Depending on the |delegate_|, it can be either non-seekable (i.e. a live
  // stream), or seekable (file mode). So calling |segment_.Finalize()| here is
  // needed.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FlushQueues();
  return segment_.Finalize();
}

void WebmMuxer::AddVideoTrack(
    const gfx::Size& frame_size,
    double frame_rate,
    const absl::optional<gfx::ColorSpace>& color_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(0u, video_track_index_)
      << "WebmMuxer can only be initialized once.";

  video_track_index_ =
      segment_.AddVideoTrack(frame_size.width(), frame_size.height(), 0);
  if (video_track_index_ <= 0) {  // See https://crbug.com/616391.
    NOTREACHED() << "Error adding video track";
    return;
  }

  mkvmuxer::VideoTrack* const video_track =
      reinterpret_cast<mkvmuxer::VideoTrack*>(
          segment_.GetTrackByNumber(video_track_index_));
  if (color_space) {
    auto colour = ColorFromColorSpace(*color_space);
    if (colour)
      video_track->SetColour(*colour);
  }
  DCHECK(video_track);
  video_track->set_codec_id(MkvCodeIcForMediaVideoCodecId(video_codec_));
  DCHECK_EQ(0ull, video_track->crop_right());
  DCHECK_EQ(0ull, video_track->crop_left());
  DCHECK_EQ(0ull, video_track->crop_top());
  DCHECK_EQ(0ull, video_track->crop_bottom());
  DCHECK_EQ(0.0f, video_track->frame_rate());

  // Segment's timestamps should be in milliseconds, DCHECK it. See
  // http://www.webmproject.org/docs/container/#muxer-guidelines
  DCHECK_EQ(1000000ull, segment_.GetSegmentInfo()->timecode_scale());

  // Set alpha channel parameters for only VPX (crbug.com/711825).
  if (video_codec_ == VideoCodec::kH264)
    return;
  video_track->SetAlphaMode(mkvmuxer::VideoTrack::kAlpha);
  // Alpha channel, if present, is stored in a BlockAdditional next to the
  // associated opaque Block, see
  // https://matroska.org/technical/specs/index.html#BlockAdditional.
  // This follows Method 1 for VP8 encoding of A-channel described on
  // http://wiki.webmproject.org/alpha-channel.
  video_track->set_max_block_additional_id(1);
}

void WebmMuxer::AddAudioTrack(const AudioParameters& params) {
  DVLOG(1) << __func__ << " " << params.AsHumanReadableString();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(0u, audio_track_index_)
      << "WebmMuxer audio can only be initialised once.";

  audio_track_index_ =
      segment_.AddAudioTrack(params.sample_rate(), params.channels(), 0);
  if (audio_track_index_ <= 0) {  // See https://crbug.com/616391.
    NOTREACHED() << "Error adding audio track";
    return;
  }

  mkvmuxer::AudioTrack* const audio_track =
      reinterpret_cast<mkvmuxer::AudioTrack*>(
          segment_.GetTrackByNumber(audio_track_index_));
  DCHECK(audio_track);
  DCHECK_EQ(params.sample_rate(), audio_track->sample_rate());
  DCHECK_EQ(params.channels(), static_cast<int>(audio_track->channels()));
  DCHECK_LE(params.channels(), 2)
      << "Only 1 or 2 channels supported, requested " << params.channels();

  // Audio data is always pcm_f32le.
  audio_track->set_bit_depth(32u);

  if (audio_codec_ == AudioCodec::kOpus) {
    audio_track->set_codec_id(mkvmuxer::Tracks::kOpusCodecId);

    uint8_t opus_header[OPUS_EXTRADATA_SIZE];
    WriteOpusHeader(params, opus_header);

    if (!audio_track->SetCodecPrivate(opus_header, OPUS_EXTRADATA_SIZE))
      LOG(ERROR) << __func__ << ": failed to set opus header.";

    // Segment's timestamps should be in milliseconds, DCHECK it. See
    // http://www.webmproject.org/docs/container/#muxer-guidelines
    DCHECK_EQ(1000000ull, segment_.GetSegmentInfo()->timecode_scale());
  } else if (audio_codec_ == AudioCodec::kPCM) {
    audio_track->set_codec_id(kPcmCodecId);
  }
}

void WebmMuxer::FlushQueues() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while ((!video_frames_.empty() || !audio_frames_.empty()) &&
         FlushNextFrame()) {
  }
}

bool WebmMuxer::PartiallyFlushQueues() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Punt writing until all tracks have been created.
  if ((has_audio_ && !audio_track_index_) ||
      (has_video_ && !video_track_index_)) {
    return true;
  }

  bool result = true;
  // We strictly sort by timestamp unless a track is not live-and-enabled. In
  // that case we relax this and allow drainage of the live-and-enabled leg.
  while ((!has_video_ || !video_frames_.empty() ||
          !video_track_live_and_enabled_) &&
         (!has_audio_ || !audio_frames_.empty() ||
          !audio_track_live_and_enabled_) &&
         result) {
    if (video_frames_.empty() && audio_frames_.empty())
      return true;
    result = FlushNextFrame();
  }
  return result;
}

bool WebmMuxer::FlushNextFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeTicks min_timestamp = base::TimeTicks::Max();
  base::circular_deque<EncodedFrame>* queue = &video_frames_;
  uint8_t track_index = video_track_index_;
  if (!video_frames_.empty())
    min_timestamp = video_frames_.front().timestamp_minus_paused_;

  if (!audio_frames_.empty() &&
      audio_frames_.front().timestamp_minus_paused_ < min_timestamp) {
    queue = &audio_frames_;
    track_index = audio_track_index_;
  }

  EncodedFrame frame = std::move(queue->front());
  queue->pop_front();

  // Update the first timestamp if necessary so we can write relative timestamps
  // into the muxer.
  if (first_timestamp_.is_null()) {
    first_timestamp_ = frame.timestamp_minus_paused_;
  }

  // The logic tracking live-and-enabled that temporarily relaxes the strict
  // timestamp sorting allows for draining a track's queue completely in the
  // presence of the other track being muted. When the muted track becomes
  // live-and-enabled again the sorting recommences. However, tracks get encoded
  // data before live-and-enabled transitions to true. This can lead to us
  // emitting non-monotonic timestamps to the muxer, which results in an error
  // return. Fix this by enforcing monotonicity by rewriting timestamps.
  // TODO(crbug.com/1145203): If this causes audio glitches in the field,
  // reconsider this solution. For example, consider auto-marking a track
  // live-and-enabled when media appears and remove this catch-all.
  base::TimeDelta relative_timestamp =
      frame.timestamp_minus_paused_ - first_timestamp_;
  DLOG_IF(WARNING, relative_timestamp < last_timestamp_written_)
      << "Enforced a monotonically increasing timestamp. Last written "
      << last_timestamp_written_ << " new " << relative_timestamp;
  did_adjust_muxer_timestamp_ |= (relative_timestamp < last_timestamp_written_);
  relative_timestamp = std::max(relative_timestamp, last_timestamp_written_);
  last_timestamp_written_ = relative_timestamp;
  auto recorded_timestamp = relative_timestamp.InMicroseconds() *
                            base::Time::kNanosecondsPerMicrosecond;

  if (force_one_libwebm_error_) {
    DVLOG(1) << "Forcing a libwebm error";
    force_one_libwebm_error_ = false;
    return false;
  }

  DCHECK(frame.data.data());
  TRACE_EVENT2("media", __func__, "is_video", queue == &video_frames_,
               "recorded_timestamp", recorded_timestamp);
  bool result =
      frame.alpha_data.empty()
          ? segment_.AddFrame(
                reinterpret_cast<const uint8_t*>(frame.data.data()),
                frame.data.size(), track_index, recorded_timestamp,
                frame.is_keyframe)
          : segment_.AddFrameWithAdditional(
                reinterpret_cast<const uint8_t*>(frame.data.data()),
                frame.data.size(),
                reinterpret_cast<const uint8_t*>(frame.alpha_data.data()),
                frame.alpha_data.size(), 1 /* add_id */, track_index,
                recorded_timestamp, frame.is_keyframe);
  return result;
}

void WebmMuxer::MaybeForceNewCluster() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!has_video_ || max_data_output_interval_.is_zero() ||
      delegate_->last_data_output_timestamp().is_null()) {
    return;
  }

  // TODO(crbug.com/1381323): consider if cluster output should be based on
  // media timestamps
  if (base::TimeTicks::Now() - delegate_->last_data_output_timestamp() >=
      max_data_output_interval_) {
    TRACE_EVENT0("media", "ForceNewClusterOnNextFrame");
    segment_.ForceNewClusterOnNextFrame();
  }
}

WebmMuxer::MonotonicTimestampSequence::MonotonicTimestampSequence(
    base::TimeTicks first_timestamp,
    bool& did_adjust_timestamp)
    : last_timestamp_(first_timestamp),
      did_adjust_timestamp_(did_adjust_timestamp) {}

base::TimeTicks WebmMuxer::MonotonicTimestampSequence::UpdateAndGetNext(
    base::TimeTicks timestamp) {
  DVLOG(3) << __func__ << " ts " << timestamp << " last " << last_timestamp_;
  // In theory, time increases monotonically. In practice, it does not.
  // See http://crbug/618407.
  DLOG_IF(WARNING, timestamp < last_timestamp_)
      << "Encountered a non-monotonically increasing timestamp. Was: "
      << last_timestamp_ << ", timestamp: " << timestamp;
  *did_adjust_timestamp_ |= (timestamp < last_timestamp_);
  last_timestamp_ = std::max(last_timestamp_, timestamp);
  return last_timestamp_;
}

}  // namespace media
