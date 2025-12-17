// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/media_recorder_handler.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
#include "media/base/media_serializers_base.h"
#include "media/base/media_switches.h"
#include "media/base/mime_util.h"
#include "media/base/supported_types.h"
#include "media/base/video_codec_string_parsers.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/formats/mp4/mp4_status.h"
#include "media/media_buildflags.h"
#include "media/mojo/clients/mojo_audio_encoder.h"
#include "media/mojo/clients/mojo_video_encoder_metrics_provider.h"
#include "media/muxers/live_webm_muxer_delegate.h"
#include "media/muxers/memory_webm_muxer_delegate.h"
#include "media/muxers/mp4_muxer.h"
#include "media/muxers/mp4_muxer_delegate.h"
#include "media/muxers/muxer.h"
#include "media/muxers/muxer_timestamp_adapter.h"
#include "media/muxers/webm_muxer.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_track_state.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/mediarecorder/media_recorder.h"
#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"
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

BASE_FEATURE(kMediaRecorderSeekableWebm, base::FEATURE_ENABLED_BY_DEFAULT);

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

#if BUILDFLAG(USE_PROPRIETARY_CODECS) || \
    BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
std::optional<VideoTrackRecorder::CodecProfile> VideoStringTagToCodecProfile(
    const String& codecs,
    const std::vector<StringView>& codecs_tags) {
  std::optional<VideoTrackRecorder::CodecProfile> codec_profile;
  for (auto& codecs_tag : codecs_tags) {
    wtf_size_t codecs_start = codecs.Find(codecs_tag);
    if (codecs_start != kNotFound) {
      wtf_size_t codecs_end = codecs.Find(",");
      auto codec_id =
          codecs
              .Substring(codecs_start,
                         codecs_end == kNotFound ? UINT_MAX : codecs_end)
              .StripWhiteSpace()
              .Ascii();
      // Do not use lowercase `codecId` here, as `codecId` is case sensitive
      // when parsing.
      if (auto result = media::ParseCodec(codec_id)) {
        codec_profile = {result->codec, result->profile, result->level};
        break;
      }
    }
  }
  return codec_profile;
}
#endif

media::AudioCodec AudioStringToAudioCodec(const String& codecs) {
  String codecs_str = codecs.LowerASCII();

  if (codecs_str.Find("opus") != kNotFound)
    return media::AudioCodec::kOpus;
  if (codecs_str.Find("pcm") != kNotFound)
    return media::AudioCodec::kPCM;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (codecs_str.Find("mp4a.40.2") != kNotFound) {
    return media::AudioCodec::kAAC;
  }
#endif
  return media::AudioCodec::kUnknown;
}

bool CanSupportVideoType(const String& type) {
  return EqualIgnoringASCIICase(type, "video/webm") ||
         EqualIgnoringASCIICase(type, "video/x-matroska") ||
         EqualIgnoringASCIICase(type, "video/matroska") ||
         EqualStringView(type, "video/mp4");
}

bool CanSupportAudioType(const String& type) {
  return EqualIgnoringASCIICase(type, "audio/webm") ||
         EqualIgnoringASCIICase(type, "audio/x-matroska") ||
         EqualIgnoringASCIICase(type, "audio/matroska") ||
         EqualStringView(type, "audio/mp4");
}

bool IsAllowedMp4Type(const String& type) {
  return EqualIgnoringASCIICase(type, "video/mp4") ||
         EqualIgnoringASCIICase(type, "audio/mp4");
}

bool IsMp4MuxerRequired(const String& type) {
  // The function should be called only after type and codecs are validated
  // by `CanSupportMimeType()` first in code path.
  return IsAllowedMp4Type(type);
}

bool ShouldAddParameterSetsToBitstream(const String& codecs) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS) || \
    BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  String codecs_str = codecs.LowerASCII();
  return codecs_str.Find("hev1") != kNotFound ||
         codecs_str.Find("avc3") != kNotFound;
#else
  return false;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS) ||
        // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
}

}  // anonymous namespace

// Parses |codecs| and returns the first recognized VideoCodec.
// If profile/level can be parsed, they are included; otherwise, only the codec
// type is returned.
VideoTrackRecorder::CodecProfile VideoStringToCodecProfile(
    const String& codecs) {
  String codecs_str = codecs.LowerASCII();
  media::VideoCodec codec = media::VideoCodec::kUnknown;

  if (codecs_str.Find("vp8") != kNotFound) {
    codec = media::VideoCodec::kVP8;
  }
  if (codecs_str.Find("vp9") != kNotFound) {
    codec = media::VideoCodec::kVP9;
  }
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (codecs_str.Find("h264") != kNotFound ||
      codecs_str.Find("avc1") != kNotFound ||
      codecs_str.Find("avc3") != kNotFound) {
    codec = media::VideoCodec::kH264;
  }
  if (auto codec_profile =
          VideoStringTagToCodecProfile(codecs, {"avc1", "avc3"})) {
    return *codec_profile;
  }
#endif
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  if (codecs_str.Find("hvc1") != kNotFound ||
      codecs_str.Find("hev1") != kNotFound) {
    codec = media::VideoCodec::kHEVC;
  }
  if (auto codec_profile =
          VideoStringTagToCodecProfile(codecs, {"hvc1", "hev1"})) {
    return *codec_profile;
  }
#endif
  // TODO(crbug.com/40923648): Remove the wrong AV1 codecs string, "av1", once
  // we confirm nobody uses this in product.
  if (codecs_str.Find("av01") != kNotFound ||
      codecs_str.Find("av1") != kNotFound) {
    codec = media::VideoCodec::kAV1;
  }
  return VideoTrackRecorder::CodecProfile(codec);
}

MediaRecorderHandler::MediaRecorderHandler(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    KeyFrameRequestProcessor::Configuration key_frame_config)
    : key_frame_config_(key_frame_config),
      main_thread_task_runner_(std::move(main_thread_task_runner)),
      media_stream_observer_(std::make_unique<MediaStreamObserver>(this)) {}

bool MediaRecorderHandler::CanSupportMimeType(const String& type,
                                              const String& web_codecs,
                                              CanSupportMimeTypeCaller caller) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  // An empty |type| means MediaRecorderHandler can choose its preferred codecs.
  if (type.empty())
    return true;

  const bool video = CanSupportVideoType(type);
  const bool audio = !video && CanSupportAudioType(type);
  if (!video && !audio)
    return false;

  std::vector<std::string> codecs_list;
  media::SplitCodecs(web_codecs.Utf8(), &codecs_list);

  for (const auto& codec : codecs_list) {
    const auto profile = VideoStringToCodecProfile(String(codec));
    const bool can_support = CanSupportMimeTypeForCodec(type, codec);

    Vector<String> uma_key_vector;
    uma_key_vector.push_back("Media.MediaRecorder.Codec");
    uma_key_vector.push_back(StringFromCanSupportMimeTypeCaller(caller));
    uma_key_vector.push_back(can_support ? "Supported" : "Unsupported");
    StringBuilder builder;
    builder.AppendRange(uma_key_vector, ".");
    String uma_handle = builder.ReleaseString();
    base::UmaHistogramEnumeration(
        uma_handle.Ascii().c_str(),
        VideoTrackRecorder::CodecHistogramFromCodec(profile.codec));

    if (!can_support) {
      return false;
    }
  }
  return true;
}

const char* MediaRecorderHandler::StringFromCanSupportMimeTypeCaller(
    CanSupportMimeTypeCaller caller) {
  switch (caller) {
    case CanSupportMimeTypeCaller::kMediaRecorderCtor:
      return "MediaRecorderCtor";
    case CanSupportMimeTypeCaller::kIsTypeSupported:
      return "IsTypeSupported";
    case CanSupportMimeTypeCaller::kEncodingInfo:
      return "EncodingInfo";
    default:
      return "Test";
  }
}

bool MediaRecorderHandler::CanSupportMimeTypeForCodec(const String& type,
                                                      std::string_view codec) {
  const bool video = CanSupportVideoType(type);

  // Both |video| and |audio| support empty |codecs|; |type| == "video" supports
  // vp8, vp9, h264, avc1, avc3, av01, av1, hvc1, hev1, opus, or pcm; |type| =
  // "audio", supports opus or pcm (little-endian 32-bit float).
  // http://www.webmproject.org/docs/container Sec:"HTML5 Video Type Parameters"
  static const char* const kVideoCodecs[] = {
      "vp8", "vp9",
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      "h264", "avc1", "avc3",
#endif
      "av01",
      // TODO(crbug.com/40923648): Remove the wrong AV1 codecs string, "av1",
      // once we confirm nobody uses this in product.
      "av1",
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      "hvc1", "hev1",
#endif
      "opus", "pcm"};
  static const char* const kAudioCodecs[] = {"opus", "pcm"};

  base::span<const char* const> relevant_codecs;
  if (video) {
    relevant_codecs = kVideoCodecs;
  } else {
    relevant_codecs = kAudioCodecs;
  }

  const bool mp4_mime_type = IsAllowedMp4Type(type);
  if (mp4_mime_type) {
    static const char* const kVideoCodecsForMP4[] = {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
        "avc1", "avc3", "mp4a.40.2",
#endif
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        "hvc1", "hev1",
#endif
        "vp9",  "av01", "opus",
    };
    static const char* const kAudioCodecsForMp4[] = {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
        "mp4a.40.2",
#endif
        "opus"};

    if (video) {
      relevant_codecs = kVideoCodecsForMP4;
    } else {
      relevant_codecs = kAudioCodecsForMp4;
    }
  }

  // For `video/x-matroska`, `video/webm`, and `audio/webm`, trim the content
  // after first '.' to do the case insensitive match based on historical
  // logic. For `video/mp4`, and `audio/mp4`, preserve the whole string to do
  // the case sensitive match.
  String codec_string = String::FromUTF8(codec);
  if (!mp4_mime_type) {
    auto str_index = codec.find_first_of('.');
    if (str_index != std::string::npos) {
      codec_string = String::FromUTF8(codec.substr(0, str_index));
    }
  }

  bool match =
      std::any_of(relevant_codecs.begin(), relevant_codecs.end(),
                  [&codec_string, &mp4_mime_type](const char* name) {
                    if (mp4_mime_type) {
                      return EqualStringView(codec_string, name);
                    } else {
                      return EqualIgnoringASCIICase(codec_string, name);
                    }
                  });

  if (video) {
    std::string mime_type = type.Ascii();
    // It supports full qualified string for `avc1`, `avc3`, `hvc1`, `hev1`,
    // and `av01` codecs, e.g.
    //  `avc1.<profile>.<level>`,
    //  `avc3.<profile>.<level>`,
    //  `hvc1.<profile>.<profile_compatibility>.<tier and level>.*`,
    //  `hev1.<profile>.<profile_compatibility>.<tier and level>.*`,
    //  `av01.<profile>.<level>.<color depth>.*`.
    auto parsed_result =
        media::ParseVideoCodecString(mime_type, codec,
                                     /*allow_ambiguous_matches=*/false);
    if (!match && mp4_mime_type) {
      match =
          parsed_result && (parsed_result->codec == media::VideoCodec::kH264 ||
                            parsed_result->codec == media::VideoCodec::kAV1);
    }

    if (codec_string.StartsWith("h264", kTextCaseASCIIInsensitive) ||
        codec_string.StartsWith("avc1", kTextCaseASCIIInsensitive) ||
        codec_string.StartsWith("avc3", kTextCaseASCIIInsensitive)) {
      // In the case of the `video/mp4` mimetype, when the profile can be
      // parsed, make use of the parsed profile.
      const media::VideoCodecProfile profile =
          (mp4_mime_type && parsed_result)
              ? parsed_result->profile
              : media::VideoCodecProfile::H264PROFILE_BASELINE;

      // If the profile is not any of the H.264 baseline, main, extended, and
      // high profiles, reject it.
      if (profile != media::VideoCodecProfile::H264PROFILE_BASELINE &&
          profile != media::VideoCodecProfile::H264PROFILE_MAIN &&
          profile != media::VideoCodecProfile::H264PROFILE_EXTENDED &&
          profile != media::VideoCodecProfile::H264PROFILE_HIGH) {
        match = false;
      }

      // If the profile is not supported by either the HW or the SW encoder,
      // reject it.
      if (!media::IsEncoderSupportedVideoType(
              {media::VideoCodec::kH264, profile})) {
        match = false;
      }
    }

    if (codec_string.StartsWith("av1", kTextCaseASCIIInsensitive) ||
        codec_string.StartsWith("av01", kTextCaseASCIIInsensitive)) {
      // In the case of the `video/mp4` mimetype, when the profile can be
      // parsed, make use of the parsed profile.
      const media::VideoCodecProfile profile =
          (mp4_mime_type && parsed_result)
              ? parsed_result->profile
              : media::VideoCodecProfile::AV1PROFILE_PROFILE_MAIN;

      // If the profile does not match the AV1 main profile, reject it.
      if (profile != media::VideoCodecProfile::AV1PROFILE_PROFILE_MAIN) {
        match = false;
      }

      if (match) {
        base::UmaHistogramBoolean(
            "Media.MediaRecorder.HasCorrectAV1CodecString",
            codec_string.StartsWith("av01", kTextCaseASCIIInsensitive));
      }

      // If the profile is not supported by either the HW or the SW encoder,
      // reject it.
      if (!media::IsEncoderSupportedVideoType(
              {media::VideoCodec::kAV1, profile})) {
        match = false;
      }
    }

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    // Support `hev1` tag as it allow parameter sets write into the bitstream,
    // which is the only option if the MediaStream has dynamically changing
    // resolution. Also support `hvc1` tag for better compatibility given the
    // fact that QuickTime and Safari only support playing `hvc1` tag mp4
    // videos, and Apple only recommend using `hvc1` for HLS.
    // https://developer.apple.com/documentation/http-live-streaming/hls-authoring-specification-for-apple-devices#2969487
    if (codec_string.StartsWith("hvc1", kTextCaseASCIIInsensitive) ||
        codec_string.StartsWith("hev1", kTextCaseASCIIInsensitive)) {
      match =
          // If the profile can be parsed, ensure it must be HEVC main
          // profile.
          (parsed_result && parsed_result->profile ==
                                media::VideoCodecProfile::HEVCPROFILE_MAIN) &&
          // Only if the feature is enabled.
          base::FeatureList::IsEnabled(media::kMediaRecorderHEVCSupport) &&
          // Only `mkv` and `mp4` are supported, `webm` is not supported.
          !EqualIgnoringASCIICase(type, "video/webm") &&
          // Only if there are platform HEVC main profile support.
          media::IsEncoderSupportedVideoType(
              {media::VideoCodec::kHEVC,
               media::VideoCodecProfile::HEVCPROFILE_MAIN});
    }
#endif
  }

  if (!match) {
    return false;
  }

  if (codec_string == "mp4a.40.2" &&
      !media::MojoAudioEncoder::IsSupported(media::AudioCodec::kAAC)) {
    return false;
  }
  return true;
}

bool MediaRecorderHandler::Initialize(
    MediaRecorder* recorder,
    MediaStreamDescriptor* media_stream,
    const String& type,
    const String& codecs,
    AudioTrackRecorder::BitrateMode audio_bitrate_mode) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  // Save histogram data so we can see how much MediaStream Recorder is used.
  // The histogram counts the number of calls to the JS API.
  UpdateWebRTCMethodCount(RTCAPIName::kMediaStreamRecorder);

  type_ = type;

  if (!CanSupportMimeType(type_, codecs,
                          CanSupportMimeTypeCaller::kMediaRecorderCtor)) {
    DLOG(ERROR) << "Unsupported " << type.Utf8() << ";codecs=" << codecs.Utf8();
    return false;
  }

  passthrough_enabled_ = type_.empty();

  // Once established that we support the codec(s), hunt then individually.
  video_codec_profile_ = VideoStringToCodecProfile(codecs);
  if (video_codec_profile_.codec == media::VideoCodec::kUnknown) {
    MediaTrackContainerType container_type =
        GetMediaContainerTypeFromString(type_);
    video_codec_profile_.codec =
        VideoTrackRecorderImpl::GetPreferredCodec(container_type);
    DVLOG(1) << "Falling back to preferred video codec "
             << static_cast<int>(video_codec_profile_.codec);
  }

  add_parameter_sets_in_bitstream_ = ShouldAddParameterSetsToBitstream(codecs);

  // Do the same for the audio codec(s).
  media::AudioCodec audio_codec = AudioStringToAudioCodec(codecs);
  if (audio_codec == media::AudioCodec::kUnknown) {
    MediaTrackContainerType container_type =
        GetMediaContainerTypeFromString(type_);
    audio_codec_id_ = AudioTrackRecorder::GetPreferredCodec(container_type);
  } else {
    audio_codec_id_ = audio_codec;
  }

  DVLOG_IF(1, audio_codec == media::AudioCodec::kUnknown)
      << "Falling back to preferred audio codec "
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
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!recording_);
  DCHECK(media_stream_);
  DCHECK(timeslice_.is_zero());
  DCHECK(!muxer_adapter_);

  DCHECK(!is_media_stream_observer_);
  media_stream_->AddObserver(media_stream_observer_->AsWeakPtr());
  is_media_stream_observer_ = true;

  timeslice_ = timeslice == std::numeric_limits<int>::max()
                   ? base::TimeDelta::Max()
                   : base::Milliseconds(timeslice);
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
  }

  std::unique_ptr<media::Muxer> muxer;
  media::AudioCodec audio_codec = audio_codec_id_;
  std::optional<base::TimeDelta> optional_timeslice;
  if (timeslice > 0) {
    optional_timeslice = timeslice_;
  }

  auto write_callback =
      blink::BindRepeating(&MediaRecorderHandler::WriteData,
                           WrapPersistent(weak_factory_.GetWeakCell()));
  if (use_mp4_muxer) {
    muxer = std::make_unique<media::Mp4Muxer>(
        audio_codec, use_video_tracks, use_audio_tracks,
        std::make_unique<media::Mp4MuxerDelegate>(
            audio_codec, video_codec_profile_.codec,
            video_codec_profile_.profile, video_codec_profile_.level,
            add_parameter_sets_in_bitstream_, write_callback),
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
  } else if (timeslice_.is_max() &&
             base::FeatureList::IsEnabled(kMediaRecorderSeekableWebm)) {
    // Write a seekable WebM instead of a live one.
    auto delegate = std::make_unique<media::MemoryWebmMuxerDelegate>(
        write_callback, BindOnce(&MediaRecorderHandler::OnStarted,
                                 WrapPersistent(weak_factory_.GetWeakCell())));
    // Hold on to a raw_ptr for the delegate so we can fall back to live mode
    // if a requestData() call comes in.
    memory_muxer_delegate_ = delegate.get();
    muxer = std::make_unique<media::WebmMuxer>(
        audio_codec, use_video_tracks, use_audio_tracks, std::move(delegate),
        optional_timeslice);
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
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  // Don't check |recording_| since we can go directly from pause() to stop().

  // TODO(crbug.com/719023): The video recorder needs to be flushed to retrieve
  // the last N frames with some codecs.

  // Unregister from media stream notifications.
  if (media_stream_ && is_media_stream_observer_) {
    media_stream_->RemoveObserver(media_stream_observer_->AsWeakPtr());
  }
  is_media_stream_observer_ = false;

  // Ensure any stored data inside the muxer is flushed out before invalidation.
  memory_muxer_delegate_ = nullptr;
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
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
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
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
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

void MediaRecorderHandler::MaybeFlush() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  if (memory_muxer_delegate_) {
    memory_muxer_delegate_->FlushAndDisableSeeking();
  }
}

void MediaRecorderHandler::EncodingInfo(
    const WebMediaConfiguration& configuration,
    OnMediaCapabilitiesEncodingInfoCallback callback) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
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

  info->supported = CanSupportMimeType(mime_type, codec,
                                       CanSupportMimeTypeCaller::kEncodingInfo);

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
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
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
      mime_type.Append(type_.Span8());
    }
    mime_type.Append(";codecs=");
  } else {
    switch (video_codec_profile_.codec) {
      case media::VideoCodec::kVP8:
      case media::VideoCodec::kVP9:
      case media::VideoCodec::kAV1:
        if (passthrough_enabled_) {
          mime_type.Append("video/webm");
        } else {
          mime_type.Append(type_.Span8());
        }
        mime_type.Append(";codecs=");
        break;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      case media::VideoCodec::kH264:
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      case media::VideoCodec::kHEVC:
#endif
        if (!passthrough_enabled_ &&
            (EqualIgnoringASCIICase(type_, "video/mp4") ||
             EqualIgnoringASCIICase(type_, "video/matroska") ||
             EqualIgnoringASCIICase(type_, "video/x-matroska"))) {
          mime_type.Append(type_.Span8());
        } else {
          mime_type.Append("video/x-matroska");
        }
        mime_type.Append(";codecs=");
        break;
#endif
      default:
        // Do nothing.
        break;
    }
  }
  if (has_video_tracks) {
    switch (video_codec_profile_.codec) {
      case media::VideoCodec::kVP8:
        mime_type.Append("vp8");
        break;
      case media::VideoCodec::kVP9:
        mime_type.Append("vp9");
        break;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      case media::VideoCodec::kH264:
        mime_type.Append(add_parameter_sets_in_bitstream_ ? "avc3" : "avc1");
        if (video_codec_profile_.profile && video_codec_profile_.level) {
          mime_type.Append(
              media::BuildH264MimeSuffix(*video_codec_profile_.profile,
                                         *video_codec_profile_.level)
                  .c_str());
        }
        break;
#endif
      case media::VideoCodec::kAV1:
        mime_type.Append("av01");
        break;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
      case media::VideoCodec::kHEVC:
        mime_type.Append(add_parameter_sets_in_bitstream_ ? "hev1" : "hvc1");
        break;
#endif
      default:
        break;
    }
  }
  if (has_video_tracks && has_audio_tracks) {
    if (video_codec_profile_.codec != media::VideoCodec::kUnknown &&
        audio_codec_id_ != media::AudioCodec::kUnknown) {
      mime_type.Append(",");
    }
  }
  if (has_audio_tracks) {
    switch (audio_codec_id_) {
      case media::AudioCodec::kOpus:
        mime_type.Append("opus");
        break;
      case media::AudioCodec::kPCM:
        mime_type.Append("pcm");
        break;
      case media::AudioCodec::kAAC:
        mime_type.Append("mp4a.40.2");
        break;
      default:
        break;
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
        FROM_HERE, BindOnce(&MediaRecorder::OnStreamChanged,
                            WrapWeakPersistent(recorder_.Get()), message));
  }
}

void MediaRecorderHandler::OnEncodedVideo(
    const media::Muxer::VideoParameters& params,
    scoped_refptr<media::DecoderBuffer> encoded_data,
    std::optional<media::VideoEncoder::CodecDescription> codec_description,
    base::TimeTicks timestamp) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());

  if (!encoded_data || encoded_data->empty()) {
    // An encoder drops a frame. This can happen with VideoToolBox encoder as
    // there is no way to disallow the frame dropping with it.
    return;
  }

#if BUILDFLAG(USE_PROPRIETARY_CODECS) || \
    BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  // TODO(crbug.com/40266540): Once Encoder supports VideoEncoder, then the
  // below code could go away.
  media::VideoCodec video_codec = video_codec_profile_.codec;
  // Convert annex stream to avc/hevc bit stream for h264/h265.
  if ((video_codec == media::VideoCodec::kH264 ||
       video_codec == media::VideoCodec::kHEVC) &&
      encoded_data->is_key_frame() && !codec_description.has_value()) {
    bool first_key_frame = false;
    if (!h26x_converter_) {
      h26x_converter_ = std::make_unique<media::H26xAnnexBToBitstreamConverter>(
          video_codec, add_parameter_sets_in_bitstream_);
      first_key_frame = true;
    }

    // We don't use the output_chunk, we just pass the configuration
    // data as a codec_descriptions.
    auto output_chunk = h26x_converter_->Convert(*encoded_data);
    codec_description = h26x_converter_->GetCodecDescription();
    if (first_key_frame) {
      video_codec_profile_.level =
          h26x_converter_->GetCodecProfileLevel().level;
    }

    // For `avc1` or `hvc1` mp4 recording, since the codec description is only
    // written to the sample entries, and not allowed to write those to the
    // bitstream, we print a error message telling the user switch to `avc3` or
    // `hev1` instead.
    if (!add_parameter_sets_in_bitstream_ &&
        !has_codec_description_changed_error_printed_ &&
        EqualIgnoringASCIICase(type_, "video/mp4") &&
        last_seen_codec_description_.size() &&
        last_seen_codec_description_ != codec_description.value() &&
        recorder_) {
      const String& message = String::Format(
          "When using \"%s\" for mp4 encoding, the codec description is not "
          "supposed to change during the entire recording. Normally, a change "
          "in the encoding resolution may lead to this situation. "
          "Consider switching to \"%s\" instead to resolve this problem",
          video_codec == media::VideoCodec::kH264 ? "avc1" : "hvc1",
          video_codec == media::VideoCodec::kH264 ? "avc3" : "hev1");
      auto* context = recorder_->GetExecutionContext();
      if (context && !context->IsContextDestroyed()) {
        context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kError, message));
      }
      has_codec_description_changed_error_printed_ = true;
    }
    last_seen_codec_description_ = codec_description.value();
  }
#endif

  auto params_with_codec = params;
  params_with_codec.codec = video_codec_profile_.codec;
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
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());

  // Update |video_codec_profile_| so that ActualMimeType() works.
  video_codec_profile_.codec = params.codec;
  HandleEncodedVideo(params, std::move(encoded_data), std::nullopt, timestamp);
}

void MediaRecorderHandler::HandleEncodedVideo(
    const media::Muxer::VideoParameters& params,
    scoped_refptr<media::DecoderBuffer> encoded_data,
    std::optional<media::VideoEncoder::CodecDescription> codec_description,
    base::TimeTicks timestamp) {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());

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
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());

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
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
  recorder_->OnError(DOMExceptionCode::kEncodingError,
                     String(media::EncoderStatusCodeToString(error_status)));
}

std::unique_ptr<media::VideoEncoderMetricsProvider>
MediaRecorderHandler::CreateVideoEncoderMetricsProvider() {
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
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
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());
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
  DCHECK(main_thread_task_runner_->RunsTasksInCurrentSequence());

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
  for (const auto& track : video_tracks_) {
    if (track->GetReadyState() == MediaStreamSource::kReadyStateEnded) {
      muxer_adapter_->SetLiveAndEnabled(false, /*is_video=*/true);
      muxer_adapter_->OnVideoEnded();
      DVLOG(2) << __func__ << " ended video";
    }
  }

  for (const auto& track : audio_tracks_) {
    if (track->GetReadyState() == MediaStreamSource::kReadyStateEnded) {
      muxer_adapter_->SetLiveAndEnabled(false, /*is_video=*/false);
      DVLOG(2) << __func__ << " ended audio";
    }
  }

  if (MediaStream* stream = ToMediaStream(media_stream_)) {
    for (const auto& track : stream->getTracks()) {
      if (track->readyState() != V8MediaStreamTrackState::Enum::kEnded) {
        return;
      }
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

void MediaRecorderHandler::OnVideoEncodingError(
    media::EncoderStatus error_status) {
  if (recorder_) {
    // The MediaRecorder::OnError callback stops the MediaRecorderHandler,
    // which in turn invalidates a reference to the calling VideoTrackRecorder
    // instance. To avoid access to an unallocated object, this operation is
    // deferred to a subsequent task.
    // See https://crbug.com/441921804 for more details.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        blink::BindOnce(
            &MediaRecorder::OnError, WrapWeakPersistent(recorder_.Get()),
            DOMExceptionCode::kEncodingError,
            String(media::EncoderStatusCodeToString(error_status))));
  }
}

void MediaRecorderHandler::OnStarted() {
  if (recorder_) {
    recorder_->MaybeEmitStartEvent();
  }
}

}  // namespace blink
