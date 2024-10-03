// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/hls_manifest_demuxer_engine.h"

#include <optional>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_codecs.h"
#include "media/base/media_log.h"
#include "media/base/media_track.h"
#include "media/base/media_util.h"
#include "media/base/pipeline_status.h"
#include "media/base/supported_types.h"
#include "media/base/video_codec_string_parsers.h"
#include "media/base/video_codecs.h"
#include "media/filters/hls_network_access_impl.h"
#include "media/filters/manifest_demuxer.h"
#include "media/formats/hls/audio_rendition.h"
#include "media/formats/hls/media_playlist.h"
#include "media/formats/hls/multivariant_playlist.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variant_stream.h"
#include "media/formats/mp4/box_reader.h"

namespace media {

namespace {

constexpr const char* kPrimary = "primary";
constexpr const char* kAudioOverride = "audio-override";

bool ParseAudioCodec(const std::string& codec, AudioType* audio_type) {
  audio_type->codec = StringToAudioCodec(codec);
  audio_type->profile = AudioCodecProfile::kUnknown;
  audio_type->spatial_rendering = false;
  return audio_type->codec != AudioCodec::kUnknown;
}

// These functions are intended to test that there are {audio/video} codecs
// present in the types, and that each codec present is supported. An empty
// list of audioo codecs should not be considered "supported audio" for example.
bool AreAllAudioCodecsSupported(const std::vector<AudioType>& audio_types) {
  if (audio_types.empty()) {
    return false;
  }
  for (const auto& type : audio_types) {
    if (!IsSupportedAudioType(type)) {
      return false;
    }
  }
  return true;
}

bool AreAllVideoCodecsSupported(const std::vector<VideoType>& video_types) {
  if (video_types.empty()) {
    return false;
  }
  for (const auto& type : video_types) {
    if (!IsSupportedVideoType(type)) {
      return false;
    }
  }
  return true;
}

hls::RenditionManager::CodecSupportType GetSupportedTypes(
    std::string_view container,
    base::span<const std::string> codecs) {
  std::vector<VideoType> video_formats;
  std::vector<AudioType> audio_formats;
  for (const std::string& codec : codecs) {
    // Try parsing it as a video codec first, which will set `video.codec`
    // to unknown if it fails.
    if (auto result = ParseCodec(codec)) {
      video_formats.push_back({result->codec, result->profile, result->level,
                               result->color_space,
                               gfx::HdrMetadataType::kNone});

      continue;
    }

    AudioType audio;
    if (ParseAudioCodec(codec, &audio)) {
      audio_formats.push_back(audio);
    }
  }

  const bool audio_support = AreAllAudioCodecsSupported(audio_formats);
  const bool video_support = AreAllVideoCodecsSupported(video_formats);

  if (audio_support && video_support) {
    return hls::RenditionManager::CodecSupportType::kSupportedAudioVideo;
  }
  if (audio_support && video_formats.empty()) {
    return hls::RenditionManager::CodecSupportType::kSupportedAudioOnly;
  }
  if (video_support && audio_formats.empty()) {
    return hls::RenditionManager::CodecSupportType::kSupportedVideoOnly;
  }
  return hls::RenditionManager::CodecSupportType::kUnsupported;
}

HlsDemuxerStatus::Or<RelaxedParserSupportedType> CheckMP4Bytes(
    const uint8_t* data,
    size_t size) {
  NullMediaLog null;
  std::unique_ptr<mp4::BoxReader> reader;
  auto result = mp4::BoxReader::ReadTopLevelBox(data, size, &null, &reader);
  if (result == mp4::ParseResult::kOk) {
    return RelaxedParserSupportedType::kMP4;
  }
  return HlsDemuxerStatus::Codes::kUnsupportedContainer;
}

HlsDemuxerStatus::Or<RelaxedParserSupportedType>
CheckBitstreamForContainerMagic(const uint8_t* data, size_t size) {
  CHECK_GT(size, 0lu);

  constexpr uint8_t kMP4FirstByte = 0x66;
  constexpr uint8_t kMPEGTSFirstByte = 0x47;
  constexpr uint8_t kFMP4FirstByte = 0x00;
  constexpr uint8_t kAACFirstByte = 0xFF;
  constexpr uint8_t kID3FirstByte = 0x49;

  switch (data[0]) {
    case kMP4FirstByte:
    case kFMP4FirstByte: {
      return CheckMP4Bytes(data, size);
    }
    case kID3FirstByte:
    case kAACFirstByte: {
      // TODO(issue/40253609): Check further bytes in the header.
      return RelaxedParserSupportedType::kAAC;
    }
    case kMPEGTSFirstByte: {
      return RelaxedParserSupportedType::kMP2T;
    }
    default: {
      return HlsDemuxerStatus::Codes::kUnsupportedContainer;
    }
  }
}

}  // namespace

HlsManifestDemuxerEngine::~HlsManifestDemuxerEngine() = default;

HlsManifestDemuxerEngine::HlsManifestDemuxerEngine(
    base::SequenceBound<HlsDataSourceProvider> dsp,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    base::RepeatingCallback<void(const MediaTrack&)> add_track,
    base::RepeatingCallback<void(const MediaTrack&)> remove_track,
    bool was_already_tainted,
    GURL root_playlist_uri,
    MediaLog* media_log)
    : media_task_runner_(std::move(media_task_runner)),
      add_track_(std::move(add_track)),
      remove_track_(std::move(remove_track)),
      root_playlist_uri_(std::move(root_playlist_uri)),
      media_log_(media_log->Clone()),
      network_access_(std::make_unique<HlsNetworkAccessImpl>(std::move(dsp))),
      origin_tainted_(was_already_tainted) {
  // This is always created on the main sequence, but used on the media sequence
  DETACH_FROM_SEQUENCE(media_sequence_checker_);
}

void HlsManifestDemuxerEngine::ProcessActionQueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);

  if (action_in_progress_ || pending_action_queue_.empty()) {
    return;
  }

  action_in_progress_ = true;
  auto action = std::move(pending_action_queue_.front());
  pending_action_queue_.pop();
  std::move(action).Run(base::BindOnce(
      &HlsManifestDemuxerEngine::OnActionComplete, weak_factory_.GetWeakPtr()));
}

void HlsManifestDemuxerEngine::OnActionComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  CHECK(action_in_progress_);
  action_in_progress_ = false;
  ProcessActionQueue();
}

HlsManifestDemuxerEngine::PlaylistParseInfo::PlaylistParseInfo(
    GURL uri,
    std::vector<std::string> codecs,
    std::string role,
    bool allow_multivariant_playlist)
    : uri(std::move(uri)),
      codecs(std::move(codecs)),
      role(std::move(role)),
      allow_multivariant_playlist(allow_multivariant_playlist) {}

HlsManifestDemuxerEngine::PlaylistParseInfo::~PlaylistParseInfo() {}

HlsManifestDemuxerEngine::PlaylistParseInfo::PlaylistParseInfo(
    const PlaylistParseInfo& copy) = default;

int64_t HlsManifestDemuxerEngine::GetMemoryUsage() {
  return total_stream_memory_;
}

bool HlsManifestDemuxerEngine::WouldTaintOrigin() {
  return origin_tainted_;
}

bool HlsManifestDemuxerEngine::IsStreaming() {
  return !is_seekable_;
}

std::string HlsManifestDemuxerEngine::GetName() const {
  return "HlsManifestDemuxer";
}

void HlsManifestDemuxerEngine::StartWaitingForSeek() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  for (auto& [_, rendition] : renditions_) {
    rendition->StartWaitingForSeek();
  }
}

void HlsManifestDemuxerEngine::AbortPendingReads(base::OnceClosure cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
}

bool HlsManifestDemuxerEngine::IsSeekable() const {
  // `IsSeekable()` is only called after the pipeline has completed successfully
  // or the initialization step fails. If init fails, we should report that the
  // player is seekable to keep consistent behavior with other players.
  return is_seekable_.value_or(true);
}

int64_t HlsManifestDemuxerEngine::GetMemoryUsage() const {
  // TODO(crbug.com/40057824): Sum the memory of the renditions and data source
  // providers.
  return 0;
}

void HlsManifestDemuxerEngine::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  pending_action_queue_ = {};

  for (auto& [_, rendition] : renditions_) {
    rendition->Stop();
  }

  network_access_.reset();
  weak_factory_.InvalidateWeakPtrs();

  multivariant_root_.reset();
  rendition_manager_.reset();
  renditions_.clear();
  host_ = nullptr;
}

void HlsManifestDemuxerEngine::Seek(base::TimeDelta time,
                                    ManifestDemuxer::SeekCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!network_access_) {
    // The pipeline can call Seek just after an error was surfaced. The error
    // handler resets |network_access_|, so we should just reply with
    // another error here.
    std::move(cb).Run(PIPELINE_ERROR_ABORT);
    return;
  }

  ProcessAsyncAction<ManifestDemuxer::SeekResponse>(
      std::move(cb), base::BindOnce(&HlsManifestDemuxerEngine::SeekAction,
                                    weak_factory_.GetWeakPtr(), time));
}

void HlsManifestDemuxerEngine::SeekAction(base::TimeDelta time,
                                          ManifestDemuxer::SeekCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  network_access_->AbortPendingReads(base::BindPostTaskToCurrentDefault(
      base::BindOnce(&HlsManifestDemuxerEngine::ContinueSeekInternal,
                     weak_factory_.GetWeakPtr(), time, std::move(cb))));
}

void HlsManifestDemuxerEngine::ContinueSeekInternal(
    base::TimeDelta time,
    ManifestDemuxer::SeekCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  bool buffers_needed = false;
  for (auto& [_, rendition] : renditions_) {
    auto response = rendition->Seek(time);
    if (!response.has_value()) {
      std::move(cb).Run(std::move(response).error().AddHere());
      return;
    }
    buffers_needed |=
        (ManifestDemuxer::SeekState::kNeedsData == std::move(response).value());
  }
  std::move(cb).Run(buffers_needed ? ManifestDemuxer::SeekState::kNeedsData
                                   : ManifestDemuxer::SeekState::kIsReady);
}

void HlsManifestDemuxerEngine::Initialize(ManifestDemuxerEngineHost* host,
                                          PipelineStatusCallback status_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);

  host_ = host;
  ProcessAsyncAction<PipelineStatus>(
      std::move(status_cb),
      base::BindOnce(&HlsManifestDemuxerEngine::InitAction,
                     weak_factory_.GetWeakPtr()));
}

void HlsManifestDemuxerEngine::InitAction(PipelineStatusCallback status_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  PlaylistParseInfo parse_info(root_playlist_uri_, {}, kPrimary,
                               /*allow_multivariant_playlist=*/true);
  LoadPlaylist(
      parse_info,
      base::BindOnce(&HlsManifestDemuxerEngine::FinishInitialization,
                     weak_factory_.GetWeakPtr(), std::move(status_cb)));
}

void HlsManifestDemuxerEngine::FinishInitialization(PipelineStatusCallback cb,
                                                    PipelineStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  std::move(cb).Run(std::move(status));
}

void HlsManifestDemuxerEngine::OnTimeUpdate(base::TimeDelta time,
                                            double playback_rate,
                                            ManifestDemuxer::DelayCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  CHECK_LE(renditions_.size(), 3lu);
  ProcessAsyncAction<base::TimeDelta>(
      std::move(cb),
      base::BindOnce(&HlsManifestDemuxerEngine::OnTimeUpdateAction,
                     weak_factory_.GetWeakPtr(), time, playback_rate));
}

void HlsManifestDemuxerEngine::OnTimeUpdateAction(
    base::TimeDelta time,
    double playback_rate,
    ManifestDemuxer::DelayCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media", "HLS::OnTimeUpdate", this);
  cb = base::BindOnce(&HlsManifestDemuxerEngine::FinishTimeUpdate,
                      weak_factory_.GetWeakPtr(), std::move(cb));
  for (const auto& [role, _] : renditions_) {
    cb = base::BindOnce(&HlsManifestDemuxerEngine::CheckState,
                        weak_factory_.GetWeakPtr(), time, playback_rate, role,
                        std::move(cb));
  }
  std::move(cb).Run(kNoTimestamp);
}

void HlsManifestDemuxerEngine::FinishTimeUpdate(
    ManifestDemuxer::DelayCallback cb,
    base::TimeDelta delay_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_END0("media", "HLS::OnTimeUpdate", this);
  std::move(cb).Run(std::move(delay_time));
}

void HlsManifestDemuxerEngine::CheckState(base::TimeDelta time,
                                          double playback_rate,
                                          std::string role,
                                          ManifestDemuxer::DelayCallback cb,
                                          base::TimeDelta delay_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  DCHECK(renditions_.contains(role));
  renditions_[role]->CheckState(
      time, playback_rate,
      base::BindOnce(&HlsManifestDemuxerEngine::OnStateChecked,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     delay_time, std::move(cb)));
}

void HlsManifestDemuxerEngine::OnStateChecked(base::TimeTicks start_time,
                                              base::TimeDelta prior_delay,
                                              ManifestDemuxer::DelayCallback cb,
                                              base::TimeDelta new_delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (prior_delay == kNoTimestamp) {
    std::move(cb).Run(new_delay);
    return;
  }

  if (new_delay == kNoTimestamp) {
    std::move(cb).Run(prior_delay);
    return;
  }

  base::TimeDelta spent_duration = base::TimeTicks::Now() - start_time;
  if (prior_delay <= spent_duration) {
    std::move(cb).Run(base::Seconds(0));
    return;
  }

  prior_delay -= spent_duration;
  if (prior_delay < new_delay) {
    std::move(cb).Run(prior_delay);
    return;
  }

  std::move(cb).Run(new_delay);
}

void HlsManifestDemuxerEngine::UpdateRenditionManifestUri(
    std::string role,
    GURL uri,
    base::OnceCallback<void(bool)> cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("media", "HLS::UpdateRenditionManifest",
                                    this, "uri", uri);
  GURL uri_copy = uri;
  ReadManifest(
      std::move(uri_copy),
      base::BindOnce(&HlsManifestDemuxerEngine::UpdateMediaPlaylistForRole,
                     weak_factory_.GetWeakPtr(), std::move(role),
                     std::move(uri), std::move(cb)));
}

void HlsManifestDemuxerEngine::UpdateMediaPlaylistForRole(
    std::string role,
    GURL uri,
    base::OnceCallback<void(bool)> cb,
    HlsDataSourceProvider::ReadResult maybe_stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!maybe_stream.has_value()) {
    Abort(std::move(maybe_stream).error().AddHere());
    std::move(cb).Run(false);
    return;
  }
  auto stream = std::move(maybe_stream).value();

  auto maybe_info = hls::Playlist::IdentifyPlaylist(stream->AsString());
  if (!maybe_info.has_value()) {
    Abort(std::move(maybe_info).error().AddHere());
    std::move(cb).Run(false);
    return;
  }

  if ((*maybe_info).kind != hls::Playlist::Kind::kMediaPlaylist) {
    Abort(HlsDemuxerStatus::Codes::kInvalidManifest);
    std::move(cb).Run(false);
    return;
  }

  auto maybe_playlist = ParseMediaPlaylistFromStringSource(
      stream->AsString(), std::move(uri), (*maybe_info).version);
  if (!maybe_playlist.has_value()) {
    Abort(std::move(maybe_playlist).error().AddHere());
    std::move(cb).Run(false);
    return;
  }
  TRACE_EVENT_NESTABLE_ASYNC_END0("media", "HLS::UpdateRenditionManifest",
                                  this);

  renditions_[role]->UpdatePlaylist(std::move(maybe_playlist).value(),
                                    std::nullopt);
  std::move(cb).Run(true);
}

void HlsManifestDemuxerEngine::OnRenditionsReselected(
    hls::AdaptationReason reason,
    const hls::VariantStream* variant,
    const hls::AudioRendition* audio_override_rendition) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  stats_reporter_.OnAdaptation(reason);
  ProcessAsyncAction<PipelineStatus>(
      base::BindOnce(&HlsManifestDemuxerEngine::OnStatus,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&HlsManifestDemuxerEngine::AdaptationAction,
                     weak_factory_.GetWeakPtr(), variant,
                     audio_override_rendition));
}

void HlsManifestDemuxerEngine::AdaptationAction(
    const hls::VariantStream* variant,
    const hls::AudioRendition* audio_override_rendition,
    PipelineStatusCallback status_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("media", "HLS::SelectRenditions", this,
                                    "reselect", true);

  OnRenditionsSelected(std::move(status_cb), variant, audio_override_rendition);
}

void HlsManifestDemuxerEngine::Abort(HlsDemuxerStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  OnStatus({PipelineStatus::Codes::DEMUXER_ERROR_COULD_NOT_PARSE,
            std::move(status)});
}

void HlsManifestDemuxerEngine::Abort(hls::ParseStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  OnStatus({PipelineStatus::Codes::DEMUXER_ERROR_COULD_NOT_PARSE,
            std::move(status)});
}

void HlsManifestDemuxerEngine::Abort(HlsDataSourceProvider::ReadStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  OnStatus(
      {PipelineStatus::Codes::DEMUXER_ERROR_COULD_NOT_OPEN, std::move(status)});
}

void HlsManifestDemuxerEngine::OnStatus(PipelineStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (host_ && !status.is_ok()) {
    host_->OnError(std::move(status).AddHere());
  }
}

void HlsManifestDemuxerEngine::UpdateHlsDataSourceStats(
    HlsDataSourceProvider::ReadCb cb,
    HlsDataSourceProvider::ReadStatus::Or<std::unique_ptr<HlsDataSourceStream>>
        result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!result.has_value()) {
    std::move(cb).Run(std::move(result).error().AddHere());
    return;
  }
  auto stream = std::move(result).value();
  origin_tainted_ |= stream->would_taint_origin();
  stats_reporter_.SetWouldTaintOrigin(origin_tainted_);
  total_stream_memory_ = stream->memory_usage();
  std::move(cb).Run(std::move(stream));
}

void HlsManifestDemuxerEngine::ReadKey(
    const hls::MediaSegment::EncryptionData& data,
    HlsDataSourceProvider::ReadCb cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  network_access_->ReadKey(std::move(data), BindStatsUpdate(std::move(cb)));
}

HlsDataSourceProvider::ReadCb HlsManifestDemuxerEngine::BindStatsUpdate(
    HlsDataSourceProvider::ReadCb cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  return base::BindOnce(&HlsManifestDemuxerEngine::UpdateHlsDataSourceStats,
                        weak_factory_.GetWeakPtr(), std::move(cb));
}

void HlsManifestDemuxerEngine::ReadManifest(const GURL& uri,
                                            HlsDataSourceProvider::ReadCb cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  network_access_->ReadManifest(std::move(uri), BindStatsUpdate(std::move(cb)));
}

void HlsManifestDemuxerEngine::ReadMediaSegment(
    const hls::MediaSegment& segment,
    bool read_chunked,
    bool include_init,
    HlsDataSourceProvider::ReadCb cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  network_access_->ReadMediaSegment(segment, read_chunked, include_init,
                                    BindStatsUpdate(std::move(cb)));
}

void HlsManifestDemuxerEngine::ReadStream(
    std::unique_ptr<HlsDataSourceStream> stream,
    HlsDataSourceProvider::ReadCb cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  network_access_->ReadStream(std::move(stream),
                              BindStatsUpdate(std::move(cb)));
}

void HlsManifestDemuxerEngine::UpdateNetworkSpeed(uint64_t bps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (rendition_manager_) {
    rendition_manager_->UpdateNetworkSpeed(bps);
  }
}

void HlsManifestDemuxerEngine::ParsePlaylist(
    PipelineStatusCallback parse_complete_cb,
    PlaylistParseInfo parse_info,
    HlsDataSourceProvider::ReadResult m_stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!m_stream.has_value()) {
    return Abort(std::move(m_stream).error().AddHere());
  }
  auto stream = std::move(m_stream).value();

  // A four hour movie manifest is ~100Kb.
  if (stream->buffer_size() > 102400) {
    MEDIA_LOG(WARNING, media_log_)
        << "Large Manifest detected: " << stream->buffer_size();
  }

  auto m_info = hls::Playlist::IdentifyPlaylist(stream->AsString());
  if (!m_info.has_value()) {
    return Abort(std::move(m_info).error().AddHere());
  }

  switch ((*m_info).kind) {
    case hls::Playlist::Kind::kMultivariantPlaylist: {
      if (!parse_info.allow_multivariant_playlist) {
        return Abort(HlsDemuxerStatus::Codes::kRecursiveMultivariantPlaylists);
      }
      stats_reporter_.SetIsMultivariantPlaylist(true);
      auto playlist = hls::MultivariantPlaylist::Parse(
          stream->AsString(), parse_info.uri, (*m_info).version);
      if (!playlist.has_value()) {
        return Abort(std::move(playlist).error().AddHere());
      }
      return OnMultivariantPlaylist(std::move(parse_complete_cb),
                                    std::move(playlist).value());
    }
    case hls::Playlist::Kind::kMediaPlaylist: {
      if (parse_info.allow_multivariant_playlist) {
        // Only a root playlist is allowed to be multivariant, so if the root
        // is only a media playlist, then this entire playback is not
        // multivariant.
        stats_reporter_.SetIsMultivariantPlaylist(false);
      }
      auto playlist = ParseMediaPlaylistFromStringSource(
          stream->AsString(), parse_info.uri, (*m_info).version);
      if (!playlist.has_value()) {
        return Abort(std::move(playlist).error().AddHere());
      }
      return OnMediaPlaylist(std::move(parse_complete_cb),
                             std::move(parse_info),
                             std::move(playlist).value());
    }
  }
}

hls::ParseStatus::Or<scoped_refptr<hls::MediaPlaylist>>
HlsManifestDemuxerEngine::ParseMediaPlaylistFromStringSource(
    std::string_view source,
    GURL uri,
    hls::types::DecimalInteger version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  return hls::MediaPlaylist::Parse(source, uri, version,
                                   multivariant_root_.get());
}

void HlsManifestDemuxerEngine::SetEndOfStream(bool ended) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  // If we receive a notice that the stream is no longer ended, assert that
  // the ended stream count is at least greater than zero for sanity checking.
  CHECK(ended || ended_stream_count_);
  if (ended && (++ended_stream_count_ == renditions_.size())) {
    host_->SetEndOfStream();
  }
  if (!ended && (--ended_stream_count_ == renditions_.size() - 1)) {
    host_->UnsetEndOfStream();
  }
}
void HlsManifestDemuxerEngine::AddRenditionForTesting(
    std::string role,
    std::unique_ptr<HlsRendition> test_rendition) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  bool is_seekable = test_rendition->GetDuration().has_value();
  CHECK_EQ(is_seekable_.value_or(is_seekable), is_seekable);
  is_seekable_ = is_seekable;
  renditions_[role] = std::move(test_rendition);
}

void HlsManifestDemuxerEngine::OnMultivariantPlaylist(
    PipelineStatusCallback parse_complete_cb,
    scoped_refptr<hls::MultivariantPlaylist> playlist) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  CHECK(!rendition_manager_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media", "HLS::SelectRenditions", this);
  multivariant_root_ = std::move(playlist);
  rendition_manager_ = std::make_unique<hls::RenditionManager>(
      multivariant_root_,
      base::BindRepeating(&HlsManifestDemuxerEngine::OnRenditionsReselected,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&GetSupportedTypes));

  if (!rendition_manager_->HasAnyVariants()) {
    // This will abort the pending init, and `parse_complete_cb` will not need
    // to be called.
    Abort(HlsDemuxerStatus::Codes::kNoRenditions);
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0("media", "HLS::LoadPlaylist", this);
  rendition_manager_->Reselect(
      base::BindOnce(&HlsManifestDemuxerEngine::OnRenditionsSelected,
                     weak_factory_.GetWeakPtr(), std::move(parse_complete_cb)));
}

void HlsManifestDemuxerEngine::OnRenditionsSelected(
    PipelineStatusCallback on_complete,
    const hls::VariantStream* variant,
    const hls::AudioRendition* audio_override_rendition) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);

  // Ensure that if the variant changes, then we update the codecs that are
  // expected. There can still be other codecs determined after parsing the
  // media content.
  if (variant) {
    std::vector<std::string> no_codecs;
    selected_variant_codecs_ = variant->GetCodecs().value_or(no_codecs);
  }

  // If nothing was selected, then we are in an unplayable state, regardless
  // of whether this is the first initialization or not.
  if (!audio_override_rendition && !variant) {
    std::move(on_complete).Run(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  // Bind the audio override rendition fetch into a closure. If we have to
  // reselect the primary rendition now, this will take the place of the
  // on_complete callback.
  if (audio_override_rendition) {
    PlaylistParseInfo override_parse_info = {
        audio_override_rendition->GetUri().value(), selected_variant_codecs_,
        kAudioOverride};

    on_complete = PipelineStatus::BindOkContinuation(
        std::move(on_complete),
        base::BindOnce(&HlsManifestDemuxerEngine::LoadPlaylist,
                       weak_factory_.GetWeakPtr(),
                       std::move(override_parse_info)));
  }
  TRACE_EVENT_NESTABLE_ASYNC_END0("media", "HLS::SelectRenditions", this);

  // If there is a variant change, just call LoadPlaylist directly. Since we've
  // already checked that variant and override are not both null, we need to
  // run the variant load CB.
  if (variant) {
    PlaylistParseInfo primary_parse_info = {variant->GetPrimaryRenditionUri(),
                                            selected_variant_codecs_, kPrimary};
    LoadPlaylist(std::move(primary_parse_info), std::move(on_complete));
  } else {
    std::move(on_complete).Run(PIPELINE_OK);
  }
}

void HlsManifestDemuxerEngine::LoadPlaylist(
    PlaylistParseInfo parse_info,
    PipelineStatusCallback on_complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  auto uri = parse_info.uri;
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("media", "HLS::LoadPlaylist", this, "uri",
                                    uri);
  ReadManifest(std::move(uri),
               base::BindOnce(&HlsManifestDemuxerEngine::ParsePlaylist,
                              weak_factory_.GetWeakPtr(),
                              std::move(on_complete), std::move(parse_info)));
}

void HlsManifestDemuxerEngine::OnMediaPlaylist(
    PipelineStatusCallback parse_complete_cb,
    PlaylistParseInfo parse_info,
    scoped_refptr<hls::MediaPlaylist> playlist) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);

  // TODO(crbug.com/40057824) On stream adaptation, if the codecs are not the
  // same, we'll have to re-create the chunk demuxer role. For now, just assume
  // the codecs are the same.
  auto maybe_exists = renditions_.find(parse_info.role);
  if (maybe_exists != renditions_.end()) {
    maybe_exists->second->UpdatePlaylist(std::move(playlist), parse_info.uri);
    TRACE_EVENT_NESTABLE_ASYNC_END0("media", "HLS::LoadPlaylist", this);
    std::move(parse_complete_cb).Run(OkStatus());
    return;
  }

  hls::MediaPlaylist* playlist_ptr = playlist.get();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "media", "HLS::DetermineStreamContainerAndCodecs", this);
  DetermineStreamContainer(
      playlist_ptr,
      base::BindOnce(&HlsManifestDemuxerEngine::OnStreamContainerDetermined,
                     weak_factory_.GetWeakPtr(), std::move(parse_complete_cb),
                     std::move(parse_info), std::move(playlist)));
}

void HlsManifestDemuxerEngine::OnStreamContainerDetermined(
    PipelineStatusCallback parse_complete_cb,
    PlaylistParseInfo parse_info,
    scoped_refptr<hls::MediaPlaylist> playlist,
    HlsDemuxerStatus::Or<RelaxedParserSupportedType> maybe_mime) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  CHECK(!renditions_.contains(parse_info.role));
  if (!maybe_mime.has_value()) {
    std::move(parse_complete_cb)
        .Run({DEMUXER_ERROR_COULD_NOT_OPEN, std::move(maybe_mime).error()});
    return;
  }

  if (!host_->AddRole(parse_info.role, std::move(maybe_mime).value())) {
    std::move(parse_complete_cb).Run(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  host_->SetSequenceMode(parse_info.role, true);

  auto rendition = HlsRendition::CreateRendition(
      host_, this, parse_info.role, std::move(playlist), parse_info.uri);

  if (parse_info.role == kPrimary) {
    auto duration_or_live = rendition->GetDuration();
    if (duration_or_live.has_value()) {
      host_->SetDuration(duration_or_live->InSecondsF());
    }
  }

  bool seekable = rendition->GetDuration().has_value();
  if (is_seekable_.value_or(seekable) != seekable) {
    std::move(parse_complete_cb).Run(DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }
  is_seekable_ = seekable;
  stats_reporter_.SetIsLiveContent(!seekable);
  renditions_[parse_info.role] = std::move(rendition);
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "media", "HLS::DetermineStreamContainerAndCodecs", this);
  TRACE_EVENT_NESTABLE_ASYNC_END0("media", "HLS::LoadPlaylist", this);
  std::move(parse_complete_cb).Run(OkStatus());
}

void HlsManifestDemuxerEngine::DetermineStreamContainer(
    hls::MediaPlaylist* playlist,
    HlsDemuxerStatusCb<RelaxedParserSupportedType> container_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  const auto& segments = playlist->GetSegments();
  if (segments.empty()) {
    std::move(container_cb).Run(HlsDemuxerStatus::Codes::kUnsupportedContainer);
    return;
  }

  // In the best case, we can just assert the mime type based on extension,
  // but if it's unrecognized, we have to fetch and parse it.
  const auto first_segment_path = segments[0]->GetUri().path_piece();

  std::optional<RelaxedParserSupportedType> mime = std::nullopt;
  if (first_segment_path.ends_with(".ts")) {
    mime = RelaxedParserSupportedType::kMP2T;
  } else if (first_segment_path.ends_with(".mp4")) {
    mime = RelaxedParserSupportedType::kMP4;
  } else if (first_segment_path.ends_with(".m4v")) {
    mime = RelaxedParserSupportedType::kMP4;
  } else if (first_segment_path.ends_with(".m4s")) {
    mime = RelaxedParserSupportedType::kMP4;
  } else if (first_segment_path.ends_with(".m4a")) {
    mime = RelaxedParserSupportedType::kMP4;
  } else if (first_segment_path.ends_with(".aac")) {
    mime = RelaxedParserSupportedType::kAAC;
  }

  if (mime.has_value()) {
    std::move(container_cb).Run(mime.value());
  } else {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("media", "HLS::PeekSegmentChunk", this,
                                      "uri", segments[0]->GetUri());
    bool read_chunked = true;
    if (auto enc_data = segments[0]->GetEncryptionData()) {
      switch (enc_data->GetMethod()) {
        case hls::XKeyTagMethod::kAES128:
        case hls::XKeyTagMethod::kAES256: {
          read_chunked = false;
          break;
        }
        default:
          break;
      }
    }

    ReadMediaSegment(
        *segments[0], read_chunked, /*include_init=*/true,
        base::BindOnce(&HlsManifestDemuxerEngine::DetermineBitstreamContainer,
                       weak_factory_.GetWeakPtr(), segments[0],
                       std::move(container_cb)));
  }
}

void HlsManifestDemuxerEngine::DetermineBitstreamContainer(
    scoped_refptr<hls::MediaSegment> segment,
    HlsDemuxerStatusCb<RelaxedParserSupportedType> cb,
    HlsDataSourceProvider::ReadResult maybe_stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  TRACE_EVENT_NESTABLE_ASYNC_END0("media", "HLS::PeekSegmentChunk", this);

  if (!maybe_stream.has_value()) {
    std::move(cb).Run(HlsDemuxerStatus::Codes::kInvalidSegmentUri);
    return;
  }

  auto stream = std::move(maybe_stream).value();
  if (!stream->buffer_size()) {
    std::move(cb).Run(HlsDemuxerStatus::Codes::kInvalidBitstream);
    return;
  }

  if (auto enc_data = segment->GetEncryptionData()) {
    switch (enc_data->GetMethod()) {
      case hls::XKeyTagMethod::kNone: {
        // Fall back to plaintext.
        break;
      }
      case hls::XKeyTagMethod::kAES128:
      case hls::XKeyTagMethod::kAES256: {
        auto decryptor = std::make_unique<crypto::Encryptor>();
        auto maybe_iv = enc_data->GetIVStr(segment->GetMediaSequenceNumber());
        auto mode = crypto::Encryptor::Mode::CBC;
        base::span<const uint8_t> stream_data =
            base::span(stream->raw_data(), stream->buffer_size());
        if (!maybe_iv.has_value()) {
          std::move(cb).Run(HlsDemuxerStatus::Codes::kInvalidBitstream);
          return;
        }
        auto iv = std::move(maybe_iv).value();
        if (!decryptor->Init(enc_data->GetKey(), mode, iv)) {
          std::move(cb).Run(HlsDemuxerStatus::Codes::kInvalidBitstream);
          return;
        }
        std::vector<uint8_t> plaintext;
        if (!decryptor->Decrypt(stream_data, &plaintext)) {
          std::move(cb).Run(HlsDemuxerStatus::Codes::kInvalidBitstream);
          return;
        }
        decryptor = nullptr;
        std::move(cb).Run(CheckBitstreamForContainerMagic(plaintext.data(),
                                                          plaintext.size()));
        return;
      }
      default: {
        std::move(cb).Run(HlsDemuxerStatus::Codes::kInvalidBitstream);
        return;
      }
    }
  }

  std::move(cb).Run(CheckBitstreamForContainerMagic(stream->raw_data(),
                                                    stream->buffer_size()));
}

void HlsManifestDemuxerEngine::OnChunkDemuxerParseWarning(
    std::string role,
    SourceBufferParseWarning warning) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  MEDIA_LOG(WARNING, media_log_)
      << "ParseWarning (" << role << "): " << static_cast<int>(warning);
}

void HlsManifestDemuxerEngine::OnChunkDemuxerTracksChanged(
    std::string role,
    std::unique_ptr<MediaTracks> tracks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  MEDIA_LOG(WARNING, media_log_) << "TracksChanged for role: " << role;
}

}  // namespace media
