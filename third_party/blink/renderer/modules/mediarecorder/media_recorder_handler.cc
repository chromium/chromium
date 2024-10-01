// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/mediarecorder/media_recorder_handler.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_parameters.h"
#include "media/base/decoder_buffer.h"
#include "media/base/mime_util.h"
#include "media/base/video_codec_string_parsers.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/formats/mp4/mp4_status.h"
#include "media/media_buildflags.h"
#include "media/mojo/clients/mojo_audio_encoder.h"
#include "media/mojo/clients/mojo_video_encoder_metrics_provider.h"
#include "media/muxers/live_webm_muxer_delegate.h"
#include "media/muxers/mp4_muxer.h"
#include "media/muxers/mp4_muxer_delegate.h"
#include "media/muxers/muxer.h"
#include "media/muxers/muxer_timestamp_adapter.h"
#include "media/muxers/webm_muxer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/mediarecorder/media_recorder.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/media_capabilities/web_media_capabilities_info.h"
#include "third_party/blink/renderer/platform/media_capabilities/web_media_configuration.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/mediastream/webrtc_uma_histograms.h"
#include "third_party/blink/renderer/platform/network/mime/content_type.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

#if BUILDFLAG(IS_WIN)
#include "media/gpu/windows/mf_audio_encoder.h"
#endif

using base::TimeTicks;

namespace blink {

BASE_FEATURE(kMediaRecorderEnableMp4Muxer,
             "MediaRecorderEnableMp4Muxer",
             base::FEATURE_ENABLED_BY_DEFAULT);
namespace {

constexpr double kDefaultVideoFrameRate = 30.0;

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
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    case media::VideoCodec::kH264:
      return VideoTrackRecorder::CodecId::kH264;
#endif
    case media::VideoCodec::kAV1:
      return VideoTrackRecorder::CodecId::kAv1;
    default:
      return VideoTrackRecorder::CodecId::kLast;
  }
  NOTREACHED_IN_MIGRATION() << "Unsupported video codec";
  return VideoTrackRecorder::CodecId::kLast;
}

media::VideoCodec MediaVideoCodecFromCodecId(VideoTrackRecorder::CodecId id) {
  switch (id) {
    case VideoTrackRecorder::CodecId::kVp8:
      return media::VideoCodec::kVP8;
    case VideoTrackRecorder::CodecId::kVp9:
      return media::VideoCodec::kVP9;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    case VideoTrackRecorder::CodecId::kH264:
      return media::VideoCodec::kH264;
#endif
    case VideoTrackRecorder::CodecId::kAv1:
      return media::VideoCodec::kAV1;
    case VideoTrackRecorder::CodecId::kLast:
      return media::VideoCodec::kUnknown;
  }
  NOTREACHED_IN_MIGRATION() << "Unsupported video codec";
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
  NOTREACHED_IN_MIGRATION() << "Unsupported audio codec";
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
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
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
    if (auto result = media::ParseAVCCodecId(avc1_str.Ascii())) {
      return {codec_id, result->profile, result->level};
    }
  }
#endif
  // TODO(crbug.com/1465734): Remove the wrong AV1 codecs string, "av1", once
  // we confirm nobody uses this in product.
  if (codecs_str.Find("av01") != kNotFound ||
      codecs_str.Find("av1") != kNotFound) {
    codec_id = VideoTrackRecorder::CodecId::kAv1;
  }
  return VideoTrackRecorder::CodecProfile(codec_id);
}

AudioTrackRecorder::CodecId AudioStringToCodecId(const String& codecs) {
  String codecs_str = codecs.LowerASCII();

  if (codecs_str.Find("opus") != kNotFound)
    return AudioTrackRecorder::CodecId::kOpus;
  if (codecs_str.Find("pcm") != kNotFound)
    return AudioTrackRecorder::CodecId::kPcm;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (codecs_str.Find("mp4a.40.2") != kNotFound) {
    return AudioTrackRecorder::CodecId::kAac;
  }
#endif
  return AudioTrackRecorder::CodecId::kLast;
}

bool CanSupportVideoType(const String& type) {
  bool support = EqualIgnoringASCIICase(type, "video/webm") ||
                 EqualIgnoringASCIICase(type, "video/x-matroska");
  if (support) {
    return true;
  }

  if (base::FeatureList::IsEnabled(kMediaRecorderEnableMp4Muxer)) {
    return EqualStringView(type, "video/mp4");
  }

  return false;
}

bool CanSupportAudioType(const String& type) {
  bool support = EqualIgnoringASCIICase(type, "audio/webm");
  if (support) {
    return true;
  }

  if (base::FeatureList::IsEnabled(kMediaRecorderEnableMp4Muxer)) {
    return EqualStringView(type, "audio/mp4");
  }

  return false;
}

bool IsAllowedMp4Type(const String& type) {
  return EqualIgnoringASCIICase(type, "video/mp4") ||
         EqualIgnoringASCIICase(type, "audio/mp4");
}

bool IsMp4MuxerRequired(const String& type) {
  // The function should be called only after type and codecs are validated
  // by `CanSupportMimeType()` first in code path.
  if (!base::FeatureList::IsEnabled(kMediaRecorderEnableMp4Muxer)) {
    return false;
  }
  return IsAllowedMp4Type(type);
}

}  // anonymous namespace

MediaRecorderHandler::MediaRecorderHandler(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    KeyFrameRequestProcessor::Configuration key_frame_config)
    : key_frame_config_(key_frame_config),
      main_thread_task_runner_(std::move(main_thread_task_runner)) {}

bool MediaRecorderHandler::CanSupportMimeType(const String& type,
                                              const String& web_codecs) {
  DCHECK(IsMainThread());
  // An empty |type| means MediaRecorderHandler can choose its preferred codecs.
  if (type.empty())
    return true;

  const bool video = CanSupportVideoType(type);
  const bool audio = !video && CanSupportAudioType(type);
  if (!video && !audio)
    return false;

  // Both |video| and |audio| support empty |codecs|; |type| == "video" supports
  // vp8, vp9, h264, avc1, av1 or opus; |type| = "audio", supports opus or pcm
  // (little-endian 32-bit float).
  // http://www.webmproject.org/docs/container Sec:"HTML5 Video Type Parameters"
  static const char* const kVideoCodecs[] = {
    "vp8",
    "vp9",
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    "h264",
    "avc1",
#endif
    "av01",
    // TODO(crbug.com/1465734): Remove the wrong AV1 codecs string, "av1", once
    // we confirm nobody uses this in product.
    "av1",
    "opus",
    "pcm"
  };
  static const char* const kAudioCodecs[] = {"opus", "pcm"};

  auto* const* relevant_codecs_begin =
      video ? std::begin(kVideoCodecs) : std::begin(kAudioCodecs);
  auto* const* relevant_codecs_end =
      video ? std::end(kVideoCodecs) : std::end(kAudioCodecs);

  bool mp4_mime_type = false;

  mp4_mime_type = IsAllowedMp4Type(type);
  if (mp4_mime_type) {
    static const char* const kVideoCodecsForMP4[] = {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
        "avc1",
        "mp4a.40.2",
#endif
        "vp9",
        "av01",
        "opus",
    };
    static const char* const kAudioCodecsForMp4[] = {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
        "mp4a.40.2",
#endif
        "opus"};

    relevant_codecs_begin =
        video ? std::begin(kVideoCodecsForMP4) : std::begin(kAudioCodecsForMp4);
    relevant_codecs_end =
        video ? std::end(kVideoCodecsForMP4) : std::end(kAudioCodecsForMp4);
  }

  std::vector<std::string> codecs_list;
  media::SplitCodecs(web_codecs.Utf8(), &codecs_list);

  // Only the mp4a.40.2 codec for aac is supported by the platform,
  // it retains the entire input string when dealing with an mp4 container.
  if (!mp4_mime_type) {
    media::StripCodecs(&codecs_list);
  }

  for (const auto& codec : codecs_list) {
    String codec_string = String::FromUTF8(codec);

    bool match =
        std::any_of(relevant_codecs_begin, relevant_codecs_end,
                    [&codec_string, &mp4_mime_type](const char* name) {
                      if (mp4_mime_type) {
                        return EqualStringView(codec_string, name);
                      } else {
                        return EqualIgnoringASCIICase(codec_string, name);
                      }
                    });

    if (!match && mp4_mime_type && video) {
      // It supports full qualified string for `avc1` and `av01` codecs, e.g.
      //  `avc1.<profile>.<level>`, `av01.<profile>.<level>.<color depth>.*`.
      auto parsed_result =
          media::ParseVideoCodecString(type.Ascii(), codec_string.Ascii(),
                                       /*allow_ambiguous_matches=*/false);
      match =
          parsed_result && (parsed_result->codec == media::VideoCodec::kH264 ||
                            parsed_result->codec == media::VideoCodec::kAV1);
    }

    if (!match) {
      return false;
    }

    if (codec_string == "mp4a.40.2" &&
        !media::MojoAudioEncoder::IsSupported(media::AudioCodec::kAAC)) {
      return false;
    }

    if (codec_string == "av01" || codec_string == "av1") {
      base::UmaHistogramBoolean("Media.MediaRecorder.HasCorrectAV1CodecString",
                                codec_string == "av01");
#if !BUILDFLAG(ENABLE_LIBAOM)
      // The software encoder is unable to process the kAV1 codec if
      // ENABLE_LIBAOM is not defined. It verifies hardware encoding supports is
      // doable.
      VideoTrackRecorder::CodecProfile codec_profile =
          VideoStringToCodecProfile(codec_string);
      if (!VideoTrackRecorderImpl::CanUseAcceleratedEncoder(
              // The CanUseAcceleratedEncoder function requires a frame size for
              // validation. However, at this point, we don’t have the frame
              // size available. We’re making an assumption that it exceeds the
              // minimum size.
              codec_profile,
              video_track_recorder::kVEAEncoderMinResolutionWidth,
              video_track_recorder::kVEAEncoderMinResolutionHeight)) {
        return false;
      }
#endif
    }
  }
  return true;
}

bool MediaRecorderHandler::Initialize(
    MediaRecorder* recorder,
    MediaStreamDescriptor* media_stream,
    const String& type,
    const String& codecs,
    AudioTrackRecorder::BitrateMode audio_bitrate_mode) {
  DCHECK(IsMainThread());
  // Save histogram data so we can see how much MediaStream Recorder is used.
  // The histogram counts the number of calls to the JS API.
  UpdateWebRTCMethodCount(RTCAPIName::kMediaStreamRecorder);

  type_ = type;

  if (!CanSupportMimeType(type_, codecs)) {
    DLOG(ERROR) << "Unsupported " << type.Utf8() << ";codecs=" << codecs.Utf8();
    return false;
  }

  passthrough_enabled_ = type_.empty();

  // Once established that we support the codec(s), hunt then individually.
  video_codec_profile_ = VideoStringToCodecProfile(codecs);
  if (video_codec_profile_.codec_id == VideoTrackRecorder::CodecId::kLast) {
    MediaTrackContainerType container_type =
        GetMediaContainerTypeFromString(type_);
    video_codec_profile_.codec_id =
        VideoTrackRecorderImpl::GetPreferredCodecId(container_type);
    DVLOG(1) << "Falling back to preferred video codec id "
             << static_cast<int>(video_codec_profile_.codec_id);
  }

  // Do the same for the audio codec(s).
  const AudioTrackRecorder::CodecId audio_codec_id =
      AudioStringToCodecId(codecs);

  if (audio_codec_id == AudioTrackRecorder::CodecId::kLast) {
    MediaTrackContainerType container_type =
        GetMediaContainerTypeFromString(type_);
    audio_codec_id_ = AudioTrackRecorder::GetPreferredCodecId(container_type);
  } else {
    audio_codec_id_ = audio_codec_id;
  }

  DVLOG_IF(1, audio_codec_id == AudioTrackRecorder::CodecId::kLast)
      << "Falling back to preferred audio codec id "
      << static_cast<int>(audio_codec_id_);

  media_stream_ = media_stream;
  DCHECK(recorder);
  recorder_ = recorder;

  audio_bitrate_mode_ = audio_bitrate_mode;
  return true;
}

AudioTrackRecorder::BitrateMode MediaRecorderHandler::AudioBitrateMode() {
  return audio_bitrate_mode_;
}

bool MediaRecorderHandler::Start(int timeslice,
                                 const String& type,
                                 uint32_t audio_bits_per_second,
                                 uint32_t video_bits_per_second) {
  DCHECK(IsMainThread());
  DCHECK(!recording_);
  DCHECK(media_stream_);
  DCHECK(timeslice_.is_zero());
  DCHECK(!muxer_adapter_);

  DCHECK(!is_media_stream_observer_);
  media_stream_->AddObserver(this);
  is_media_stream_observer_ = true;

  timeslice_ = base::Milliseconds(timeslice);
  slice_origin_timestamp_ = base::TimeTicks::Now();

  audio_bits_per_second_ = audio_bits_per_second;
  video_bits_per_second_ = video_bits_per_second;

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

  const bool use_mp4_muxer = IsMp4MuxerRequired(type);

  // For each track in tracks, if the User Agent cannot record the track using
  // the current configuration, abort. See step 14 in
  // https://w3c.github.io/mediacapture-record/MediaRecorder.html#dom-mediarecorder-start
  if (!type.empty()) {
    const bool video_type_supported = CanSupportVideoType(type);
    const bool audio_type_supported = CanSupportAudioType(type);
    if (use_video_tracks && !video_type_supported) {
      return false;
    }
    if (use_audio_tracks && !(video_type_supported || audio_type_supported)) {
      return false;
    }

    if (use_mp4_muxer &&
        !base::FeatureList::IsEnabled(kMediaRecorderEnableMp4Muxer)) {
      return false;
    }
  }

  std::unique_ptr<media::Muxer> muxer;
  media::AudioCodec audio_codec = CodecIdToMediaAudioCodec(audio_codec_id_);
  std::optional<base::TimeDelta> optional_timeslice;
  if (timeslice > 0) {
    optional_timeslice = timeslice_;
  }

  auto write_callback =
      WTF::BindRepeating(&MediaRecorderHandler::WriteData,
                         WrapPersistent(weak_factory_.GetWeakCell()));
  if (use_mp4_muxer) {
    muxer = std::make_unique<media::Mp4Muxer>(
        audio_codec, use_video_tracks, use_audio_tracks,
        std::make_unique<media::Mp4MuxerDelegate>(
            audio_codec, video_codec_profile_.profile,
            video_codec_profile_.level, write_callback),
        optional_timeslice);

#if BUILDFLAG(IS_WIN)
    // Windows OS uses MediaFoundation for MP4 muxing, which requires the
    // specific audio bit rate for AAC encoding.
    if (audio_bits_per_second_ != 0u) {
      audio_bits_per_second_ =
          media::MFAudioEncoder::ClampAccCodecBitrate(audio_bits_per_second_);
      recorder_->UpdateAudioBitrate(audio_bits_per_second_);
    }
#endif
  } else {
    muxer = std::make_unique<media::WebmMuxer>(
        audio_codec, use_video_tracks, use_audio_tracks,
        std::make_unique<media::LiveWebmMuxerDelegate>(write_callback),
        optional_timeslice);
  }
  muxer_adapter_ = std::make_unique<media::MuxerTimestampAdapter>(
      std::move(muxer), use_video_tracks, use_audio_tracks);

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
    const bool use_encoded_source_output =
        video_track->source() != nullptr &&
        video_track->source()->SupportsEncodedOutput();
    if (passthrough_enabled_ && use_encoded_source_output) {
      video_recorders_.emplace_back(
          std::make_unique<VideoTrackRecorderPassthrough>(
              main_thread_task_runner_, video_tracks_[0],
              weak_video_factory_.GetWeakCell(), key_frame_config_));
    } else {
      video_recorders_.emplace_back(std::make_unique<VideoTrackRecorderImpl>(
          main_thread_task_runner_, video_codec_profile_, video_tracks_[0],
          weak_video_factory_.GetWeakCell(), video_bits_per_second_,
          key_frame_config_));
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

    audio_recorders_.emplace_back(std::make_unique<AudioTrackRecorder>(
        main_thread_task_runner_, audio_codec_id_, audio_tracks_[0],
        weak_audio_factory_.GetWeakCell(), audio_bits_per_second_,
        audio_bitrate_mode_));
  }

  recording_ = true;
  return true;
}

void MediaRecorderHandler::Stop() {
  DCHECK(IsMainThread());
  // Don't check |recording_| since we can go directly from pause() to stop().

  // TODO(crbug.com/719023): The video recorder needs to be flushed to retrieve
  // the last N frames with some codecs.

  // Unregister from media stream notifications.
  if (media_stream_ && is_media_stream_observer_) {
    media_stream_->RemoveObserver(this);
  }
  is_media_stream_observer_ = false;

  // Ensure any stored data inside the muxer is flushed out before invalidation.
  muxer_adapter_ = nullptr;
  weak_audio_factory_.Invalidate();
  weak_video_factory_.Invalidate();
  weak_factory_.Invalidate();

  recording_ = false;
  timeslice_ = base::Milliseconds(0);
  video_recorders_.clear();
  audio_recorders_.clear();
}

void MediaRecorderHandler::Pause() {
  DCHECK(IsMainThread());
  DCHECK(recording_);
  recording_ = false;
  for (const auto& video_recorder : video_recorders_)
    video_recorder->Pause();
  for (const auto& audio_recorder : audio_recorders_)
    audio_recorder->Pause();
  if (muxer_adapter_) {
    muxer_adapter_->Pause();
  }
}

void MediaRecorderHandler::Resume() {
  DCHECK(IsMainThread());
  DCHECK(!recording_);
  recording_ = true;
  for (const auto& video_recorder : video_recorders_)
    video_recorder->Resume();
  for (const auto& audio_recorder : audio_recorders_)
    audio_recorder->Resume();
  if (muxer_adapter_) {
    muxer_adapter_->Resume();
  }
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
    VideoTrackRecorder::CodecProfile codec_profile =
        VideoStringToCodecProfile(codec);
    const bool is_likely_accelerated =
        VideoTrackRecorderImpl::CanUseAcceleratedEncoder(
            codec_profile, configuration.video_configuration->width,
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
    if (passthrough_enabled_) {
      DCHECK(type_.empty());
      mime_type.Append("audio/webm");
    } else {
      mime_type.Append(type_.Characters8(), type_.length());
    }
    mime_type.Append(";codecs=");
  } else {
    switch (video_codec_profile_.codec_id) {
      case VideoTrackRecorder::CodecId::kVp8:
      case VideoTrackRecorder::CodecId::kVp9:
      case VideoTrackRecorder::CodecId::kAv1:
        if (passthrough_enabled_) {
          mime_type.Append("video/webm");
        } else {
          mime_type.Append(type_.Characters8(), type_.length());
        }
        mime_type.Append(";codecs=");
        break;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      case VideoTrackRecorder::CodecId::kH264:
        if (!passthrough_enabled_ &&
            EqualIgnoringASCIICase(type_, "video/mp4")) {
          mime_type.Append(type_.Characters8(), type_.length());
        } else {
          mime_type.Append("video/x-matroska");
        }
        mime_type.Append(";codecs=");
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
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
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
      case VideoTrackRecorder::CodecId::kAv1:
        mime_type.Append("av01");
        break;
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
        mime_type.Append("mp4a.40.2");
        break;
      case AudioTrackRecorder::CodecId::kLast:
        DCHECK_NE(video_codec_profile_.codec_id,
                  VideoTrackRecorder::CodecId::kLast);
    }
  }
  return mime_type.ToString();
}

void MediaRecorderHandler::TrackAdded(const WebString& track_id) {
  OnStreamChanged("Tracks in MediaStream were added.");
}

void MediaRecorderHandler::TrackRemoved(const WebString& track_id) {
  OnStreamChanged("Tracks in MediaStream were removed.");
}

void MediaRecorderHandler::OnStreamChanged(const String& message) {
  if (recorder_) {
    // The call to MediaRecorder::OnStreamChanged has to be posted because
    // otherwise stream track set changing leads to the MediaRecorder
    // synchronously changing state to "inactive", which contradicts
    // https://www.w3.org/TR/mediastream-recording/#dom-mediarecorder-start
    // step 14.4.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, WTF::BindOnce(&MediaRecorder::OnStreamChanged,
                                 WrapWeakPersistent(recorder_.Get()), message));
  }
}

void MediaRecorderHandler::OnEncodedVideo(
    const media::Muxer::VideoParameters& params,
    scoped_refptr<media::DecoderBuffer> encoded_data,
    std::optional<media::VideoEncoder::CodecDescription> codec_description,
    base::TimeTicks timestamp) {
  DCHECK(IsMainThread());

  if (encoded_data->empty()) {
    // An encoder drops a frame. This can happen with VideoToolBox encoder as
    // there is no way to disallow the frame dropping with it.
    return;
  }

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  // TODO(crbug.com/40266540). Once Encoder supports VideoEncoder, then the
  // below code could go away.

  // Convert annex stream to avc bit stream for h264.
  if (video_codec_profile_.codec_id == VideoTrackRecorder::CodecId::kH264 &&
      encoded_data->is_key_frame() && !codec_description.has_value()) {
    bool first_key_frame = false;
    if (!h264_converter_) {
      h264_converter_ =
          std::make_unique<media::H264AnnexBToAvcBitstreamConverter>();
      first_key_frame = true;
    }

    // We don't care the config_changed or not, we just pass the configuration
    // data as a codec_descriptions.
    bool config_changed = false;
    size_t desired_size = 0;
    std::vector<uint8_t> output_chunk;

    auto status = h264_converter_->ConvertChunk(
        encoded_data->AsSpan(), output_chunk, &config_changed, &desired_size);
    CHECK_EQ(status.code(), media::MP4Status::Codes::kBufferTooSmall);
    output_chunk.resize(desired_size);
    status = h264_converter_->ConvertChunk(encoded_data->AsSpan(), output_chunk,
                                           &config_changed, &desired_size);
    DCHECK(status.is_ok());

    const auto& config = h264_converter_->GetCurrentConfig();
    media::VideoEncoder::CodecDescription avc_config_data;
    if (!config.Serialize(avc_config_data)) {
      DVLOG(1) << "Failed to get h264 config";
    }
    codec_description = std::move(avc_config_data);

    if (first_key_frame) {
      video_codec_profile_.level = config.avc_level;
    }
  }
#endif

  auto params_with_codec = params;
  params_with_codec.codec =
      MediaVideoCodecFromCodecId(video_codec_profile_.codec_id);
  if (!params_with_codec.frame_rate) {
    params_with_codec.frame_rate = kDefaultVideoFrameRate;
  }

  HandleEncodedVideo(params_with_codec, std::move(encoded_data),
                     std::move(codec_description), timestamp);
}

void MediaRecorderHandler::OnPassthroughVideo(
    const media::Muxer::VideoParameters& params,
    scoped_refptr<media::DecoderBuffer> encoded_data,
    base::TimeTicks timestamp) {
  DCHECK(IsMainThread());

  // Update |video_codec_profile_| so that ActualMimeType() works.
  video_codec_profile_.codec_id = CodecIdFromMediaVideoCodec(params.codec);
  HandleEncodedVideo(params, std::move(encoded_data), std::nullopt, timestamp);
}

void MediaRecorderHandler::HandleEncodedVideo(
    const media::Muxer::VideoParameters& params,
    scoped_refptr<media::DecoderBuffer> encoded_data,
    std::optional<media::VideoEncoder::CodecDescription> codec_description,
    base::TimeTicks timestamp) {
  DCHECK(IsMainThread());

  if (!last_seen_codec_.has_value())
    last_seen_codec_ = params.codec;
  if (*last_seen_codec_ != params.codec) {
    recorder_->OnError(
        DOMExceptionCode::kUnknownError,
        String::Format("Video codec changed from %s to %s",
                       media::GetCodecName(*last_seen_codec_).c_str(),
                       media::GetCodecName(params.codec).c_str()));
    return;
  }
  if (!muxer_adapter_) {
    return;
  }
  if (!muxer_adapter_->OnEncodedVideo(params, std::move(encoded_data),
                                      std::move(codec_description),
                                      timestamp)) {
    recorder_->OnError(DOMExceptionCode::kUnknownError,
                       "Error muxing video data");
  }
}

void MediaRecorderHandler::OnEncodedAudio(
    const media::AudioParameters& params,
    scoped_refptr<media::DecoderBuffer> encoded_data,
    std::optional<media::AudioEncoder::CodecDescription> codec_description,
    base::TimeTicks timestamp) {
  DCHECK(IsMainThread());

  if (!muxer_adapter_) {
    return;
  }
  if (!muxer_adapter_->OnEncodedAudio(params, std::move(encoded_data),
                                      std::move(codec_description),
                                      timestamp)) {
    recorder_->OnError(DOMExceptionCode::kUnknownError,
                       "Error muxing audio data");
  }
}

void MediaRecorderHandler::OnAudioEncodingError(
    media::EncoderStatus error_status) {
  DCHECK(IsMainThread());
  recorder_->OnError(DOMExceptionCode::kEncodingError,
                     String(media::EncoderStatusCodeToString(error_status)));
}

std::unique_ptr<media::VideoEncoderMetricsProvider>
MediaRecorderHandler::CreateVideoEncoderMetricsProvider() {
  DCHECK(IsMainThread());
  mojo::PendingRemote<media::mojom::VideoEncoderMetricsProvider>
      video_encoder_metrics_provider;
  recorder_->DomWindow()->GetFrame()->GetBrowserInterfaceBroker().GetInterface(
      video_encoder_metrics_provider.InitWithNewPipeAndPassReceiver());
  return base::MakeRefCounted<media::MojoVideoEncoderMetricsProviderFactory>(
             media::mojom::VideoEncoderUseCase::kMediaRecorder,
             std::move(video_encoder_metrics_provider))
      ->CreateVideoEncoderMetricsProvider();
}

void MediaRecorderHandler::WriteData(base::span<const uint8_t> data) {
  DCHECK(IsMainThread());
  DVLOG(3) << __func__ << " " << data.size() << "B";

  const base::TimeTicks now = base::TimeTicks::Now();
  const bool last_in_slice =
      timeslice_.is_zero() ? true : now > slice_origin_timestamp_ + timeslice_;
  DVLOG_IF(1, last_in_slice) << "Slice finished @ " << now;
  if (last_in_slice) {
    slice_origin_timestamp_ = now;
  }
  recorder_->WriteData(data, last_in_slice, /*error_event=*/nullptr);
}

void MediaRecorderHandler::UpdateTracksLiveAndEnabled() {
  DCHECK(IsMainThread());

  if (!video_tracks_.empty()) {
    UpdateTrackLiveAndEnabled(*video_tracks_[0], /*is_video=*/true);
  }
  if (!audio_tracks_.empty()) {
    UpdateTrackLiveAndEnabled(*audio_tracks_[0], /*is_video=*/false);
  }
}

void MediaRecorderHandler::UpdateTrackLiveAndEnabled(
    const MediaStreamComponent& track,
    bool is_video) {
  const bool track_live_and_enabled =
      track.GetReadyState() == MediaStreamSource::kReadyStateLive &&
      track.Enabled();
  if (muxer_adapter_) {
    muxer_adapter_->SetLiveAndEnabled(track_live_and_enabled, is_video);
  }
}

void MediaRecorderHandler::OnSourceReadyStateChanged() {
  MediaStream* stream = ToMediaStream(media_stream_);
  for (const auto& track : stream->getTracks()) {
    if (track->readyState() != "ended") {
      return;
    }
  }
  // All tracks are ended, so stop the recorder in accordance with
  // https://www.w3.org/TR/mediastream-recording/#mediarecorder-methods.
  recorder_->OnAllTracksEnded();
}

void MediaRecorderHandler::OnVideoFrameForTesting(
    scoped_refptr<media::VideoFrame> frame,
    const TimeTicks& timestamp) {
  for (const auto& recorder : video_recorders_) {
    recorder->OnVideoFrameForTesting(frame, timestamp,
                                     /*allow_vea_encoder=*/true);
  }
}

void MediaRecorderHandler::OnEncodedVideoFrameForTesting(
    scoped_refptr<EncodedVideoFrame> frame,
    const base::TimeTicks& timestamp) {
  for (const auto& recorder : video_recorders_) {
    recorder->OnEncodedVideoFrameForTesting(base::TimeTicks::Now(), frame,
                                            timestamp);
  }
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
  visitor->Trace(weak_audio_factory_);
  visitor->Trace(weak_video_factory_);
  visitor->Trace(weak_factory_);
}

void MediaRecorderHandler::OnVideoEncodingError() {
  if (recorder_) {
    recorder_->OnError(DOMExceptionCode::kUnknownError,
                       "Video encoding failed.");
  }
}

}  // namespace blink
