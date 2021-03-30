// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/webm_muxer.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "media/base/audio_parameters.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/formats/common/opus_constants.h"

namespace media {

namespace {

// Force new clusters at a maximum rate of 10 Hz.
constexpr base::TimeDelta kMinimumForcedClusterDuration =
    base::TimeDelta::FromMilliseconds(100);

void WriteOpusHeader(const media::AudioParameters& params, uint8_t* header) {
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

static double GetFrameRate(const WebmMuxer::VideoParameters& params) {
  const double kZeroFrameRate = 0.0;
  const double kDefaultFrameRate = 30.0;

  double frame_rate = params.frame_rate;
  if (frame_rate <= kZeroFrameRate ||
      frame_rate > media::limits::kMaxFramesPerSecond) {
    frame_rate = kDefaultFrameRate;
  }
  return frame_rate;
}

static const char kH264CodecId[] = "V_MPEG4/ISO/AVC";
static const char kPcmCodecId[] = "A_PCM/FLOAT/IEEE";

static const char* MkvCodeIcForMediaVideoCodecId(VideoCodec video_codec) {
  switch (video_codec) {
    case kCodecVP8:
      return mkvmuxer::Tracks::kVp8CodecId;
    case kCodecVP9:
      return mkvmuxer::Tracks::kVp9CodecId;
    case kCodecH264:
      return kH264CodecId;
    default:
      NOTREACHED() << "Unsupported codec " << GetCodecName(video_codec);
      return "";
  }
}

base::Optional<mkvmuxer::Colour> ColorFromColorSpace(
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
      return base::nullopt;
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
      return base::nullopt;
  }
  colour.set_range(range);
  int transfer_characteristics;
  switch (color.GetTransferID()) {
    case TransferID::BT709:
      transfer_characteristics = Colour::kIturBt709Tc;
      break;
    case TransferID::IEC61966_2_1:
      transfer_characteristics = Colour::kIec6196621;
      break;
    case TransferID::SMPTEST2084:
      transfer_characteristics = Colour::kSmpteSt2084;
      break;
    default:
      return base::nullopt;
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
      return base::nullopt;
  }
  colour.set_primaries(primaries);
  return colour;
}

}  // anonymous namespace

WebmMuxer::VideoParameters::VideoParameters(
    scoped_refptr<media::VideoFrame> frame)
    : visible_rect_size(frame->visible_rect().size()),
      frame_rate(frame->metadata().frame_rate.value_or(0.0)),
      codec(kUnknownVideoCodec),
      color_space(frame->ColorSpace()) {}

WebmMuxer::VideoParameters::VideoParameters(
    gfx::Size visible_rect_size,
    double frame_rate,
    VideoCodec codec,
    base::Optional<gfx::ColorSpace> color_space)
    : visible_rect_size(visible_rect_size),
      frame_rate(frame_rate),
      codec(codec),
      color_space(color_space) {}

WebmMuxer::VideoParameters::VideoParameters(const VideoParameters&) = default;

WebmMuxer::VideoParameters::~VideoParameters() = default;

WebmMuxer::WebmMuxer(AudioCodec audio_codec,
                     bool has_video,
                     bool has_audio,
                     const WriteDataCB& write_data_callback)
    : audio_codec_(audio_codec),
      video_codec_(kUnknownVideoCodec),
      video_track_index_(0),
      audio_track_index_(0),
      has_video_(has_video),
      has_audio_(has_audio),
      write_data_callback_(write_data_callback),
      position_(0),
      force_one_libwebm_error_(false) {
  DCHECK(has_video_ || has_audio_);
  DCHECK(!write_data_callback_.is_null());
  DCHECK(audio_codec == kCodecOpus || audio_codec == kCodecPCM)
      << " Unsupported audio codec: " << GetCodecName(audio_codec);

  segment_.Init(this);
  segment_.set_mode(mkvmuxer::Segment::kLive);
  segment_.OutputCues(false);

  mkvmuxer::SegmentInfo* const info = segment_.GetSegmentInfo();
  info->set_writing_app("Chrome");
  info->set_muxing_app("Chrome");

  // Creation can be done on a different sequence than main activities.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

WebmMuxer::~WebmMuxer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Flush();
}

void WebmMuxer::SetMaximumDurationToForceDataOutput(base::TimeDelta interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  max_data_output_interval_ = std::max(interval, kMinimumForcedClusterDuration);
}

bool WebmMuxer::OnEncodedVideo(const VideoParameters& params,
                               std::string encoded_data,
                               std::string encoded_alpha,
                               base::TimeTicks timestamp,
                               bool is_key_frame) {
  DVLOG(2) << __func__ << " - " << encoded_data.size() << "B";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(params.codec == kCodecVP8 || params.codec == kCodecVP9 ||
         params.codec == kCodecH264)
      << " Unsupported video codec: " << GetCodecName(params.codec);
  DCHECK(video_codec_ == kUnknownVideoCodec || video_codec_ == params.codec)
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
    if (first_frame_timestamp_video_.is_null()) {
      // Compensate for time in pause spent before the first frame.
      first_frame_timestamp_video_ = timestamp - total_time_in_pause_;
      last_frame_timestamp_video_ = first_frame_timestamp_video_;
    }
  }

  // TODO(ajose): Support multiple tracks: http://crbug.com/528523
  if (has_audio_ && !audio_track_index_) {
    DVLOG(1) << __func__ << ": delaying until audio track ready.";
    if (is_key_frame)  // Upon Key frame reception, empty the encoded queue.
      video_frames_.clear();
  }
  const base::TimeTicks recorded_timestamp =
      UpdateLastTimestampMonotonically(timestamp, &last_frame_timestamp_video_);
  video_frames_.push_back(EncodedFrame{
      std::move(encoded_data), std::move(encoded_alpha),
      recorded_timestamp - first_frame_timestamp_video_, is_key_frame});
  return PartiallyFlushQueues();
}

bool WebmMuxer::OnEncodedAudio(const media::AudioParameters& params,
                               std::string encoded_data,
                               base::TimeTicks timestamp) {
  DVLOG(2) << __func__ << " - " << encoded_data.size() << "B";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MaybeForceNewCluster();
  if (!audio_track_index_) {
    AddAudioTrack(params);
    if (first_frame_timestamp_audio_.is_null()) {
      // Compensate for time in pause spent before the first frame.
      first_frame_timestamp_audio_ = timestamp - total_time_in_pause_;
      last_frame_timestamp_audio_ = first_frame_timestamp_audio_;
    }
  }

  const base::TimeTicks recorded_timestamp =
      UpdateLastTimestampMonotonically(timestamp, &last_frame_timestamp_audio_);
  audio_frames_.push_back(
      EncodedFrame{encoded_data, std::string(),
                   recorded_timestamp - first_frame_timestamp_audio_,
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
    elapsed_time_in_pause_ = std::make_unique<base::ElapsedTimer>();
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
  // No need to segment_.Finalize() since is not Seekable(), i.e. a live
  // stream, but is a good practice.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FlushQueues();
  return segment_.Finalize();
}

void WebmMuxer::AddVideoTrack(
    const gfx::Size& frame_size,
    double frame_rate,
    const base::Optional<gfx::ColorSpace>& color_space) {
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
  if (video_codec_ == kCodecH264)
    return;
  video_track->SetAlphaMode(mkvmuxer::VideoTrack::kAlpha);
  // Alpha channel, if present, is stored in a BlockAdditional next to the
  // associated opaque Block, see
  // https://matroska.org/technical/specs/index.html#BlockAdditional.
  // This follows Method 1 for VP8 encoding of A-channel described on
  // http://wiki.webmproject.org/alpha-channel.
  video_track->set_max_block_additional_id(1);
}

void WebmMuxer::AddAudioTrack(const media::AudioParameters& params) {
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

  if (audio_codec_ == kCodecOpus) {
    audio_track->set_codec_id(mkvmuxer::Tracks::kOpusCodecId);

    uint8_t opus_header[OPUS_EXTRADATA_SIZE];
    WriteOpusHeader(params, opus_header);

    if (!audio_track->SetCodecPrivate(opus_header, OPUS_EXTRADATA_SIZE))
      LOG(ERROR) << __func__ << ": failed to set opus header.";

    // Segment's timestamps should be in milliseconds, DCHECK it. See
    // http://www.webmproject.org/docs/container/#muxer-guidelines
    DCHECK_EQ(1000000ull, segment_.GetSegmentInfo()->timecode_scale());
  } else if (audio_codec_ == kCodecPCM) {
    audio_track->set_codec_id(kPcmCodecId);
  }
}

mkvmuxer::int32 WebmMuxer::Write(const void* buf, mkvmuxer::uint32 len) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << __func__ << " len " << len;
  DCHECK(buf);
  last_data_output_timestamp_ = base::TimeTicks::Now();
  write_data_callback_.Run(
      base::StringPiece(reinterpret_cast<const char*>(buf), len));
  position_ += len;
  return 0;
}

mkvmuxer::int64 WebmMuxer::Position() const {
  return position_.ValueOrDie();
}

mkvmuxer::int32 WebmMuxer::Position(mkvmuxer::int64 position) {
  // The stream is not Seekable() so indicate we cannot set the position.
  return -1;
}

bool WebmMuxer::Seekable() const {
  return false;
}

void WebmMuxer::ElementStartNotify(mkvmuxer::uint64 element_id,
                                   mkvmuxer::int64 position) {
  // This method gets pinged before items are sent to |write_data_callback_|.
  DCHECK_GE(position, position_.ValueOrDefault(0))
      << "Can't go back in a live WebM stream.";
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
  base::TimeDelta min_timestamp = base::TimeDelta::Max();
  base::circular_deque<EncodedFrame>* queue = &video_frames_;
  uint8_t track_index = video_track_index_;
  if (!video_frames_.empty())
    min_timestamp = video_frames_.front().relative_timestamp;

  if (!audio_frames_.empty() &&
      audio_frames_.front().relative_timestamp < min_timestamp) {
    queue = &audio_frames_;
    track_index = audio_track_index_;
  }

  EncodedFrame frame = std::move(queue->front());
  queue->pop_front();
  // The logic tracking live-and-enabled that temporarily relaxes the strict
  // timestamp sorting allows for draining a track's queue completely in the
  // presence of the other track being muted. When the muted track becomes
  // live-and-enabled again the sorting recommences. However, tracks get encoded
  // data before live-and-enabled transitions to true. This can lead to us
  // emitting non-monotonic timestamps to the muxer, which results in an error
  // return. Fix this by enforcing monotonicity by rewriting timestamps.
  base::TimeDelta relative_timestamp = frame.relative_timestamp;
  DLOG_IF(WARNING, relative_timestamp < last_timestamp_written_)
      << "Enforced a monotonically increasing timestamp. Last written "
      << last_timestamp_written_ << " new " << relative_timestamp;
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

base::TimeTicks WebmMuxer::UpdateLastTimestampMonotonically(
    base::TimeTicks timestamp,
    base::TimeTicks* last_timestamp) {
  base::TimeTicks compensated_timestamp = timestamp - total_time_in_pause_;
  // In theory, time increases monotonically. In practice, it does not.
  // See http://crbug/618407.
  DLOG_IF(WARNING, compensated_timestamp < *last_timestamp)
      << "Encountered a non-monotonically increasing timestamp. Was: "
      << *last_timestamp << ", compensated: " << compensated_timestamp
      << ", uncompensated: " << timestamp;
  *last_timestamp = std::max(*last_timestamp, compensated_timestamp);
  return *last_timestamp;
}

void WebmMuxer::MaybeForceNewCluster() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (has_video_ && !max_data_output_interval_.is_zero() &&
      !last_data_output_timestamp_.is_null()) {
    base::TimeTicks now = base::TimeTicks::Now();
    if (now - last_data_output_timestamp_ >= max_data_output_interval_) {
      segment_.ForceNewClusterOnNextFrame();
    }
  }
}

}  // namespace media
