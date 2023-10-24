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
      media_log_(media_log->Clone()) {
  // This is always created on the main sequence, but used on the media sequence
  DETACH_FROM_SEQUENCE(media_sequence_checker_);
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

std::string HlsManifestDemuxerEngine::GetName() const {
  return "HlsManifestDemuxer";
}

void HlsManifestDemuxerEngine::InitializeWithMockCodecDetectorForTesting(
    ManifestDemuxerEngineHost* host,
    PipelineStatusCallback cb,
    std::unique_ptr<HlsCodecDetector> codec_detector) {
  InitializeWithCodecDetector(host, std::move(cb), std::move(codec_detector));
}

void HlsManifestDemuxerEngine::InitializeWithCodecDetector(
    ManifestDemuxerEngineHost* host,
    PipelineStatusCallback status_cb,
    std::unique_ptr<HlsCodecDetector> codec_detector) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);

  // Initialize the codec detector on the media thread.
  codec_detector_ = std::move(codec_detector);
  host_ = host;
  PlaylistParseInfo parse_info(root_playlist_uri_, {}, kPrimary,
                               /*allow_multivariant_playlist=*/true);
  ReadFromUrl(root_playlist_uri_, false, absl::nullopt,
              base::BindOnce(&HlsManifestDemuxerEngine::ParsePlaylist,
                             weak_factory_.GetWeakPtr(), std::move(status_cb),
                             std::move(parse_info)));
}

void HlsManifestDemuxerEngine::Initialize(ManifestDemuxerEngineHost* host,
                                          PipelineStatusCallback status_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  InitializeWithCodecDetector(
      host, std::move(status_cb),
      std::make_unique<HlsCodecDetectorImpl>(media_log_.get(), this));
}

void HlsManifestDemuxerEngine::OnTimeUpdate(base::TimeDelta time,
                                            double playback_rate,
                                            ManifestDemuxer::DelayCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (renditions_.empty()) {
    std::move(cb).Run(kNoTimestamp);
    return;
  }

  CheckStateAtIndex(time, playback_rate, std::move(cb), 0, absl::nullopt);
}

void HlsManifestDemuxerEngine::CheckStateAtIndex(
    base::TimeDelta media_time,
    double playback_rate,
    ManifestDemuxer::DelayCallback cb,
    size_t rendition_index,
    absl::optional<base::TimeDelta> response_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (rendition_index >= renditions_.size()) {
    // The response time collected at this point _must_ be valid.
    std::move(cb).Run(response_time.value());
    return;
  }

  auto recurse = base::BindOnce(
      &HlsManifestDemuxerEngine::CheckStateAtIndex, weak_factory_.GetWeakPtr(),
      media_time, playback_rate, std::move(cb), rendition_index + 1);

  auto on_reply = base::BindOnce(
      &HlsManifestDemuxerEngine::OnStateChecked, weak_factory_.GetWeakPtr(),
      base::TimeTicks::Now(), response_time, std::move(recurse));

  renditions_[rendition_index]->CheckState(media_time, playback_rate,
                                           std::move(on_reply));
}

void HlsManifestDemuxerEngine::OnStateChecked(
    base::TimeTicks call_start,
    absl::optional<base::TimeDelta> prior_delay,
    base::OnceCallback<void(absl::optional<base::TimeDelta>)> cb,
    base::TimeDelta delay_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (prior_delay.value_or(kNoTimestamp) == kNoTimestamp) {
    std::move(cb).Run(delay_time);
    return;
  }

  base::TimeDelta spent_duration = base::TimeTicks::Now() - call_start;
  if (spent_duration > prior_delay.value()) {
    // Some previous rendition requested a delay that we've already spent while
    // calculating the delay for the current rendition. Going forward then,
    // we want to have no delay.
    std::move(cb).Run(base::Seconds(0));
    return;
  }

  auto adjusted_prior_delay = prior_delay.value() - spent_duration;
  if (delay_time == kNoTimestamp) {
    std::move(cb).Run(adjusted_prior_delay);
    return;
  }

  std::move(cb).Run(adjusted_prior_delay > delay_time ? delay_time
                                                      : adjusted_prior_delay);
}

void HlsManifestDemuxerEngine::Seek(base::TimeDelta time,
                                    ManifestDemuxer::SeekCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!data_source_provider_) {
    // The pipeline can call Seek just after an error was surfaced. The error
    // handler resets |data_source_provider_|, so we should just reply with
    // another error here.
    std::move(cb).Run(PIPELINE_ERROR_ABORT);
    return;
  }
  data_source_provider_.AsyncCall(&HlsDataSourceProvider::AbortPendingReads)
      .WithArgs(base::BindPostTaskToCurrentDefault(
          base::BindOnce(&HlsManifestDemuxerEngine::ContinueSeekInternal,
                         weak_factory_.GetWeakPtr(), time, std::move(cb))));
}

void HlsManifestDemuxerEngine::ContinueSeekInternal(
    base::TimeDelta time,
    ManifestDemuxer::SeekCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  bool buffers_needed = false;
  for (auto& rendition : renditions_) {
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

void HlsManifestDemuxerEngine::StartWaitingForSeek() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  for (auto& rendition : renditions_) {
    rendition->StartWaitingForSeek();
  }
}

void HlsManifestDemuxerEngine::AbortPendingReads() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
}

bool HlsManifestDemuxerEngine::IsSeekable() const {
  // `IsSeekable()` is only called after the pipeline has completed successfully
  // or the initialization step fails. If init fails, we should report that the
  // player is seekable to keep consistent behavior with other players.
  return is_seekable_.value_or(true);
}

int64_t HlsManifestDemuxerEngine::GetMemoryUsage() const {
  // TODO(crbug/1266991): Sum the memory of the renditions and data source
  // providers.
  return 0;
}

void HlsManifestDemuxerEngine::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  AbortPendingReads();
  for (auto& rendition : renditions_) {
    rendition->Stop();
  }

  data_source_provider_.Reset();
  weak_factory_.InvalidateWeakPtrs();

  multivariant_root_.reset();
  rendition_selector_.reset();
  renditions_.clear();
  host_ = nullptr;
}

void HlsManifestDemuxerEngine::Abort(HlsDemuxerStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!host_) {
    return;
  }
  host_->OnError({PipelineStatus::Codes::DEMUXER_ERROR_COULD_NOT_PARSE,
                  std::move(status)});
}

void HlsManifestDemuxerEngine::Abort(hls::ParseStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!host_) {
    return;
  }
  host_->OnError({PipelineStatus::Codes::DEMUXER_ERROR_COULD_NOT_PARSE,
                  std::move(status)});
}

void HlsManifestDemuxerEngine::Abort(HlsDataSourceProvider::ReadStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!host_) {
    return;
  }
  host_->OnError(
      {PipelineStatus::Codes::DEMUXER_ERROR_COULD_NOT_OPEN, std::move(status)});
}

void HlsManifestDemuxerEngine::ReadUntilExhausted(
    HlsDataSourceProvider::ReadCb cb,
    HlsDataSourceProvider::ReadStatus::Or<std::unique_ptr<HlsDataSourceStream>>
        result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!result.has_value()) {
    std::move(cb).Run(std::move(result).error());
    return;
  }
  auto stream = std::move(result).value();
  if (!stream->CanReadMore()) {
    std::move(cb).Run(std::move(stream));
    return;
  }

  ReadStream(std::move(stream),
             base::BindOnce(&HlsManifestDemuxerEngine::ReadUntilExhausted,
                            weak_factory_.GetWeakPtr(), std::move(cb)));
}

void HlsManifestDemuxerEngine::ReadFromUrl(
    GURL uri,
    bool read_chunked,
    absl::optional<hls::types::ByteRange> range,
    HlsDataSourceProvider::ReadCb cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!data_source_provider_) {
    std::move(cb).Run(HlsDataSourceProvider::ReadStatus::Codes::kAborted);
    return;
  }

  if (read_chunked) {
    cb = base::BindOnce(&HlsManifestDemuxerEngine::ReadUntilExhausted,
                        weak_factory_.GetWeakPtr(), std::move(cb));
  }

  data_source_provider_.AsyncCall(&HlsDataSourceProvider::ReadFromUrl)
      .WithArgs(std::move(uri), range,
                base::BindPostTaskToCurrentDefault(std::move(cb)));
}

void HlsManifestDemuxerEngine::ReadStream(
    std::unique_ptr<HlsDataSourceStream> stream,
    HlsDataSourceProvider::ReadCb cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!data_source_provider_) {
    std::move(cb).Run(HlsDataSourceProvider::ReadStatus::Codes::kAborted);
    return;
  }
  data_source_provider_
      .AsyncCall(&HlsDataSourceProvider::ReadFromExistingStream)
      .WithArgs(std::move(stream),
                base::BindPostTaskToCurrentDefault(std::move(cb)));
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
      auto playlist = hls::MultivariantPlaylist::Parse(
          stream->AsString(), parse_info.uri, (*m_info).version);
      if (!playlist.has_value()) {
        return Abort(std::move(playlist).error().AddHere());
      }
      return OnMultivariantPlaylist(std::move(parse_complete_cb),
                                    std::move(playlist).value());
    }
    case hls::Playlist::Kind::kMediaPlaylist: {
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
    base::StringPiece source,
    GURL uri,
    hls::types::DecimalInteger version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  return hls::MediaPlaylist::Parse(source, uri, version,
                                   multivariant_root_.get());
}

void HlsManifestDemuxerEngine::AddRenditionForTesting(
    std::unique_ptr<HlsRendition> test_rendition) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  bool is_seekable = test_rendition->GetDuration().has_value();
  CHECK_EQ(is_seekable_.value_or(is_seekable), is_seekable);
  is_seekable_ = is_seekable;
  renditions_.push_back(std::move(test_rendition));
}

void HlsManifestDemuxerEngine::OnMultivariantPlaylist(
    PipelineStatusCallback parse_complete_cb,
    scoped_refptr<hls::MultivariantPlaylist> playlist) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  CHECK(!rendition_selector_);
  multivariant_root_ = std::move(playlist);
  rendition_selector_ = std::make_unique<hls::RenditionSelector>(
      multivariant_root_, base::BindRepeating(&GetSupportedTypes));

  hls::RenditionSelector::PreferredVariants streams =
      rendition_selector_->GetPreferredVariants(video_preferences_,
                                                audio_preferences_);

  // Possible outcomes of the rendition selector:
  // | AOVariant | SelVariant | AORend  | primary=? | secondary=? |
  // |-----------|------------|---------|-----------|-------------|
  // | null      | null       | null    | X         | X           |
  // |-----------|------------|---------|-----------|-------------|
  // | null      | present    | null    | SV        | X           |
  // |-----------|------------|---------|-----------|-------------|
  // | present   | null       | present | AOV       | X           |
  // |-----------|------------|---------|-----------|-------------|
  // | present   | present    | null    | SV        | X           |
  // |-----------|------------|---------|-----------|-------------|
  // | present   | present    | present | SV        | AOV         |
  // |-----------|------------|---------|-----------|-------------|
  absl::optional<GURL> audio_override_uri;
  const GURL& primary_uri = streams.selected_variant->GetPrimaryRenditionUri();
  if (streams.audio_override_rendition) {
    CHECK_NE(streams.audio_override_variant, nullptr);
    audio_override_uri = streams.audio_override_rendition->GetUri().value_or(
        streams.audio_override_variant->GetPrimaryRenditionUri());
  }

  std::vector<PlaylistParseInfo> renditions_to_parse;
  std::vector<std::string> no_codecs;

  if (streams.selected_variant) {
    renditions_to_parse.emplace_back(
        streams.selected_variant->GetPrimaryRenditionUri(),
        streams.selected_variant->GetCodecs().value_or(no_codecs), kPrimary);

    if (streams.audio_override_rendition &&
        primary_uri != audio_override_uri.value_or(primary_uri)) {
      CHECK_NE(streams.audio_override_variant, nullptr);
      renditions_to_parse.emplace_back(
          *audio_override_uri,
          streams.audio_override_variant->GetCodecs().value_or(no_codecs),
          kAudioOverride);
    }
  } else if (streams.audio_override_rendition &&
             primary_uri != audio_override_uri.value_or(primary_uri)) {
    renditions_to_parse.emplace_back(
        *audio_override_uri,
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  hls::MediaPlaylist* playlist_ptr = playlist.get();
  DetermineStreamContainerAndCodecs(
      playlist_ptr,
      base::BindOnce(&HlsManifestDemuxerEngine::OnPlaylistContainerDetermined,
                     weak_factory_.GetWeakPtr(), std::move(parse_complete_cb),
                     std::move(parse_info), std::move(playlist)));
}

void HlsManifestDemuxerEngine::OnPlaylistContainerDetermined(
    PipelineStatusCallback parse_complete_cb,
    PlaylistParseInfo parse_info,
    scoped_refptr<hls::MediaPlaylist> playlist,
    HlsDemuxerStatus::Or<HlsCodecDetector::ContainerAndCodecs> maybe_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
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
  renditions_.push_back(std::move(rendition));
  std::move(parse_complete_cb).Run(OkStatus());
}

void HlsManifestDemuxerEngine::DetermineStreamContainerAndCodecs(
    hls::MediaPlaylist* playlist,
    HlsDemuxerStatusCb<HlsCodecDetector::ContainerAndCodecs> container_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  const auto& segments = playlist->GetSegments();
  if (segments.empty()) {
    std::move(container_cb).Run(HlsDemuxerStatus::Codes::kUnsupportedContainer);
    return;
  }

  ReadFromUrl(
      segments[0]->GetUri(), true, segments[0]->GetByteRange(),
      base::BindOnce(&HlsManifestDemuxerEngine::PeekFirstSegment,
                     weak_factory_.GetWeakPtr(), std::move(container_cb)));
}

void HlsManifestDemuxerEngine::PeekFirstSegment(
    HlsDemuxerStatusCb<HlsCodecDetector::ContainerAndCodecs> cb,
    HlsDataSourceProvider::ReadResult maybe_stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!maybe_stream.has_value()) {
    std::move(cb).Run(HlsDemuxerStatus::Codes::kInvalidSegmentUri);
    return;
  }
  codec_detector_->DetermineContainerAndCodec(std::move(maybe_stream).value(),
                                              std::move(cb));
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
