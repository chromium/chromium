// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/webm_muxer.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "media/base/audio_parameters.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/formats/common/opus_constants.h"

namespace media {

namespace {

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

}  // anonymous namespace

WebmMuxer::VideoParameters::VideoParameters(
    scoped_refptr<media::VideoFrame> frame) {
  visible_rect_size = frame->visible_rect().size();
  frame_rate = 0.0;
  ignore_result(frame->metadata()->GetDouble(VideoFrameMetadata::FRAME_RATE,
                                             &frame_rate));
}

WebmMuxer::VideoParameters::~VideoParameters() = default;

WebmMuxer::WebmMuxer(VideoCodec video_codec,
                     AudioCodec audio_codec,
                     bool has_video,
                     bool has_audio,
                     const WriteDataCB& write_data_callback)
    : video_codec_(video_codec),
      audio_codec_(audio_codec),
      video_track_index_(0),
      audio_track_index_(0),
      has_video_(has_video),
      has_audio_(has_audio),
      write_data_callback_(write_data_callback),
      position_(0),
      force_one_libwebm_error_(false) {
  DCHECK(has_video_ || has_audio_);
  DCHECK(!write_data_callback_.is_null());
  DCHECK(video_codec == kCodecVP8 || video_codec == kCodecVP9 ||
         video_codec == kCodecH264)
      << " Unsupported video codec: " << GetCodecName(video_codec);
  DCHECK(audio_codec == kCodecOpus || audio_codec == kCodecPCM)
      << " Unsupported audio codec: " << GetCodecName(audio_codec);

  segment_.Init(this);
  segment_.set_mode(mkvmuxer::Segment::kLive);
  segment_.OutputCues(false);

  mkvmuxer::SegmentInfo* const info = segment_.GetSegmentInfo();
  info->set_writing_app("Chrome");
  info->set_muxing_app("Chrome");

  // Creation is done on a different thread than main activities.
  thread_checker_.DetachFromThread();
}

WebmMuxer::~WebmMuxer() {
  // No need to segment_.Finalize() since is not Seekable(), i.e. a live
  // stream, but is a good practice.
  DCHECK(thread_checker_.CalledOnValidThread());
  segment_.Finalize();
}

bool WebmMuxer::OnEncodedVideo(const VideoParameters& params,
                               std::string encoded_data,
                               std::string encoded_alpha,
                               base::TimeTicks timestamp,
                               bool is_key_frame) {
  DVLOG(1) << __func__ << " - " << encoded_data.size() << "B";
  DCHECK(thread_checker_.CalledOnValidThread());

  if (encoded_data.size() == 0u) {
    DLOG(WARNING) << __func__ << ": zero size encoded frame, skipping";
    // Some encoders give sporadic zero-size data, see https://crbug.com/716451.
    return true;
  }

  if (!video_track_index_) {
    // |track_index_|, cannot be zero (!), initialize WebmMuxer in that case.
    // http://www.matroska.org/technical/specs/index.html#Tracks
    AddVideoTrack(params.visible_rect_size, GetFrameRate(params));
    if (first_frame_timestamp_video_.is_null())
      first_frame_timestamp_video_ = timestamp;
  }

  // TODO(ajose): Support multiple tracks: http://crbug.com/528523
  if (has_audio_ && !audio_track_index_) {
    DVLOG(1) << __func__ << ": delaying until audio track ready.";
    if (is_key_frame)  // Upon Key frame reception, empty the encoded queue.
      encoded_frames_queue_.clear();

    encoded_frames_queue_.push_back(std::make_unique<EncodedVideoFrame>(
        std::move(encoded_data), std::move(encoded_alpha), timestamp,
        is_key_frame));
    return true;
  }

  // Any saved encoded video frames must have been dumped in OnEncodedAudio();
  DCHECK(encoded_frames_queue_.empty());

  return AddFrame(encoded_data, encoded_alpha, video_track_index_,
                  timestamp - first_frame_timestamp_video_, is_key_frame);
}

bool WebmMuxer::OnEncodedAudio(const media::AudioParameters& params,
                               std::string encoded_data,
                               base::TimeTicks timestamp) {
  DVLOG(2) << __func__ << " - " << encoded_data.size() << "B";
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!audio_track_index_) {
    AddAudioTrack(params);
    if (first_frame_timestamp_audio_.is_null())
      first_frame_timestamp_audio_ = timestamp;
  }

  // TODO(ajose): Don't drop audio data: http://crbug.com/547948
  // TODO(ajose): Support multiple tracks: http://crbug.com/528523
  if (has_video_ && !video_track_index_) {
    DVLOG(1) << __func__ << ": delaying until video track ready.";
    return true;
  }

  // Dump all saved encoded video frames if any.
  while (!encoded_frames_queue_.empty()) {
    const bool res = AddFrame(
        encoded_frames_queue_.front()->data,
        encoded_frames_queue_.front()->alpha_data, video_track_index_,
        encoded_frames_queue_.front()->timestamp - first_frame_timestamp_video_,
        encoded_frames_queue_.front()->is_keyframe);
    if (!res)
      return false;
    encoded_frames_queue_.pop_front();
  }
  return AddFrame(encoded_data, std::string(), audio_track_index_,
                  timestamp - first_frame_timestamp_audio_,
                  true /* is_key_frame -- always true for audio */);
}

void WebmMuxer::Pause() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!elapsed_time_in_pause_)
    elapsed_time_in_pause_.reset(new base::ElapsedTimer());
}

void WebmMuxer::Resume() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
  if (elapsed_time_in_pause_) {
    total_time_in_pause_ += elapsed_time_in_pause_->Elapsed();
    elapsed_time_in_pause_.reset();
  }
}

void WebmMuxer::AddVideoTrack(const gfx::Size& frame_size, double frame_rate) {
  DCHECK(thread_checker_.CalledOnValidThread());
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
  DCHECK(thread_checker_.CalledOnValidThread());
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
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(buf);
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

bool WebmMuxer::AddFrame(const std::string& encoded_data,
                         const std::string& encoded_alpha,
                         uint8_t track_index,
                         base::TimeDelta timestamp,
                         bool is_key_frame) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!has_video_ || video_track_index_);
  DCHECK(!has_audio_ || audio_track_index_);

  most_recent_timestamp_ =
      std::max(most_recent_timestamp_, timestamp - total_time_in_pause_);

  if (force_one_libwebm_error_) {
    DVLOG(1) << "Forcing a libwebm error";
    force_one_libwebm_error_ = false;
    return false;
  }

  DCHECK(encoded_data.data());
  if (encoded_alpha.empty()) {
    return segment_.AddFrame(
        reinterpret_cast<const uint8_t*>(encoded_data.data()),
        encoded_data.size(), track_index,
        most_recent_timestamp_.InMicroseconds() *
            base::Time::kNanosecondsPerMicrosecond,
        is_key_frame);
  }

  DCHECK(encoded_alpha.data());
  return segment_.AddFrameWithAdditional(
      reinterpret_cast<const uint8_t*>(encoded_data.data()),
      encoded_data.size(),
      reinterpret_cast<const uint8_t*>(encoded_alpha.data()),
      encoded_alpha.size(), 1 /* add_id */, track_index,
      most_recent_timestamp_.InMicroseconds() *
          base::Time::kNanosecondsPerMicrosecond,
      is_key_frame);
}

WebmMuxer::EncodedVideoFrame::EncodedVideoFrame(std::string data,
                                                std::string alpha_data,
                                                base::TimeTicks timestamp,
                                                bool is_keyframe)
    : data(std::move(data)),
      alpha_data(std::move(alpha_data)),
      timestamp(timestamp),
      is_keyframe(is_keyframe) {}

WebmMuxer::EncodedVideoFrame::~EncodedVideoFrame() = default;

}  // namespace media
