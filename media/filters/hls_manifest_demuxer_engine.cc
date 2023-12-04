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

hls::RenditionManager::CodecSupportType GetSupportedTypes(
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
    return hls::RenditionManager::CodecSupportType::kSupportedAudioVideo;
  }
  if (audio_support) {
    return hls::RenditionManager::CodecSupportType::kSupportedAudioOnly;
  }
  if (video_support) {
    return hls::RenditionManager::CodecSupportType::kSupportedVideoOnly;
  }
  return hls::RenditionManager::CodecSupportType::kUnsupported;
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

  // Because multiple renditions may be appending data to the chunk demuxer,
  // it's possible that the chunk demuxer could be ready after the primary
  // rendition data is appended, but before the audio override rendition has
  // even finished its manifest parsing. When this happens, the pipeline will
  // try to call `Seek` on us. Seeks should be postponed until this class is
  // truly initialized.
  pending_initialization_ = true;

  // Initialize the codec detector on the media thread.
  codec_detector_ = std::move(codec_detector);
  host_ = host;
  PlaylistParseInfo parse_info(root_playlist_uri_, {}, kPrimary,
                               /*allow_multivariant_playlist=*/true);

  LoadPlaylist(
      parse_info,
      base::BindOnce(&HlsManifestDemuxerEngine::FinishInitialization,
                     weak_factory_.GetWeakPtr(), std::move(status_cb)));
}

void HlsManifestDemuxerEngine::Initialize(ManifestDemuxerEngineHost* host,
                                          PipelineStatusCallback status_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  InitializeWithCodecDetector(
      host, std::move(status_cb),
      std::make_unique<HlsCodecDetectorImpl>(media_log_.get(), this));
}

void HlsManifestDemuxerEngine::FinishInitialization(PipelineStatusCallback cb,
                                                    PipelineStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  pending_initialization_ = false;
  std::move(cb).Run(std::move(status));
  // If this is a multi-rendition playback, post the pending seek task to the
  // current task runner.
  if (pending_seek_closure_) {
    std::move(pending_seek_closure_).Run();
  }
}

void HlsManifestDemuxerEngine::OnTimeUpdate(base::TimeDelta time,
                                            double playback_rate,
                                            ManifestDemuxer::DelayCallback cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  // HLS supports a max of three renditions: primary, audio override, and
  // subtitles. As of now, we only support primary and audio override.
  CHECK_LE(renditions_.size(), 3lu);

  // Capture each role into a sequential closure then run the whole thing.
  for (const auto& [role, _] : renditions_) {
    cb = base::BindOnce(&HlsManifestDemuxerEngine::CheckState,
                        weak_factory_.GetWeakPtr(), time, playback_rate, role,
                        std::move(cb));
  }
  std::move(cb).Run(kNoTimestamp);
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

  if (pending_initialization_ || pending_adaptation_) {
    // In multivariant streams, the primary rendition might finish loading and
    // trigger the ChunkDemuxer to finish it's initialization process, leading
    // to the media pipeline to issue a seek. If there is an audio override
    // rendition that hasn't yet been loaded, we need to block that seek until
    // the audio override rendition has finished loading as well. Completing
    // our initialization process will then re-launch `pending_seek_closure_`.

    // Similarly, when a pending adaptation is in progress, we must wait to
    // abort network requests. The adaptation will update the queue of segments
    // into which the seek must happen. This adaptation must complete either
    // before or after the abort takes place, so we might as well postpone the
    // abort until after that parsing happens. Finishing the adaptation will
    // similarly re-launch `pending_seek_closure_`.
    pending_seek_closure_ =
        base::BindOnce(&HlsManifestDemuxerEngine::Seek,
                       weak_factory_.GetWeakPtr(), time, std::move(cb));
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

void HlsManifestDemuxerEngine::StartWaitingForSeek() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  for (auto& [_, rendition] : renditions_) {
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
  for (auto& [_, rendition] : renditions_) {
    rendition->Stop();
  }

  data_source_provider_.Reset();
  weak_factory_.InvalidateWeakPtrs();

  multivariant_root_.reset();
  rendition_manager_.reset();
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
    std::move(cb).Run(HlsDataSourceProvider::ReadStatus::Codes::kStopped);
    return;
  }

  if (!read_chunked) {
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
  CHECK(stream);
  if (!data_source_provider_) {
    std::move(cb).Run(HlsDataSourceProvider::ReadStatus::Codes::kStopped);
    return;
  }
  data_source_provider_
      .AsyncCall(&HlsDataSourceProvider::ReadFromExistingStream)
      .WithArgs(std::move(stream),
                base::BindPostTaskToCurrentDefault(std::move(cb)));
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

void HlsManifestDemuxerEngine::UpdateRenditionManifestUri(
    std::string role,
    GURL uri,
    base::OnceClosure cb) {
  GURL uri_copy = uri;
  ReadFromUrl(
      std::move(uri_copy), false, absl::nullopt,
      base::BindOnce(&HlsManifestDemuxerEngine::UpdateMediaPlaylistForRole,
                     weak_factory_.GetWeakPtr(), role, std::move(uri),
                     std::move(cb)));
}

void HlsManifestDemuxerEngine::UpdateMediaPlaylistForRole(
    std::string role,
    GURL uri,
    base::OnceClosure cb,
    HlsDataSourceProvider::ReadResult maybe_stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!maybe_stream.has_value()) {
    Abort(std::move(maybe_stream).error().AddHere());
    std::move(cb).Run();
    return;
  }
  auto stream = std::move(maybe_stream).value();

  auto maybe_info = hls::Playlist::IdentifyPlaylist(stream->AsString());
  if (!maybe_info.has_value()) {
    Abort(std::move(maybe_info).error().AddHere());
    std::move(cb).Run();
    return;
  }

  if ((*maybe_info).kind != hls::Playlist::Kind::kMediaPlaylist) {
    Abort(HlsDemuxerStatus::Codes::kInvalidManifest);
    std::move(cb).Run();
    return;
  }

  auto maybe_playlist = ParseMediaPlaylistFromStringSource(
      stream->AsString(), std::move(uri), (*maybe_info).version);
  if (!maybe_playlist.has_value()) {
    Abort(std::move(maybe_playlist).error().AddHere());
    std::move(cb).Run();
    return;
  }

  renditions_[role]->UpdatePlaylist(std::move(maybe_playlist).value(),
                                    absl::nullopt);
  std::move(cb).Run();
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

  rendition_manager_->Reselect(
      base::BindOnce(&HlsManifestDemuxerEngine::OnRenditionsSelected,
                     weak_factory_.GetWeakPtr(), std::move(parse_complete_cb)));
}

void HlsManifestDemuxerEngine::OnRenditionsReselected(
    const hls::VariantStream* variant,
    const hls::AudioRendition* audio_override_rendition) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);

  // Flag that a pending rendition change is taking effect. If a seek aborts the
  // manifest network request, it's important that the seek should restart the
  // parsing task before attempting to update the timestamp.
  pending_adaptation_ = true;

  OnRenditionsSelected(
      base::BindOnce(&HlsManifestDemuxerEngine::OnAdaptationComplete,
                     weak_factory_.GetWeakPtr()),
      variant, audio_override_rendition);
}

void HlsManifestDemuxerEngine::OnAdaptationComplete(PipelineStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!status.is_ok()) {
    host_->OnError(std::move(status).AddHere());
    return;
  }

  pending_adaptation_ = false;
  if (pending_seek_closure_) {
    std::move(pending_seek_closure_).Run();
  }
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
  ReadFromUrl(std::move(uri), false, absl::nullopt,
              base::BindOnce(&HlsManifestDemuxerEngine::ParsePlaylist,
                             weak_factory_.GetWeakPtr(), std::move(on_complete),
                             std::move(parse_info)));
}

void HlsManifestDemuxerEngine::OnMediaPlaylist(
    PipelineStatusCallback parse_complete_cb,
    PlaylistParseInfo parse_info,
    scoped_refptr<hls::MediaPlaylist> playlist) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);

  // TODO(crbug/1266991) On stream adaptation, if the codecs are not the same,
  // we'll have to re-create the chunk demuxer role. For now, just assume the
  // codecs are the same.
  auto maybe_exists = renditions_.find(parse_info.role);
  if (maybe_exists != renditions_.end()) {
    maybe_exists->second->UpdatePlaylist(std::move(playlist), parse_info.uri);
    std::move(parse_complete_cb).Run(OkStatus());
    return;
  }

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
  CHECK(!renditions_.contains(parse_info.role));
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
  renditions_[parse_info.role] = std::move(rendition);
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
