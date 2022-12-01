// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/media_recorder_handler.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/system/sys_info.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_parameters.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/mime_util.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/muxers/live_webm_muxer_delegate.h"
#include "media/muxers/muxer.h"
#include "media/muxers/webm_muxer.h"
#include "third_party/blink/renderer/modules/mediarecorder/buildflags.h"
#include "third_party/blink/renderer/modules/mediarecorder/media_recorder.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/media_capabilities/web_media_capabilities_info.h"
#include "third_party/blink/renderer/platform/media_capabilities/web_media_configuration.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/mediastream/webrtc_uma_histograms.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

using base::TimeTicks;

namespace blink {

namespace {

// Encoding smoothness depends on a number of parameters, namely: frame rate,
// resolution, hardware support availability, platform and IsLowEndDevice(); to
// simplify calculations we compare the amount of pixels per second (i.e.
// resolution times frame rate). Software based encoding on Desktop can run
// fine up and until HD resolution at 30fps, whereas if IsLowEndDevice() we set
// the cut at VGA at 30fps (~27Mpps and ~9Mpps respectively).
// TODO(mcasas): The influence of the frame rate is not exactly linear, so this
// threshold might be oversimplified, https://crbug.com/709181.
const float kNumPixelsPerSecondSmoothnessThresholdLow = 640 * 480 * 30.0;
const float kNumPixelsPerSecondSmoothnessThresholdHigh = 1280 * 720 * 30.0;

VideoTrackRecorder::CodecId CodecIdFromMediaVideoCodec(media::VideoCodec id) {
  switch (id) {
    case media::VideoCodec::kVP8:
      return VideoTrackRecorder::CodecId::kVp8;
    case media::VideoCodec::kVP9:
      return VideoTrackRecorder::CodecId::kVp9;
#if BUILDFLAG(RTC_USE_H264)
    case media::VideoCodec::kH264:
      return VideoTrackRecorder::CodecId::kH264;
#endif
    default:
      return VideoTrackRecorder::CodecId::kLast;
  }
  NOTREACHED() << "Unsupported video codec";
  return VideoTrackRecorder::CodecId::kLast;
}

media::VideoCodec MediaVideoCodecFromCodecId(VideoTrackRecorder::CodecId id) {
  switch (id) {
    case VideoTrackRecorder::CodecId::kVp8:
      return media::VideoCodec::kVP8;
    case VideoTrackRecorder::CodecId::kVp9:
      return media::VideoCodec::kVP9;
#if BUILDFLAG(RTC_USE_H264)
    case VideoTrackRecorder::CodecId::kH264:
      return media::VideoCodec::kH264;
#endif
    case VideoTrackRecorder::CodecId::kLast:
      return media::VideoCodec::kUnknown;
  }
  NOTREACHED() << "Unsupported video codec";
  return media::VideoCodec::kUnknown;
}

media::AudioCodec CodecIdToMediaAudioCodec(AudioTrackRecorder::CodecId id) {
  switch (id) {
    case AudioTrackRecorder::CodecId::kPcm:
      return media::AudioCodec::kPCM;
    case AudioTrackRecorder::CodecId::kOpus:
      return media::AudioCodec::kOpus;
    case AudioTrackRecorder::CodecId::kAac:
      return media::AudioCodec::kAAC;
    case AudioTrackRecorder::CodecId::kLast:
      return media::AudioCodec::kUnknown;
  }
  NOTREACHED() << "Unsupported audio codec";
  return media::AudioCodec::kUnknown;
}

// Extracts the first recognised CodecId of |codecs| or CodecId::LAST if none
// of them is known. Sets codec profile and level if the information can be
// parsed from codec suffix.
VideoTrackRecorder::CodecProfile VideoStringToCodecProfile(
    const String& codecs) {
  String codecs_str = codecs.LowerASCII();
  VideoTrackRecorder::CodecId codec_id = VideoTrackRecorder::CodecId::kLast;

  if (codecs_str.Find("vp8") != kNotFound)
    codec_id = VideoTrackRecorder::CodecId::kVp8;
  if (codecs_str.Find("vp9") != kNotFound)
    codec_id = VideoTrackRecorder::CodecId::kVp9;
#if BUILDFLAG(RTC_USE_H264)
  if (codecs_str.Find("h264") != kNotFound)
    codec_id = VideoTrackRecorder::CodecId::kH264;
  wtf_size_t avc1_start = codecs_str.Find("avc1");
  if (avc1_start != kNotFound) {
    codec_id = VideoTrackRecorder::CodecId::kH264;

    wtf_size_t avc1_end = codecs_str.Find(",");
    String avc1_str =
        codecs_str
            .Substring(avc1_start, avc1_end == kNotFound ? UINT_MAX : avc1_end)
            .StripWhiteSpace();
    media::VideoCodecProfile profile;
    uint8_t level;
    if (media::ParseAVCCodecId(avc1_str.Ascii(), &profile, &level))
      return {codec_id, profile, level};
  }
#endif
  return VideoTrackRecorder::CodecProfile(codec_id);
}

AudioTrackRecorder::CodecId AudioStringToCodecId(const String& codecs) {
  String codecs_str = codecs.LowerASCII();

  if (codecs_str.Find("opus") != kNotFound)
    return AudioTrackRecorder::CodecId::kOpus;
  if (codecs_str.Find("pcm") != kNotFound)
    return AudioTrackRecorder::CodecId::kPcm;

  return AudioTrackRecorder::CodecId::kLast;
}

}  // anonymous namespace

MediaRecorderHandler::MediaRecorderHandler(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : video_codec_profile_(VideoTrackRecorder::CodecId::kLast),
      audio_codec_id_(AudioTrackRecorder::CodecId::kLast),
      recorder_(nullptr),
      task_runner_(std::move(task_runner)) {}

MediaRecorderHandler::~MediaRecorderHandler() = default;

bool MediaRecorderHandler::CanSupportMimeType(const String& type,
                                              const String& web_codecs) {
  DCHECK(IsMainThread());
  // An empty |type| means MediaRecorderHandler can choose its preferred codecs.
  if (type.empty())
    return true;

  const bool video = EqualIgnoringASCIICase(type, "video/webm") ||
                     EqualIgnoringASCIICase(type, "video/x-matroska");
  const bool audio = !video && EqualIgnoringASCIICase(type, "audio/webm");
  if (!video && !audio)
    return false;

  // Both |video| and |audio| support empty |codecs|; |type| == "video" supports
  // vp8, vp9, h264 and avc1 or opus; |type| = "audio", supports opus or pcm
  // (little-endian 32-bit float).
  // http://www.webmproject.org/docs/container Sec:"HTML5 Video Type Parameters"
  static const char* const kVideoCodecs[] = {
    "vp8",
    "vp9",
#if BUILDFLAG(RTC_USE_H264)
    "h264",
    "avc1",
#endif
    "opus",
    "pcm"
  };
  static const char* const kAudioCodecs[] = {"opus", "pcm"};

  auto* const* relevant_codecs_begin =
      video ? std::begin(kVideoCodecs) : std::begin(kAudioCodecs);
  auto* const* relevant_codecs_end =
      video ? std::end(kVideoCodecs) : std::end(kAudioCodecs);
  std::vector<std::string> codecs_list;
  media::SplitCodecs(web_codecs.Utf8(), &codecs_list);
  media::StripCodecs(&codecs_list);
  for (const auto& codec : codecs_list) {
    String codec_string = String::FromUTF8(codec);
    if (std::none_of(relevant_codecs_begin, relevant_codecs_end,
                     [&codec_string](const char* name) {
                       return EqualIgnoringASCIICase(codec_string, name);
                     })) {
      return false;
    }
  }
  return true;
}

bool MediaRecorderHandler::Initialize(
    MediaRecorder* recorder,
    MediaStreamDescriptor* media_stream,
    const String& type,
    const String& codecs,
    uint32_t audio_bits_per_second,
    uint32_t video_bits_per_second,
    AudioTrackRecorder::BitrateMode audio_bitrate_mode) {
  DCHECK(IsMainThread());
  // Save histogram data so we can see how much MediaStream Recorder is used.
  // The histogram counts the number of calls to the JS API.
  UpdateWebRTCMethodCount(RTCAPIName::kMediaStreamRecorder);

  if (!CanSupportMimeType(type, codecs)) {
    DLOG(ERROR) << "Unsupported " << type.Utf8() << ";codecs=" << codecs.Utf8();
    return false;
  }

  passthrough_enabled_ = type.empty();

  // Once established that we support the codec(s), hunt then individually.
  video_codec_profile_ = VideoStringToCodecProfile(codecs);
  if (video_codec_profile_.codec_id == VideoTrackRecorder::CodecId::kLast) {
    video_codec_profile_.codec_id =
        VideoTrackRecorderImpl::GetPreferredCodecId();
    DVLOG(1) << "Falling back to preferred video codec id "
             << static_cast<int>(video_codec_profile_.codec_id);
  }

  // Do the same for the audio codec(s).
  const AudioTrackRecorder::CodecId audio_codec_id =
      AudioStringToCodecId(codecs);
  audio_codec_id_ = (audio_codec_id != AudioTrackRecorder::CodecId::kLast)
                        ? audio_codec_id
                        : AudioTrackRecorder::GetPreferredCodecId();
  DVLOG_IF(1, audio_codec_id == AudioTrackRecorder::CodecId::kLast)
      << "Falling back to preferred audio codec id "
      << static_cast<int>(audio_codec_id_);

  media_stream_ = media_stream;
  DCHECK(recorder);
  recorder_ = recorder;

  audio_bits_per_second_ = audio_bits_per_second;
  video_bits_per_second_ = video_bits_per_second;
  audio_bitrate_mode_ = audio_bitrate_mode;
  return true;
}

AudioTrackRecorder::BitrateMode MediaRecorderHandler::AudioBitrateMode() {
  return audio_bitrate_mode_;
}

bool MediaRecorderHandler::Start(int timeslice) {
  DCHECK(IsMainThread());
  DCHECK(!recording_);
  DCHECK(media_stream_);
  DCHECK(timeslice_.is_zero());
  DCHECK(!muxer_);

  invalidated_ = false;

  timeslice_ = base::Milliseconds(timeslice);
  slice_origin_timestamp_ = base::TimeTicks::Now();

  video_tracks_ = media_stream_->VideoComponents();
  audio_tracks_ = media_stream_->AudioComponents();

  if (video_tracks_.empty() && audio_tracks_.empty()) {
    LOG(WARNING) << __func__ << ": no media tracks.";
    return false;
  }

  const bool use_video_tracks =
      !video_tracks_.empty() &&
      video_tracks_[0]->GetReadyState() != MediaStreamSource::kReadyStateEnded;
  const bool use_audio_tracks =
      !audio_tracks_.empty() && audio_tracks_[0]->GetPlatformTrack() &&
      audio_tracks_[0]->GetReadyState() != MediaStreamSource::kReadyStateEnded;

  if (!use_video_tracks && !use_audio_tracks) {
    LOG(WARNING) << __func__ << ": no tracks to be recorded.";
    return false;
  }

  muxer_ = std::make_unique<media::WebmMuxer>(
      CodecIdToMediaAudioCodec(audio_codec_id_), use_video_tracks,
      use_audio_tracks,
      std::make_unique<media::LiveWebmMuxerDelegate>(WTF::BindRepeating(
          &MediaRecorderHandler::WriteData, WrapWeakPersistent(this))));
  if (timeslice > 0)
    muxer_->SetMaximumDurationToForceDataOutput(timeslice_);
  if (use_video_tracks) {
    // TODO(mcasas): The muxer API supports only one video track. Extend it to
    // several video tracks, see http://crbug.com/528523.
    LOG_IF(WARNING, video_tracks_.size() > 1u)
        << "Recording multiple video tracks is not implemented. "
        << "Only recording first video track.";
    if (!video_tracks_[0])
      return false;
    UpdateTrackLiveAndEnabled(*video_tracks_[0], /*is_video=*/true);

    MediaStreamVideoTrack* const video_track =
        static_cast<MediaStreamVideoTrack*>(
            video_tracks_[0]->GetPlatformTrack());
    base::OnceClosure on_track_source_changed_cb = media::BindToCurrentLoop(
        WTF::BindOnce(&MediaRecorderHandler::OnSourceReadyStateChanged,
                      WrapWeakPersistent(this)));
    const bool use_encoded_source_output =
        video_track->source() != nullptr &&
        video_track->source()->SupportsEncodedOutput();
    if (passthrough_enabled_ && use_encoded_source_output) {
      const VideoTrackRecorder::OnEncodedVideoCB on_passthrough_video_cb =
          media::BindToCurrentLoop(
              WTF::BindRepeating(&MediaRecorderHandler::OnPassthroughVideo,
                                 WrapWeakPersistent(this)));
      video_recorders_.emplace_back(
          std::make_unique<VideoTrackRecorderPassthrough>(
              video_tracks_[0], std::move(on_passthrough_video_cb),
              std::move(on_track_source_changed_cb), task_runner_));
    } else {
      const VideoTrackRecorder::OnEncodedVideoCB on_encoded_video_cb =
          media::BindToCurrentLoop(WTF::BindRepeating(
              &MediaRecorderHandler::OnEncodedVideo, WrapWeakPersistent(this)));
      video_recorders_.emplace_back(std::make_unique<VideoTrackRecorderImpl>(
          video_codec_profile_, video_tracks_[0],
          std::move(on_encoded_video_cb), std::move(on_track_source_changed_cb),
          video_bits_per_second_, task_runner_));
    }
  }

  if (use_audio_tracks) {
    // TODO(ajose): The muxer API supports only one audio track. Extend it to
    // several tracks.
    LOG_IF(WARNING, audio_tracks_.size() > 1u)
        << "Recording multiple audio"
        << " tracks is not implemented.  Only recording first audio track.";
    if (!audio_tracks_[0])
      return false;
    UpdateTrackLiveAndEnabled(*audio_tracks_[0], /*is_video=*/false);

    const AudioTrackRecorder::OnEncodedAudioCB on_encoded_audio_cb =
        media::BindToCurrentLoop(WTF::BindRepeating(
            &MediaRecorderHandler::OnEncodedAudio, WrapWeakPersistent(this)));
    base::OnceClosure on_track_source_changed_cb = media::BindToCurrentLoop(
        WTF::BindOnce(&MediaRecorderHandler::OnSourceReadyStateChanged,
                      WrapWeakPersistent(this)));
    audio_recorders_.emplace_back(std::make_unique<AudioTrackRecorder>(
        audio_codec_id_, audio_tracks_[0], std::move(on_encoded_audio_cb),
        std::move(on_track_source_changed_cb), audio_bits_per_second_,
        audio_bitrate_mode_));
  }

  recording_ = true;
  return true;
}

void MediaRecorderHandler::Stop() {
  DCHECK(IsMainThread());
  // Don't check |recording_| since we can go directly from pause() to stop().

  invalidated_ = true;

  recording_ = false;
  timeslice_ = base::Milliseconds(0);
  video_recorders_.clear();
  audio_recorders_.clear();
  muxer_.reset();
}

void MediaRecorderHandler::Pause() {
  DCHECK(IsMainThread());
  DCHECK(recording_);
  recording_ = false;
  for (const auto& video_recorder : video_recorders_)
    video_recorder->Pause();
  for (const auto& audio_recorder : audio_recorders_)
    audio_recorder->Pause();
  if (muxer_)
    muxer_->Pause();
}

void MediaRecorderHandler::Resume() {
  DCHECK(IsMainThread());
  DCHECK(!recording_);
  recording_ = true;
  for (const auto& video_recorder : video_recorders_)
    video_recorder->Resume();
  for (const auto& audio_recorder : audio_recorders_)
    audio_recorder->Resume();
  if (muxer_)
    muxer_->Resume();
}

void MediaRecorderHandler::EncodingInfo(
    const WebMediaConfiguration& configuration,
    OnMediaCapabilitiesEncodingInfoCallback callback) {
  DCHECK(IsMainThread());
  DCHECK(configuration.video_configuration ||
         configuration.audio_configuration);

  std::unique_ptr<WebMediaCapabilitiesInfo> info(
      new WebMediaCapabilitiesInfo());

  // TODO(mcasas): Support the case when both video and audio configurations are
  // specified: https://crbug.com/709181.
  String mime_type;
  String codec;
  if (configuration.video_configuration) {
    mime_type = configuration.video_configuration->mime_type;
    codec = configuration.video_configuration->codec;
  } else {
    mime_type = configuration.audio_configuration->mime_type;
    codec = configuration.audio_configuration->codec;
  }

  info->supported = CanSupportMimeType(mime_type, codec);

  if (configuration.video_configuration && info->supported) {
    const bool is_likely_accelerated =
        VideoTrackRecorderImpl::CanUseAcceleratedEncoder(
            VideoStringToCodecProfile(codec).codec_id,
            configuration.video_configuration->width,
            configuration.video_configuration->height,
            configuration.video_configuration->framerate);

    const float pixels_per_second =
        configuration.video_configuration->width *
        configuration.video_configuration->height *
        configuration.video_configuration->framerate;
    // Encoding is considered |smooth| up and until the pixels per second
    // threshold or if it's likely to be accelerated.
    const float threshold = base::SysInfo::IsLowEndDevice()
                                ? kNumPixelsPerSecondSmoothnessThresholdLow
                                : kNumPixelsPerSecondSmoothnessThresholdHigh;
    info->smooth = is_likely_accelerated || pixels_per_second <= threshold;

    // TODO(mcasas): revisit what |power_efficient| means
    // https://crbug.com/709181.
    info->power_efficient = info->smooth;
  }
  DVLOG(1) << "type: " << mime_type.Ascii() << ", params:" << codec.Ascii()
           << " is" << (info->supported ? " supported" : " NOT supported")
           << " and" << (info->smooth ? " smooth" : " NOT smooth");

  std::move(callback).Run(std::move(info));
}

String MediaRecorderHandler::ActualMimeType() {
  DCHECK(IsMainThread());
  DCHECK(recorder_) << __func__ << " should be called after Initialize()";

  const bool has_video_tracks = media_stream_->NumberOfVideoComponents();
  const bool has_audio_tracks = media_stream_->NumberOfAudioComponents();
  if (!has_video_tracks && !has_audio_tracks)
    return String();

  StringBuilder mime_type;
  if (!has_video_tracks && has_audio_tracks) {
    mime_type.Append("audio/webm;codecs=");
  } else {
    switch (video_codec_profile_.codec_id) {
      case VideoTrackRecorder::CodecId::kVp8:
      case VideoTrackRecorder::CodecId::kVp9:
        mime_type.Append("video/webm;codecs=");
        break;
#if BUILDFLAG(RTC_USE_H264)
      case VideoTrackRecorder::CodecId::kH264:
        mime_type.Append("video/x-matroska;codecs=");
        break;
#endif
      case VideoTrackRecorder::CodecId::kLast:
        // Do nothing.
        break;
    }
  }
  if (has_video_tracks) {
    switch (video_codec_profile_.codec_id) {
      case VideoTrackRecorder::CodecId::kVp8:
        mime_type.Append("vp8");
        break;
      case VideoTrackRecorder::CodecId::kVp9:
        mime_type.Append("vp9");
        break;
#if BUILDFLAG(RTC_USE_H264)
      case VideoTrackRecorder::CodecId::kH264:
        mime_type.Append("avc1");
        if (video_codec_profile_.profile && video_codec_profile_.level) {
          mime_type.Append(
              media::BuildH264MimeSuffix(*video_codec_profile_.profile,
                                         *video_codec_profile_.level)
                  .c_str());
        }
        break;
#endif
      case VideoTrackRecorder::CodecId::kLast:
        DCHECK_NE(audio_codec_id_, AudioTrackRecorder::CodecId::kLast);
    }
  }
  if (has_video_tracks && has_audio_tracks) {
    if (video_codec_profile_.codec_id != VideoTrackRecorder::CodecId::kLast &&
        audio_codec_id_ != AudioTrackRecorder::CodecId::kLast) {
      mime_type.Append(",");
    }
  }
  if (has_audio_tracks) {
    switch (audio_codec_id_) {
      case AudioTrackRecorder::CodecId::kOpus:
        mime_type.Append("opus");
        break;
      case AudioTrackRecorder::CodecId::kPcm:
        mime_type.Append("pcm");
        break;
      case AudioTrackRecorder::CodecId::kAac:
        mime_type.Append("m4a.40.2");
        break;
      case AudioTrackRecorder::CodecId::kLast:
        DCHECK_NE(video_codec_profile_.codec_id,
                  VideoTrackRecorder::CodecId::kLast);
    }
  }
  return mime_type.ToString();
}

void MediaRecorderHandler::OnEncodedVideo(
    const media::Muxer::VideoParameters& params,
    std::string encoded_data,
    std::string encoded_alpha,
    base::TimeTicks timestamp,
    bool is_key_frame) {
  DCHECK(IsMainThread());

  if (invalidated_)
    return;

  auto params_with_codec = params;
  params_with_codec.codec =
      MediaVideoCodecFromCodecId(video_codec_profile_.codec_id);
  HandleEncodedVideo(params_with_codec, std::move(encoded_data),
                     std::move(encoded_alpha), timestamp, is_key_frame);
}

void MediaRecorderHandler::OnPassthroughVideo(
    const media::Muxer::VideoParameters& params,
    std::string encoded_data,
    std::string encoded_alpha,
    base::TimeTicks timestamp,
    bool is_key_frame) {
  DCHECK(IsMainThread());

  // Update |video_codec_profile_| so that ActualMimeType() works.
  video_codec_profile_.codec_id = CodecIdFromMediaVideoCodec(params.codec);
  HandleEncodedVideo(params, std::move(encoded_data), std::move(encoded_alpha),
                     timestamp, is_key_frame);
}

void MediaRecorderHandler::HandleEncodedVideo(
    const media::Muxer::VideoParameters& params,
    std::string encoded_data,
    std::string encoded_alpha,
    base::TimeTicks timestamp,
    bool is_key_frame) {
  DCHECK(IsMainThread());

  if (UpdateTracksAndCheckIfChanged()) {
    recorder_->OnError("Amount of tracks in MediaStream has changed.");
    return;
  }

  if (!last_seen_codec_.has_value())
    last_seen_codec_ = params.codec;
  if (*last_seen_codec_ != params.codec) {
    recorder_->OnError(
        String::Format("Video codec changed from %s to %s",
                       media::GetCodecName(*last_seen_codec_).c_str(),
                       media::GetCodecName(params.codec).c_str()));
    return;
  }

  if (!muxer_)
    return;
  if (!muxer_->OnEncodedVideo(params, std::move(encoded_data),
                              std::move(encoded_alpha), timestamp,
                              is_key_frame)) {
    DLOG(ERROR) << "Error muxing video data";
    recorder_->OnError("Error muxing video data");
  }
}

void MediaRecorderHandler::OnEncodedAudio(const media::AudioParameters& params,
                                          std::string encoded_data,
                                          base::TimeTicks timestamp) {
  DCHECK(IsMainThread());

  if (invalidated_)
    return;

  if (UpdateTracksAndCheckIfChanged()) {
    recorder_->OnError("Amount of tracks in MediaStream has changed.");
    return;
  }
  if (!muxer_)
    return;
  if (!muxer_->OnEncodedAudio(params, std::move(encoded_data), timestamp)) {
    DLOG(ERROR) << "Error muxing audio data";
    recorder_->OnError("Error muxing audio data");
  }
}

void MediaRecorderHandler::WriteData(base::StringPiece data) {
  DCHECK(IsMainThread());

  if (invalidated_)
    return;

  const base::TimeTicks now = base::TimeTicks::Now();
  // Non-buffered mode does not need to check timestamps.
  if (timeslice_.is_zero()) {
    recorder_->WriteData(
        data.data(), data.length(), true /* lastInSlice */,
        (now - base::TimeTicks::UnixEpoch()).InMillisecondsF());
    return;
  }

  const bool last_in_slice = now > slice_origin_timestamp_ + timeslice_;
  DVLOG_IF(1, last_in_slice) << "Slice finished @ " << now;
  if (last_in_slice)
    slice_origin_timestamp_ = now;
  recorder_->WriteData(data.data(), data.length(), last_in_slice,
                       (now - base::TimeTicks::UnixEpoch()).InMillisecondsF());
}

bool MediaRecorderHandler::UpdateTracksAndCheckIfChanged() {
  DCHECK(IsMainThread());

  const auto video_tracks = media_stream_->VideoComponents();
  const auto audio_tracks = media_stream_->AudioComponents();

  bool video_tracks_changed = video_tracks_.size() != video_tracks.size();
  bool audio_tracks_changed = audio_tracks_.size() != audio_tracks.size();

  if (!video_tracks_changed) {
    for (wtf_size_t i = 0; i < video_tracks.size(); ++i) {
      if (video_tracks_[i]->Id() != video_tracks[i]->Id()) {
        video_tracks_changed = true;
        break;
      }
    }
  }
  if (!video_tracks_changed && !audio_tracks_changed) {
    for (wtf_size_t i = 0; i < audio_tracks.size(); ++i) {
      if (audio_tracks_[i]->Id() != audio_tracks[i]->Id()) {
        audio_tracks_changed = true;
        break;
      }
    }
  }

  if (video_tracks_changed)
    video_tracks_ = video_tracks;
  if (audio_tracks_changed)
    audio_tracks_ = audio_tracks;

  if (video_tracks_.size())
    UpdateTrackLiveAndEnabled(*video_tracks_[0], /*is_video=*/true);
  if (audio_tracks_.size())
    UpdateTrackLiveAndEnabled(*audio_tracks_[0], /*is_video=*/false);

  return video_tracks_changed || audio_tracks_changed;
}

void MediaRecorderHandler::UpdateTrackLiveAndEnabled(
    const MediaStreamComponent& track,
    bool is_video) {
  const bool track_live_and_enabled =
      track.GetReadyState() == MediaStreamSource::kReadyStateLive &&
      track.Enabled();
  if (muxer_)
    muxer_->SetLiveAndEnabled(track_live_and_enabled, is_video);
}

void MediaRecorderHandler::OnSourceReadyStateChanged() {
  for (const auto& track : video_tracks_) {
    DCHECK(track->Source());
    if (track->GetReadyState() != MediaStreamSource::kReadyStateEnded)
      return;
  }
  for (const auto& track : audio_tracks_) {
    DCHECK(track->Source());
    if (track->GetReadyState() != MediaStreamSource::kReadyStateEnded)
      return;
  }
  // All tracks are ended, so stop the recorder in accordance with
  // https://www.w3.org/TR/mediastream-recording/#mediarecorder-methods.
  recorder_->OnAllTracksEnded();
}

void MediaRecorderHandler::OnVideoFrameForTesting(
    scoped_refptr<media::VideoFrame> frame,
    const TimeTicks& timestamp) {
  for (const auto& recorder : video_recorders_)
    recorder->OnVideoFrameForTesting(frame, timestamp);
}

void MediaRecorderHandler::OnEncodedVideoFrameForTesting(
    scoped_refptr<EncodedVideoFrame> frame,
    const base::TimeTicks& timestamp) {
  for (const auto& recorder : video_recorders_)
    recorder->OnEncodedVideoFrameForTesting(frame, timestamp);
}

void MediaRecorderHandler::OnAudioBusForTesting(
    const media::AudioBus& audio_bus,
    const base::TimeTicks& timestamp) {
  for (const auto& recorder : audio_recorders_)
    recorder->OnData(audio_bus, timestamp);
}

void MediaRecorderHandler::SetAudioFormatForTesting(
    const media::AudioParameters& params) {
  for (const auto& recorder : audio_recorders_)
    recorder->OnSetFormat(params);
}

void MediaRecorderHandler::Trace(Visitor* visitor) const {
  visitor->Trace(media_stream_);
  visitor->Trace(video_tracks_);
  visitor->Trace(audio_tracks_);
  visitor->Trace(recorder_);
}

}  // namespace blink
