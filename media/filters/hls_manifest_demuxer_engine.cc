// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_manifest_demuxer_engine.h"

#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/audio_codecs.h"
#include "media/base/media_log.h"
#include "media/base/media_track.h"
#include "media/base/pipeline_status.h"
#include "media/base/supported_types.h"
#include "media/base/video_codecs.h"
#include "media/filters/manifest_demuxer.h"
#include "media/formats/hls/audio_rendition.h"
#include "media/formats/hls/media_playlist.h"
#include "media/formats/hls/multivariant_playlist.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variant_stream.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

bool AreAllAudioCodecsSupported(std::vector<AudioType> audio_types) {
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

bool AreAllVideoCodecsSupported(std::vector<VideoType> video_types) {
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

hls::RenditionSelector::CodecSupportType GetSupportedTypes(
    base::StringPiece container,
    base::span<const std::string> codecs) {
  std::vector<VideoType> video_formats;
  std::vector<AudioType> audio_formats;
  for (const std::string& codec : codecs) {
    // Try parsing it as a video codec first, which will set `video.codec`
    // to unknown if it fails.
    VideoType video;
    uint8_t video_level;
    video.hdr_metadata_type = gfx::HdrMetadataType::kNone;
    ParseCodec(codec, video.codec, video.profile, video_level,
               video.color_space);
    if (video.codec != VideoCodec::kUnknown) {
      video.level = video_level;
      video_formats.push_back(video);
      continue;
    }

    AudioType audio;
    if (ParseAudioCodec(codec, &audio)) {
      audio_formats.push_back(audio);
    }
  }

  bool audio_support = AreAllAudioCodecsSupported(std::move(audio_formats));
  bool video_support = AreAllVideoCodecsSupported(std::move(video_formats));

  if (audio_support && video_support) {
    return hls::RenditionSelector::CodecSupportType::kSupportedAudioVideo;
  }
  if (audio_support) {
    return hls::RenditionSelector::CodecSupportType::kSupportedAudioOnly;
  }
  if (video_support) {
    return hls::RenditionSelector::CodecSupportType::kSupportedVideoOnly;
  }
  return hls::RenditionSelector::CodecSupportType::kUnsupported;
}

}  // namespace

HlsManifestDemuxerEngine::~HlsManifestDemuxerEngine() = default;

HlsManifestDemuxerEngine::HlsManifestDemuxerEngine(
    base::SequenceBound<HlsDataSourceProvider> dsp,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    GURL root_playlist_uri,
    MediaLog* media_log)
    : data_source_provider_(std::move(dsp)),
      media_task_runner_(std::move(media_task_runner)),
      root_playlist_uri_(std::move(root_playlist_uri)),
      media_log_(media_log->Clone()) {}

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

std::string HlsManifestDemuxerEngine::GetName() const {
  return "HlsManifestDemuxer";
}

void HlsManifestDemuxerEngine::Initialize(ManifestDemuxerEngineHost* host,
                                          PipelineStatusCallback status_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // Initialize the codec detector on the media thread.
  codec_detector_ = std::make_unique<HlsCodecDetector>(media_log_.get());
  host_ = host;
  PlaylistParseInfo parse_info(root_playlist_uri_, {}, kPrimary,
                               /*allow_multivariant_playlist=*/true);
  ReadFromUrl(root_playlist_uri_, false, absl::nullopt,
              base::BindOnce(&HlsManifestDemuxerEngine::ParsePlaylist,
                             weak_factory_.GetWeakPtr(), std::move(status_cb),
                             std::move(parse_info)));
}

void HlsManifestDemuxerEngine::OnTimeUpdate(base::TimeDelta time,
                                            double playback_rate,
                                            ManifestDemuxer::DelayCallback cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // TODO(crbug/1266991): Run all renditions in series.
  std::move(cb).Run(kNoTimestamp);
  NOTIMPLEMENTED();
}

bool HlsManifestDemuxerEngine::Seek(base::TimeDelta time) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  bool needs_more_data = false;
  for (auto& rendition : renditions_) {
    needs_more_data |= rendition->Seek(time);
  }
  return needs_more_data;
}

void HlsManifestDemuxerEngine::StartWaitingForSeek() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  for (auto& rendition : renditions_) {
    rendition->CancelPendingNetworkRequests();
  }
}

void HlsManifestDemuxerEngine::AbortPendingReads() {}

bool HlsManifestDemuxerEngine::IsSeekable() {
  // TODO(crbug/1266991): Check that all renditions are either live or non, and
  // determine how to surface an error in the case where they report liveness
  // differently.
  for (auto& rendition : renditions_) {
    if (!rendition->DurationOrLive().has_value()) {
      return false;
    }
  }
  return true;
}

int64_t HlsManifestDemuxerEngine::GetMemoryUsage() const {
  // TODO(crbug/1266991): Sum the memory of the renditions and data source
  // providers.
  return 0;
}

void HlsManifestDemuxerEngine::Stop() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  // Note, this drops all data sources, it does not clear the
  // `data_source_provider_` pointer.
  data_source_provider_.Reset();

  multivariant_root_.reset();
  rendition_selector_.reset();
  renditions_.clear();
}

void HlsManifestDemuxerEngine::Abort(HlsDemuxerStatus status) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  host_->OnError({PipelineStatus::Codes::DEMUXER_ERROR_COULD_NOT_PARSE,
                  std::move(status)});
}

void HlsManifestDemuxerEngine::Abort(hls::ParseStatus status) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  host_->OnError({PipelineStatus::Codes::DEMUXER_ERROR_COULD_NOT_PARSE,
                  std::move(status)});
}

void HlsManifestDemuxerEngine::Abort(HlsDataSource::ReadStatus status) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  host_->OnError(
      {PipelineStatus::Codes::DEMUXER_ERROR_COULD_NOT_OPEN, std::move(status)});
}

void HlsManifestDemuxerEngine::ReadDataSource(
    bool read_chunked,
    HlsDataSourceStream::ReadCb cb,
    std::unique_ptr<HlsDataSource> source) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (!source) {
    Abort(HlsDemuxerStatus::Codes::kPlaylistUrlInvalid);
    return;
  }

  auto stream = HlsDataSourceStream(std::move(source));
  if (read_chunked) {
    std::move(stream).ReadChunk(
        base::BindPostTask(media_task_runner_, std::move(cb)));
  } else {
    std::move(stream).ReadAll(
        base::BindPostTask(media_task_runner_, std::move(cb)));
  }
}

void HlsManifestDemuxerEngine::ReadFromUrl(
    GURL uri,
    bool read_chunked,
    absl::optional<hls::types::ByteRange> range,
    HlsDataSourceStream::ReadCb cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  data_source_provider_.AsyncCall(&HlsDataSourceProvider::RequestDataSource)
      .WithArgs(std::move(uri), range,
                base::BindPostTask(
                    media_task_runner_,
                    base::BindOnce(&HlsManifestDemuxerEngine::ReadDataSource,
                                   weak_factory_.GetWeakPtr(), read_chunked,
                                   std::move(cb))));
}

void HlsManifestDemuxerEngine::ParsePlaylist(
    PipelineStatusCallback parse_complete_cb,
    PlaylistParseInfo parse_info,
    HlsDataSourceStream::ReadResult m_stream) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (!m_stream.has_value()) {
    return Abort(std::move(m_stream).error().AddHere());
  }
  auto stream = std::move(m_stream).value();
  DCHECK(!stream.CanReadMore());

  // A four hour movie manifest is ~100Kb.
  if (stream.BytesInBuffer() > 102400) {
    MEDIA_LOG(WARNING, media_log_)
        << "Large Manifest detected: " << stream.BytesInBuffer();
  }

  auto m_info = hls::Playlist::IdentifyPlaylist(stream.AsStringPiece());
  if (!m_info.has_value()) {
    return Abort(std::move(m_info).error().AddHere());
  }

  switch ((*m_info).kind) {
    case hls::Playlist::Kind::kMultivariantPlaylist: {
      if (!parse_info.allow_multivariant_playlist) {
        return Abort(HlsDemuxerStatus::Codes::kRecursiveMultivariantPlaylists);
      }
      auto playlist = hls::MultivariantPlaylist::Parse(
          stream.AsStringPiece(), parse_info.uri, (*m_info).version);
      if (!playlist.has_value()) {
        return Abort(std::move(playlist).error().AddHere());
      }
      return OnMultivariantPlaylist(std::move(parse_complete_cb),
                                    std::move(playlist).value());
    }
    case hls::Playlist::Kind::kMediaPlaylist: {
      auto playlist = ParseMediaPlaylistFromStream(
          std::move(stream), parse_info.uri, (*m_info).version);
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
HlsManifestDemuxerEngine::ParseMediaPlaylistFromStream(
    HlsDataSourceStream stream,
    GURL uri,
    hls::types::DecimalInteger version) {
  return hls::MediaPlaylist::Parse(stream.AsStringPiece(), uri, version,
                                   multivariant_root_.get());
}

void HlsManifestDemuxerEngine::OnMultivariantPlaylist(
    PipelineStatusCallback parse_complete_cb,
    scoped_refptr<hls::MultivariantPlaylist> playlist) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  CHECK(!rendition_selector_);
  multivariant_root_ = std::move(playlist);
  rendition_selector_ = std::make_unique<hls::RenditionSelector>(
      multivariant_root_, base::BindRepeating(&GetSupportedTypes));

  hls::RenditionSelector::PreferredVariants streams =
      rendition_selector_->GetPreferredVariants(video_preferences_,
                                                audio_preferences_);

  if (streams.audio_override_rendition &&
      !streams.audio_override_rendition->GetUri().has_value()) {
    return Abort(HlsDemuxerStatus::Codes::kPlaylistUrlInvalid);
  }

  std::vector<PlaylistParseInfo> renditions_to_parse;
  std::vector<std::string> no_codecs;

  if (streams.selected_variant) {
    renditions_to_parse.emplace_back(
        streams.selected_variant->GetPrimaryRenditionUri(),
        streams.selected_variant->GetCodecs().value_or(no_codecs), kPrimary);

    if (streams.audio_override_rendition) {
      CHECK_NE(streams.audio_override_variant, nullptr);
      renditions_to_parse.emplace_back(
          streams.audio_override_rendition->GetUri().value_or(GURL{}),
          streams.audio_override_variant->GetCodecs().value_or(no_codecs),
          kAudioOverride);
    }
  } else if (streams.audio_override_rendition) {
    renditions_to_parse.emplace_back(
        streams.audio_override_rendition->GetUri().value_or(GURL{}),
        streams.audio_override_variant->GetCodecs().value_or(no_codecs),
        kPrimary);
  } else {
    Abort(HlsDemuxerStatus::Codes::kNoRenditions);
    return;
  }

  SetStreams(std::move(renditions_to_parse), std::move(parse_complete_cb),
             PIPELINE_OK);
}

void HlsManifestDemuxerEngine::SetStreams(
    std::vector<PlaylistParseInfo> playlists,
    PipelineStatusCallback cb,
    PipelineStatus exit_on_error) {
  if (!exit_on_error.is_ok() || playlists.empty()) {
    // We've either hit the end of the list with a success, or have errored out
    // early. Either way, the status should be forwarded to the cb.
    std::move(cb).Run(std::move(exit_on_error));
    return;
  }

  const PlaylistParseInfo playlist = playlists.back();
  playlists.pop_back();
  PipelineStatusCallback on_parsed = base::BindOnce(
      &HlsManifestDemuxerEngine::SetStreams, weak_factory_.GetWeakPtr(),
      std::move(playlists), std::move(cb));

  GURL uri = playlist.uri;
  ReadFromUrl(uri, false, absl::nullopt,
              base::BindOnce(&HlsManifestDemuxerEngine::ParsePlaylist,
                             weak_factory_.GetWeakPtr(), std::move(on_parsed),
                             std::move(playlist)));
}

void HlsManifestDemuxerEngine::OnMediaPlaylist(
    PipelineStatusCallback parse_complete_cb,
    PlaylistParseInfo parse_info,
    scoped_refptr<hls::MediaPlaylist> playlist) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  hls::MediaPlaylist* playlist_ptr = playlist.get();
  DetermineStreamContainerAndCodecs(
      playlist_ptr, parse_info,
      base::BindOnce(&HlsManifestDemuxerEngine::OnPlaylistContainerDetermined,
                     weak_factory_.GetWeakPtr(), std::move(parse_complete_cb),
                     std::move(parse_info), std::move(playlist)));
}

void HlsManifestDemuxerEngine::OnPlaylistContainerDetermined(
    PipelineStatusCallback parse_complete_cb,
    PlaylistParseInfo parse_info,
    scoped_refptr<hls::MediaPlaylist> playlist,
    HlsDemuxerStatus::Or<HlsCodecDetector::ContainerAndCodecs> maybe_info) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (!maybe_info.has_value()) {
    std::move(parse_complete_cb)
        .Run({DEMUXER_ERROR_COULD_NOT_OPEN, std::move(maybe_info).error()});
    return;
  }

  auto container_and_codecs = std::move(maybe_info).value();
  std::string container = std::move(container_and_codecs.container);
  std::string codecs = std::move(container_and_codecs.codecs);

  if (parse_info.codecs.size()) {
    // Codecs came from a multivariant playlist rather than from detection. We
    // want to join whatever came from the playlist and use that instead.
    std::stringstream codecstream;
    std::copy(parse_info.codecs.begin(), parse_info.codecs.end(),
              std::ostream_iterator<std::string>(codecstream, ", "));
    codecs = codecstream.str();

    // codecs ends with a trailing ", " now, so we need to drop that
    codecs = codecs.substr(0, codecs.length() - 2);
  }

  if (!host_->AddRole(parse_info.role, container, codecs)) {
    std::move(parse_complete_cb).Run(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  host_->SetSequenceMode(parse_info.role, true);

  auto m_rendition = HlsRendition::CreateRendition(
      host_, this, parse_info.role, std::move(playlist), parse_info.uri);

  if (!m_rendition.has_value()) {
    std::move(parse_complete_cb)
        .Run({DEMUXER_ERROR_COULD_NOT_PARSE,
              std::move(m_rendition).error().AddHere()});
    return;
  }

  auto rendition = std::move(m_rendition).value();

  if (parse_info.role == kPrimary) {
    auto duration_or_live = rendition->DurationOrLive();
    if (duration_or_live.has_value()) {
      host_->SetDuration(duration_or_live->InSecondsF());
    }
  }

  renditions_.push_back(std::move(rendition));
  std::move(parse_complete_cb).Run(OkStatus());
}

void HlsManifestDemuxerEngine::DetermineStreamContainerAndCodecs(
    hls::MediaPlaylist* playlist,
    PlaylistParseInfo parse_info,
    HlsDemuxerStatusCb<HlsCodecDetector::ContainerAndCodecs> container_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  const auto& segments = playlist->GetSegments();
  if (segments.empty()) {
    std::move(container_cb).Run(HlsDemuxerStatus::Codes::kUnsupportedContainer);
    return;
  }

  const auto& first_segment_uri = segments[0]->GetUri();
  data_source_provider_.AsyncCall(&HlsDataSourceProvider::RequestDataSource)
      .WithArgs(std::move(first_segment_uri), segments[0]->GetByteRange(),
                base::BindPostTask(
                    media_task_runner_,
                    base::BindOnce(&HlsManifestDemuxerEngine::PeekFirstSegment,
                                   weak_factory_.GetWeakPtr(), parse_info,
                                   std::move(container_cb))));
}

void HlsManifestDemuxerEngine::PeekFirstSegment(
    PlaylistParseInfo parse_info,
    HlsDemuxerStatusCb<HlsCodecDetector::ContainerAndCodecs> cb,
    std::unique_ptr<HlsDataSource> data_source) {
  if (!data_source) {
    std::move(cb).Run(HlsDemuxerStatus::Codes::kInvalidSegmentUri);
    return;
  }

  auto stream = HlsDataSourceStream(std::move(data_source));
  codec_detector_->DetermineContainerAndCodec(std::move(stream), std::move(cb));
}

void HlsManifestDemuxerEngine::OnChunkDemuxerParseWarning(
    std::string role,
    SourceBufferParseWarning warning) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  MEDIA_LOG(WARNING, media_log_)
      << "ParseWarning (" << role << "): " << static_cast<int>(warning);
}

void HlsManifestDemuxerEngine::OnChunkDemuxerTracksChanged(
    std::string role,
    std::unique_ptr<MediaTracks> tracks) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  MEDIA_LOG(WARNING, media_log_) << "TracksChanged for role: " << role;
}

}  // namespace media
