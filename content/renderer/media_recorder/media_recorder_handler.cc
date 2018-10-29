// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media_recorder/media_recorder_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "content/renderer/media/stream/media_stream_audio_track.h"
#include "content/renderer/media/stream/media_stream_track.h"
#include "content/renderer/media/webrtc/webrtc_uma_histograms.h"
#include "content/renderer/media_recorder/audio_track_recorder.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_parameters.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/mime_util.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/muxers/webm_muxer.h"
#include "third_party/blink/public/platform/modules/media_capabilities/web_media_configuration.h"
#include "third_party/blink/public/platform/scoped_web_callbacks.h"
#include "third_party/blink/public/platform/web_media_recorder_handler_client.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_string.h"

using base::TimeDelta;
using base::TimeTicks;
using base::ToLowerASCII;

namespace content {

using blink::WebMediaCapabilitiesQueryCallbacks;

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

media::VideoCodec CodecIdToMediaVideoCodec(VideoTrackRecorder::CodecId id) {
  switch (id) {
    case VideoTrackRecorder::CodecId::VP8:
      return media::kCodecVP8;
    case VideoTrackRecorder::CodecId::VP9:
      return media::kCodecVP9;
#if BUILDFLAG(RTC_USE_H264)
    case VideoTrackRecorder::CodecId::H264:
      return media::kCodecH264;
#endif
    case VideoTrackRecorder::CodecId::LAST:
      return media::kUnknownVideoCodec;
  }
  NOTREACHED() << "Unsupported video codec";
  return media::kUnknownVideoCodec;
}

media::AudioCodec CodecIdToMediaAudioCodec(AudioTrackRecorder::CodecId id) {
  switch (id) {
    case AudioTrackRecorder::CodecId::PCM:
      return media::kCodecPCM;
    case AudioTrackRecorder::CodecId::OPUS:
      return media::kCodecOpus;
    case AudioTrackRecorder::CodecId::LAST:
      return media::kUnknownAudioCodec;
  }
  NOTREACHED() << "Unsupported audio codec";
  return media::kUnknownAudioCodec;
}

// Extracts the first recognised CodecId of |codecs| or CodecId::LAST if none
// of them is known.
VideoTrackRecorder::CodecId VideoStringToCodecId(
    const blink::WebString& codecs) {
  const std::string& codecs_str = ToLowerASCII(codecs.Utf8());

  if (codecs_str.find("vp8") != std::string::npos)
    return VideoTrackRecorder::CodecId::VP8;
  if (codecs_str.find("vp9") != std::string::npos)
    return VideoTrackRecorder::CodecId::VP9;
#if BUILDFLAG(RTC_USE_H264)
  if (codecs_str.find("h264") != std::string::npos ||
      codecs_str.find("avc1") != std::string::npos)
    return VideoTrackRecorder::CodecId::H264;
#endif
  return VideoTrackRecorder::CodecId::LAST;
}

AudioTrackRecorder::CodecId AudioStringToCodecId(
    const blink::WebString& codecs) {
  const std::string& codecs_str = ToLowerASCII(codecs.Utf8());

  if (codecs_str.find("opus") != std::string::npos)
    return AudioTrackRecorder::CodecId::OPUS;
  if (codecs_str.find("pcm") != std::string::npos)
    return AudioTrackRecorder::CodecId::PCM;

  return AudioTrackRecorder::CodecId::LAST;
}

void OnEncodingInfoError(
    std::unique_ptr<WebMediaCapabilitiesQueryCallbacks> callbacks) {
  callbacks->OnError();
}

}  // anonymous namespace

MediaRecorderHandler::MediaRecorderHandler(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : video_bits_per_second_(0),
      audio_bits_per_second_(0),
      video_codec_id_(VideoTrackRecorder::CodecId::LAST),
      audio_codec_id_(AudioTrackRecorder::CodecId::LAST),
      recording_(false),
      client_(nullptr),
      task_runner_(std::move(task_runner)),
      weak_factory_(this) {}

MediaRecorderHandler::~MediaRecorderHandler() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  // Send a |last_in_slice| to our |client_|.
  if (client_)
    client_->WriteData(
        nullptr, 0u, true,
        (TimeTicks::Now() - TimeTicks::UnixEpoch()).InMillisecondsF());
}

bool MediaRecorderHandler::CanSupportMimeType(
    const blink::WebString& web_type,
    const blink::WebString& web_codecs) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  // An empty |web_type| means MediaRecorderHandler can choose its preferred
  // codecs.
  if (web_type.IsEmpty())
    return true;

  const std::string type(web_type.Utf8());
  const bool video = base::EqualsCaseInsensitiveASCII(type, "video/webm") ||
                     base::EqualsCaseInsensitiveASCII(type, "video/x-matroska");
  const bool audio =
      video ? false : (base::EqualsCaseInsensitiveASCII(type, "audio/webm"));
  if (!video && !audio)
    return false;

  // Both |video| and |audio| support empty |codecs|; |type| == "video" supports
  // vp8, vp9, h264 and avc1 or opus; |type| = "audio", supports opus or pcm
  // (little-endian 32-bit float).
  // http://www.webmproject.org/docs/container Sec:"HTML5 Video Type Parameters"
  static const char* const kVideoCodecs[] = {"vp8",  "vp9",  "h264",
                                             "avc1", "opus", "pcm"};
  static const char* const kAudioCodecs[] = {"opus", "pcm"};
  const char* const* codecs = video ? &kVideoCodecs[0] : &kAudioCodecs[0];
  const int codecs_count =
      video ? arraysize(kVideoCodecs) : arraysize(kAudioCodecs);

  std::vector<std::string> codecs_list;
  media::SplitCodecs(web_codecs.Utf8(), &codecs_list);
  media::StripCodecs(&codecs_list);
  for (const auto& codec : codecs_list) {
    auto* const* found = std::find_if(
        &codecs[0], &codecs[codecs_count], [&codec](const char* name) {
          return base::EqualsCaseInsensitiveASCII(codec, name);
        });
    if (found == &codecs[codecs_count])
      return false;
  }
  return true;
}

bool MediaRecorderHandler::Initialize(
    blink::WebMediaRecorderHandlerClient* client,
    const blink::WebMediaStream& media_stream,
    const blink::WebString& type,
    const blink::WebString& codecs,
    int32_t audio_bits_per_second,
    int32_t video_bits_per_second) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  // Save histogram data so we can see how much MediaStream Recorder is used.
  // The histogram counts the number of calls to the JS API.
  UpdateWebRTCMethodCount(blink::WebRTCAPIName::kMediaStreamRecorder);

  if (!CanSupportMimeType(type, codecs)) {
    DLOG(ERROR) << "Unsupported " << type.Utf8() << ";codecs=" << codecs.Utf8();
    return false;
  }

  // Once established that we support the codec(s), hunt then individually.
  const VideoTrackRecorder::CodecId video_codec_id =
      VideoStringToCodecId(codecs);
  video_codec_id_ = (video_codec_id != VideoTrackRecorder::CodecId::LAST)
                        ? video_codec_id
                        : VideoTrackRecorder::GetPreferredCodecId();
  DVLOG_IF(1, video_codec_id == VideoTrackRecorder::CodecId::LAST)
      << "Falling back to preferred video codec id "
      << static_cast<int>(video_codec_id_);

  // Do the same for the audio codec(s).
  const AudioTrackRecorder::CodecId audio_codec_id =
      AudioStringToCodecId(codecs);
  audio_codec_id_ = (audio_codec_id != AudioTrackRecorder::CodecId::LAST)
                        ? audio_codec_id
                        : AudioTrackRecorder::GetPreferredCodecId();
  DVLOG_IF(1, audio_codec_id == AudioTrackRecorder::CodecId::LAST)
      << "Falling back to preferred audio codec id "
      << static_cast<int>(audio_codec_id_);

  media_stream_ = media_stream;
  DCHECK(client);
  client_ = client;

  audio_bits_per_second_ = audio_bits_per_second;
  video_bits_per_second_ = video_bits_per_second;
  return true;
}

bool MediaRecorderHandler::Start(int timeslice) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(!recording_);
  DCHECK(!media_stream_.IsNull());
  DCHECK(timeslice_.is_zero());
  DCHECK(!webm_muxer_);

  timeslice_ = TimeDelta::FromMilliseconds(timeslice);
  slice_origin_timestamp_ = TimeTicks::Now();

  video_tracks_ = media_stream_.VideoTracks();
  audio_tracks_ = media_stream_.AudioTracks();

  if (video_tracks_.IsEmpty() && audio_tracks_.IsEmpty()) {
    LOG(WARNING) << __func__ << ": no media tracks.";
    return false;
  }

  const bool use_video_tracks =
      !video_tracks_.IsEmpty() && video_tracks_[0].IsEnabled() &&
      video_tracks_[0].Source().GetReadyState() !=
          blink::WebMediaStreamSource::kReadyStateEnded;
  const bool use_audio_tracks =
      !audio_tracks_.IsEmpty() &&
      MediaStreamAudioTrack::From(audio_tracks_[0]) &&
      audio_tracks_[0].IsEnabled() &&
      audio_tracks_[0].Source().GetReadyState() !=
          blink::WebMediaStreamSource::kReadyStateEnded;

  if (!use_video_tracks && !use_audio_tracks) {
    LOG(WARNING) << __func__ << ": no tracks to be recorded.";
    return false;
  }

  webm_muxer_.reset(
      new media::WebmMuxer(CodecIdToMediaVideoCodec(video_codec_id_),
                           CodecIdToMediaAudioCodec(audio_codec_id_),
                           use_video_tracks, use_audio_tracks,
                           base::Bind(&MediaRecorderHandler::WriteData,
                                      weak_factory_.GetWeakPtr())));

  if (use_video_tracks) {
    // TODO(mcasas): The muxer API supports only one video track. Extend it to
    // several video tracks, see http://crbug.com/528523.
    LOG_IF(WARNING, video_tracks_.size() > 1u)
        << "Recording multiple video tracks is not implemented. "
        << "Only recording first video track.";
    const blink::WebMediaStreamTrack& video_track = video_tracks_[0];
    if (video_track.IsNull())
      return false;

    const VideoTrackRecorder::OnEncodedVideoCB on_encoded_video_cb =
        media::BindToCurrentLoop(base::Bind(
            &MediaRecorderHandler::OnEncodedVideo, weak_factory_.GetWeakPtr()));

    video_recorders_.emplace_back(new VideoTrackRecorder(
        video_codec_id_, video_track, on_encoded_video_cb,
        video_bits_per_second_, task_runner_));
  }

  if (use_audio_tracks) {
    // TODO(ajose): The muxer API supports only one audio track. Extend it to
    // several tracks.
    LOG_IF(WARNING, audio_tracks_.size() > 1u)
        << "Recording multiple audio"
        << " tracks is not implemented.  Only recording first audio track.";
    const blink::WebMediaStreamTrack& audio_track = audio_tracks_[0];
    if (audio_track.IsNull())
      return false;

    const AudioTrackRecorder::OnEncodedAudioCB on_encoded_audio_cb =
        media::BindToCurrentLoop(base::Bind(
            &MediaRecorderHandler::OnEncodedAudio, weak_factory_.GetWeakPtr()));

    audio_recorders_.emplace_back(new AudioTrackRecorder(
        audio_codec_id_, audio_track, std::move(on_encoded_audio_cb),
        audio_bits_per_second_));
  }

  recording_ = true;
  return true;
}

void MediaRecorderHandler::Stop() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  // Don't check |recording_| since we can go directly from pause() to stop().

  weak_factory_.InvalidateWeakPtrs();
  recording_ = false;
  timeslice_ = TimeDelta::FromMilliseconds(0);
  video_recorders_.clear();
  audio_recorders_.clear();
  webm_muxer_.reset();
}

void MediaRecorderHandler::Pause() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(recording_);
  recording_ = false;
  for (const auto& video_recorder : video_recorders_)
    video_recorder->Pause();
  for (const auto& audio_recorder : audio_recorders_)
    audio_recorder->Pause();
  webm_muxer_->Pause();
}

void MediaRecorderHandler::Resume() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(!recording_);
  recording_ = true;
  for (const auto& video_recorder : video_recorders_)
    video_recorder->Resume();
  for (const auto& audio_recorder : audio_recorders_)
    audio_recorder->Resume();
  webm_muxer_->Resume();
}

void MediaRecorderHandler::EncodingInfo(
    const blink::WebMediaConfiguration& configuration,
    std::unique_ptr<blink::WebMediaCapabilitiesQueryCallbacks> callbacks) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(configuration.video_configuration ||
         configuration.audio_configuration);

  auto scoped_callbacks = blink::MakeScopedWebCallbacks(
      std::move(callbacks), base::BindOnce(&OnEncodingInfoError));

  std::unique_ptr<blink::WebMediaCapabilitiesInfo> info(
      new blink::WebMediaCapabilitiesInfo());

  // TODO(mcasas): Support the case when both video and audio configurations are
  // specified: https://crbug.com/709181.
  blink::WebString mime_type;
  blink::WebString codec;
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
        VideoTrackRecorder::CanUseAcceleratedEncoder(
            VideoStringToCodecId(codec),
            configuration.video_configuration->width,
            configuration.video_configuration->height);

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

  scoped_callbacks.PassCallbacks()->OnSuccess(std::move(info));
}

blink::WebString MediaRecorderHandler::ActualMimeType() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(client_) << __func__ << " should be called after Initialize()";

  const bool has_video_tracks = !media_stream_.VideoTracks().empty();
  const bool has_audio_tracks = !media_stream_.AudioTracks().empty();
  if (!has_video_tracks && !has_audio_tracks)
    return blink::WebString();

  std::string mime_type;
  if (!has_video_tracks && has_audio_tracks) {
    mime_type.append("audio/webm;codecs=");
  } else {
    switch (video_codec_id_) {
      case VideoTrackRecorder::CodecId::VP8:
      case VideoTrackRecorder::CodecId::VP9:
        mime_type.append("video/webm;codecs=");
        break;
#if BUILDFLAG(RTC_USE_H264)
      case VideoTrackRecorder::CodecId::H264:
        mime_type.append("video/x-matroska;codecs=");
        break;
#endif
      case VideoTrackRecorder::CodecId::LAST:
        // Do nothing.
        break;
    }
  }
  if (has_video_tracks) {
    switch (video_codec_id_) {
      case VideoTrackRecorder::CodecId::VP8:
        mime_type.append("vp8");
        break;
      case VideoTrackRecorder::CodecId::VP9:
        mime_type.append("vp9");
        break;
#if BUILDFLAG(RTC_USE_H264)
      case VideoTrackRecorder::CodecId::H264:
        mime_type.append("avc1");
        break;
#endif
      case VideoTrackRecorder::CodecId::LAST:
        DCHECK_NE(audio_codec_id_, AudioTrackRecorder::CodecId::LAST);
    }
  }
  if (has_video_tracks && has_audio_tracks) {
    if (video_codec_id_ != VideoTrackRecorder::CodecId::LAST &&
        audio_codec_id_ != AudioTrackRecorder::CodecId::LAST) {
      mime_type.append(",");
    }
  }
  if (has_audio_tracks) {
    switch (audio_codec_id_) {
      case AudioTrackRecorder::CodecId::OPUS:
        mime_type.append("opus");
        break;
      case AudioTrackRecorder::CodecId::PCM:
        mime_type.append("pcm");
        break;
      case AudioTrackRecorder::CodecId::LAST:
        DCHECK_NE(video_codec_id_, VideoTrackRecorder::CodecId::LAST);
    }
  }
  return blink::WebString::FromUTF8(mime_type);
}

void MediaRecorderHandler::OnEncodedVideo(
    const media::WebmMuxer::VideoParameters& params,
    std::unique_ptr<std::string> encoded_data,
    std::unique_ptr<std::string> encoded_alpha,
    TimeTicks timestamp,
    bool is_key_frame) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());

  if (UpdateTracksAndCheckIfChanged()) {
    client_->OnError("Amount of tracks in MediaStream has changed.");
    return;
  }
  if (!webm_muxer_)
    return;
  if (!webm_muxer_->OnEncodedVideo(params, std::move(encoded_data),
                                   std::move(encoded_alpha), timestamp,
                                   is_key_frame)) {
    DLOG(ERROR) << "Error muxing video data";
    client_->OnError("Error muxing video data");
  }
}

void MediaRecorderHandler::OnEncodedAudio(
    const media::AudioParameters& params,
    std::unique_ptr<std::string> encoded_data,
    base::TimeTicks timestamp) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());

  if (UpdateTracksAndCheckIfChanged()) {
    client_->OnError("Amount of tracks in MediaStream has changed.");
    return;
  }
  if (!webm_muxer_)
    return;
  if (!webm_muxer_->OnEncodedAudio(params, std::move(encoded_data),
                                   timestamp)) {
    DLOG(ERROR) << "Error muxing audio data";
    client_->OnError("Error muxing audio data");
  }
}

void MediaRecorderHandler::WriteData(base::StringPiece data) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  const TimeTicks now = TimeTicks::Now();
  // Non-buffered mode does not need to check timestamps.
  if (timeslice_.is_zero()) {
    client_->WriteData(data.data(), data.length(), true /* lastInSlice */,
                       (now - TimeTicks::UnixEpoch()).InMillisecondsF());
    return;
  }

  const bool last_in_slice = now > slice_origin_timestamp_ + timeslice_;
  DVLOG_IF(1, last_in_slice) << "Slice finished @ " << now;
  if (last_in_slice)
    slice_origin_timestamp_ = now;
  client_->WriteData(data.data(), data.length(), last_in_slice,
                     (now - TimeTicks::UnixEpoch()).InMillisecondsF());
}

bool MediaRecorderHandler::UpdateTracksAndCheckIfChanged() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks, audio_tracks;
  video_tracks = media_stream_.VideoTracks();
  audio_tracks = media_stream_.AudioTracks();

  bool video_tracks_changed = video_tracks_.size() != video_tracks.size();
  bool audio_tracks_changed = audio_tracks_.size() != audio_tracks.size();

  if (!video_tracks_changed) {
    for (size_t i = 0; i < video_tracks.size(); ++i) {
      if (video_tracks_[i].Id() != video_tracks[i].Id()) {
        video_tracks_changed = true;
        break;
      }
    }
  }
  if (!video_tracks_changed && !audio_tracks_changed) {
    for (size_t i = 0; i < audio_tracks.size(); ++i) {
      if (audio_tracks_[i].Id() != audio_tracks[i].Id()) {
        audio_tracks_changed = true;
        break;
      }
    }
  }

  if (video_tracks_changed)
    video_tracks_ = video_tracks;
  if (audio_tracks_changed)
    audio_tracks_ = audio_tracks;

  return video_tracks_changed || audio_tracks_changed;
}

void MediaRecorderHandler::OnVideoFrameForTesting(
    const scoped_refptr<media::VideoFrame>& frame,
    const TimeTicks& timestamp) {
  for (const auto& recorder : video_recorders_)
    recorder->OnVideoFrameForTesting(frame, timestamp);
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

}  // namespace content
