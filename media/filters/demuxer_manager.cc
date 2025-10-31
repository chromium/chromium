// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/demuxer_manager.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "media/base/cross_origin_data_source.h"
#include "media/base/data_source.h"
#include "media/base/media_switches.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "net/storage_access_api/status.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_HLS_DEMUXER)
#include "media/filters/hls_manifest_demuxer_engine.h"
#include "media/filters/manifest_demuxer.h"
#endif  // BUILDFLAG(ENABLE_HLS_DEMUXER)

namespace media {

namespace {

#if BUILDFLAG(ENABLE_HLS_DEMUXER)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MimeType {
  kOtherMimeType = 0,
  kApplicationDashXml = 1,
  kApplicationOgg = 2,
  kApplicationMpegUrl = 3,
  kApplicationVndAppleMpegUrl = 4,
  kApplicationXMpegUrl = 5,
  kAudioMpegUrl = 6,
  kAudioXMpegUrl = 7,
  kNonspecificAudio = 8,
  kNonspecificImage = 9,
  kNonspecificVideo = 10,
  kTextVtt = 11,
  kMaxValue = kTextVtt,  // For UMA histograms.
};

MimeType TranslateMimeTypeToHistogramEnum(std::string_view mime_type) {
  constexpr auto kCaseInsensitive = base::CompareCase::INSENSITIVE_ASCII;
  if (base::StartsWith(mime_type, "application/dash+xml", kCaseInsensitive)) {
    return MimeType::kApplicationDashXml;
  }
  if (base::StartsWith(mime_type, "application/ogg", kCaseInsensitive)) {
    return MimeType::kApplicationOgg;
  }
  if (base::StartsWith(mime_type, "application/mpegurl", kCaseInsensitive)) {
    return MimeType::kApplicationMpegUrl;
  }
  if (base::StartsWith(mime_type, "application/vnd.apple.mpegurl",
                       kCaseInsensitive)) {
    return MimeType::kApplicationVndAppleMpegUrl;
  }
  if (base::StartsWith(mime_type, "application/x-mpegurl", kCaseInsensitive)) {
    return MimeType::kApplicationXMpegUrl;
  }

  if (base::StartsWith(mime_type, "audio/mpegurl", kCaseInsensitive)) {
    return MimeType::kAudioMpegUrl;
  }
  if (base::StartsWith(mime_type, "audio/x-mpegurl", kCaseInsensitive)) {
    return MimeType::kAudioXMpegUrl;
  }

  if (base::StartsWith(mime_type, "audio/", kCaseInsensitive)) {
    return MimeType::kNonspecificAudio;
  }
  if (base::StartsWith(mime_type, "image/", kCaseInsensitive)) {
    return MimeType::kNonspecificImage;
  }
  if (base::StartsWith(mime_type, "video/", kCaseInsensitive)) {
    return MimeType::kNonspecificVideo;
  }

  if (base::StartsWith(mime_type, "text/vtt", kCaseInsensitive)) {
    return MimeType::kTextVtt;
  }

  return MimeType::kOtherMimeType;
}

#endif  // BUILDFLAG(ENABLE_HLS_DEMUXER)

#if BUILDFLAG(ENABLE_FFMPEG)
// Returns true if `url` represents (or is likely to) a local file.
bool IsLocalFile(const GURL& url) {
  return url.SchemeIsFile() || url.SchemeIsFileSystem() ||
         url.SchemeIs(url::kContentScheme) ||
         url.SchemeIs(url::kContentIDScheme) ||
         url.SchemeIs("chrome-extension");
}
#endif

}  // namespace

DemuxerManager::DemuxerManager(
    Client* client,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    MediaLog* log,
    std::unique_ptr<Demuxer> demuxer_override)
    : client_(client),
      media_task_runner_(std::move(media_task_runner)),
      media_log_(log->Clone()),
      demuxer_override_(std::move(demuxer_override)) {
  DCHECK(client_);
}

DemuxerManager::~DemuxerManager() {
  // ManifestDemuxer has multiple outstanding weak pointers bound to the media
  // thread, and needs to be deleted there.
  if (GetDemuxerType() == DemuxerType::kManifestDemuxer) {
    media_task_runner_->DeleteSoon(FROM_HERE, std::move(demuxer_));
  }
}

void DemuxerManager::InvalidateWeakPtrs() {
  weak_factory_.InvalidateWeakPtrs();
}

void DemuxerManager::RestartClientForHLS() {
  if (client_ && fallback_allowed_) {
    client_->RestartForHls();
  }
}

void DemuxerManager::OnPipelineError(PipelineStatus error) {
  DCHECK(client_);

  if (!fallback_allowed_) {
    return client_->OnError(std::move(error));
  }

#if BUILDFLAG(ENABLE_HLS_DEMUXER)
  if (base::FeatureList::IsEnabled(kBuiltInHlsPlayer) &&
      error == DEMUXER_ERROR_DETECTED_HLS) {
    hls_fallback_ = true;

    // If we've gotten a request to start HLS fallback and logging, we can
    // assert that data source has been set.
    CHECK(data_source_);

    // TODO(crbug.com/410588476): Ensure that updating the URL like this will
    // continue to respect CORS attributes for security reasons. Right now all
    // HLS content is considered CORS, but that will change.
    loaded_url_ = GetDataSourceUrlAfterRedirects().value();
    client_->UpdateLoadedUrl(loaded_url_);

    if (auto* co_data_source = data_source_->GetAsCrossOriginDataSource()) {
      MimeType mime_type =
          TranslateMimeTypeToHistogramEnum(co_data_source->GetMimeType());
      base::UmaHistogramEnumeration("Media.WebMediaPlayerImpl.HLS.MimeType",
                                    mime_type);
    }

    // The data source must be stopped after the client, after which the
    // old demuxer and data source can be freed.
    client_->StopForDemuxerReset();
    data_source_->Stop();
    FreeResourcesAfterMediaThreadWait(base::BindOnce(
        &DemuxerManager::RestartClientForHLS, weak_factory_.GetWeakPtr()));

    return;
  }
#endif  // BUILDFLAG(ENABLE_HLS_DEMUXER)

  client_->OnError(std::move(error));
}

void DemuxerManager::FreeResourcesAfterMediaThreadWait(base::OnceClosure cb) {
  // The demuxer and data source must be freed on the main thread, but we have
  // to make sure nothing is using them on the media thread first. So we have
  // to post to the media thread and back.
  data_source_info_ = nullptr;
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          [](std::unique_ptr<Demuxer> demuxer,
             std::unique_ptr<DataSource> data_source,
             base::OnceClosure done_cb) {
            demuxer.reset();
            data_source.reset();
            std::move(done_cb).Run();
          },
          std::move(demuxer_), std::move(data_source_), std::move(cb))));
}

void DemuxerManager::DisallowFallback() {
  fallback_allowed_ = false;
}

void DemuxerManager::SetLoadedUrl(GURL url) {
  // The URL might be a rather large data:// url, so move it to prevent a
  // copy.
  loaded_url_ = std::move(url);
}

const GURL& DemuxerManager::LoadedUrl() const {
  return loaded_url_;
}

std::optional<double> DemuxerManager::GetDemuxerDuration() {
  if (!demuxer_) {
    return std::nullopt;
  }
  if (demuxer_->GetDemuxerType() != DemuxerType::kChunkDemuxer) {
    return std::nullopt;
  }

  // Use duration from ChunkDemuxer when present. MSE allows users to specify
  // duration as a double. This propagates to the rest of the pipeline as a
  // TimeDelta with potentially reduced precision (limited to Microseconds).
  // ChunkDemuxer returns the full-precision user-specified double. This ensures
  // users can "get" the exact duration they "set".
  // TODO(crbug.com/40243452) Get rid of this static cast.
  return static_cast<ChunkDemuxer*>(demuxer_.get())->GetDuration();
}

std::optional<DemuxerType> DemuxerManager::GetDemuxerType() const {
  if (!demuxer_) {
    return std::nullopt;
  }
  return demuxer_->GetDemuxerType();
}

std::optional<container_names::MediaContainerName>
DemuxerManager::GetContainerForMetrics() {
  if (!demuxer_) {
    return std::nullopt;
  }
  return demuxer_->GetContainerForMetrics();
}

void DemuxerManager::RespondToDemuxerMemoryUsageReport(
    base::OnceCallback<void(int64_t)> cb) {
  if (!demuxer_) {
    return std::move(cb).Run(0);
  }
  switch (demuxer_->GetDemuxerType()) {
    case DemuxerType::kChunkDemuxer:
      // ChunkDemuxer locks while getting the memory size, so we don't have
      // to post cross thread.
      return std::move(cb).Run(demuxer_->GetMemoryUsage());
    default:
      // FFmpegDemuxer is single threaded and only runs on the media thread,
      // so we have to post there and wait for the reply. We can't be sure what
      // other demuxers do.
      // base::Unretained is safe here because |this| is posted for destruction
      // and |this| strongly owns |demuxer_|. See WMPI::ReportMemoryUsage() for
      // more information about destruction order.
      media_task_runner_->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&Demuxer::GetMemoryUsage,
                         base::Unretained(demuxer_.get())),
          std::move(cb));
      break;
  }
}

void DemuxerManager::DisableDemuxerCanChangeType() {
  demuxer_->DisableCanChangeType();
}

PipelineStatus DemuxerManager::CreateDemuxer(
    bool load_media_source,
    DataSource::Preload preload,
    bool needs_first_frame,
    DemuxerManager::DemuxerCreatedCB on_demuxer_created,
    base::flat_map<std::string, std::string> headers) {
  // TODO(crbug.com/40243452) return a better error
  if (!client_) {
    return DEMUXER_ERROR_COULD_NOT_OPEN;
  }

  // We can only do a universal suspend for posters, unless the flag is enabled.
  auto suspended_mode = Pipeline::StartType::kSuspendAfterMetadataForAudioOnly;
  if (!needs_first_frame) {
    suspended_mode = Pipeline::StartType::kSuspendAfterMetadata;
  }

#if BUILDFLAG(ENABLE_HLS_DEMUXER)
  if (hls_fallback_ || (base::FeatureList::IsEnabled(kBuiltInHlsPlayer) &&
                        loaded_url_.path().ends_with(".m3u8"))) {
    std::unique_ptr<Demuxer> demuxer;
    std::tie(data_source_info_, demuxer) = CreateHlsDemuxer();
    SetDemuxer(std::move(demuxer));
    return std::move(on_demuxer_created)
        .Run(demuxer_.get(), suspended_mode, /*is_streaming=*/false,
             /*is_static=*/false);
  }
#endif  // BUILDFLAG(ENABLE_HLS_DEMUXER)

  // TODO(sandersd): FileSystem objects may also be non-static, but due to our
  // caching layer such situations are broken already. http://crbug.com/593159
  bool is_static = true;

  if (demuxer_override_) {
    // TODO(crbug.com/40128583): Should everything else after this block
    // run in the demuxer override case?
    SetDemuxer(std::move(demuxer_override_));
  } else if (!load_media_source) {
#if BUILDFLAG(ENABLE_FFMPEG)
    SetDemuxer(CreateFFmpegDemuxer());
#else
    return DEMUXER_ERROR_PROGRESSIVE_DISABLED;
#endif
  } else {
    DCHECK(!HasDataSource());
    SetDemuxer(CreateChunkDemuxer());
    is_static = false;
  }

  if (!demuxer_) {
    return DEMUXER_ERROR_COULD_NOT_OPEN;
  }

  // A myriad of reasons exists that prevent us from entering a suspended state
  // after metadata is reached - in this case we'll have to do a normal startup.
  if (demuxer_->GetDemuxerType() == DemuxerType::kChunkDemuxer ||
      preload != DataSource::METADATA || client_->CouldPlayIfEnoughData() ||
      IsStreaming()) {
    return std::move(on_demuxer_created)
        .Run(demuxer_.get(), Pipeline::StartType::kNormal, IsStreaming(),
             is_static);
  }

  return std::move(on_demuxer_created)
      .Run(demuxer_.get(), suspended_mode, IsStreaming(), is_static);
}

DataSource* DemuxerManager::GetDataSourceForTesting() const {
  return data_source_.get();
}

void DemuxerManager::SetDataSource(std::unique_ptr<DataSource> data_source) {
  data_source_ = std::move(data_source);
  data_source_info_ = data_source_.get();
}

void DemuxerManager::StopPreloading() {
  CHECK(data_source_);
  data_source_->StopPreloading();
}

void DemuxerManager::SetPreload(DataSource::Preload preload) {
  if (data_source_) {
    data_source_->SetPreload(preload);
  }
}

void DemuxerManager::StopAndResetClient() {
  if (data_source_) {
    data_source_->Stop();
  }
  client_ = nullptr;
}

int64_t DemuxerManager::GetDataSourceMemoryUsage() {
  return data_source_info_ ? data_source_info_->GetMemoryUsage() : 0;
}

void DemuxerManager::OnDataSourcePlaybackRateChange(double rate, bool paused) {
  if (!data_source_) {
    return;
  }
  data_source_->OnMediaPlaybackRateChanged(rate);
  if (!paused) {
    data_source_->OnMediaIsPlaying();
  }
}

void DemuxerManager::DurationChanged() {
}

bool DemuxerManager::WouldTaintOrigin() const {
  if (hls_fallback_) {
    // TODO(crbug.com/410588476): return data_source_info_->WouldTaintOrigin();
    // For now, we should continue to assume that tainting is always true with
    // HLS content.
    return true;
  }
  return data_source_info_ && data_source_info_->WouldTaintOrigin();
}

bool DemuxerManager::HasDataSource() const {
  return data_source_ != nullptr;
}

bool DemuxerManager::HasDemuxer() const {
  return !!demuxer_;
}

bool DemuxerManager::HasDemuxerOverride() const {
  return !!demuxer_override_;
}

std::optional<GURL> DemuxerManager::GetDataSourceUrlAfterRedirects() const {
  if (data_source_) {
    return data_source_->GetUrlAfterRedirects();
  }
  return std::nullopt;
}

bool DemuxerManager::DataSourceFullyBuffered() const {
  return data_source_ && data_source_->AssumeFullyBuffered();
}

bool DemuxerManager::IsStreaming() const {
  return (data_source_info_ && data_source_info_->IsStreaming()) ||
         (demuxer_ && !demuxer_->IsSeekable());
}

bool DemuxerManager::IsLiveContent() const {
  // Manifest demuxer reports true live content accurately, while all other
  // demuxers do not. TODO(crbug.com/40057824): Consider making IsSeekable
  // return an enum class with vod/semi-live/true-live states.
  if (GetDemuxerType() == DemuxerType::kManifestDemuxer) {
    return !demuxer_->IsSeekable();
  }
  return false;
}

std::unique_ptr<Demuxer> DemuxerManager::CreateChunkDemuxer() {
  return std::make_unique<ChunkDemuxer>(
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &DemuxerManager::OnChunkDemuxerOpened, weak_factory_.GetWeakPtr())),
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &DemuxerManager::OnProgress, weak_factory_.GetWeakPtr())),
      base::BindPostTaskToCurrentDefault(
          base::BindRepeating(&DemuxerManager::OnEncryptedMediaInitData,
                              weak_factory_.GetWeakPtr())),
      media_log_.get());
}

#if BUILDFLAG(ENABLE_FFMPEG)
std::unique_ptr<Demuxer> DemuxerManager::CreateFFmpegDemuxer() {
  DCHECK(data_source_);
  return std::make_unique<FFmpegDemuxer>(
      media_task_runner_, data_source_.get(),
      base::BindPostTaskToCurrentDefault(
          base::BindRepeating(&DemuxerManager::OnEncryptedMediaInitData,
                              weak_factory_.GetWeakPtr())),
      base::BindPostTaskToCurrentDefault(
          base::BindRepeating(&DemuxerManager::OnFFmpegMediaTracksUpdated,
                              weak_factory_.GetWeakPtr())),
      media_log_.get(), IsLocalFile(loaded_url_));
}
#endif  // BUILDFLAG(ENABLE_FFMPEG)

#if BUILDFLAG(ENABLE_HLS_DEMUXER)
std::tuple<raw_ptr<DataSourceInfo>, std::unique_ptr<Demuxer>>
DemuxerManager::CreateHlsDemuxer() {
  bool would_taint_origin =
      data_source_info_ && data_source_info_->WouldTaintOrigin();
  auto engine = std::make_unique<HlsManifestDemuxerEngine>(
      client_->GetHlsDataSourceProvider(), media_task_runner_,
      BindPostTaskToCurrentDefault(base::BindRepeating(
          &DemuxerManager::AddTrack, weak_factory_.GetWeakPtr())),
      BindPostTaskToCurrentDefault(base::BindRepeating(
          &DemuxerManager::RemoveTrack, weak_factory_.GetWeakPtr())),
      would_taint_origin, loaded_url_, media_log_.get());

  raw_ptr<DataSourceInfo> datasource_info = engine.get();
  return std::make_tuple(
      datasource_info,
      std::make_unique<ManifestDemuxer>(
          media_task_runner_,
          base::BindPostTaskToCurrentDefault(
              base::BindRepeating(&DemuxerManager::DemuxerRequestsSeek,
                                  weak_factory_.GetWeakPtr())),
          std::move(engine), media_log_.get()));
}
#endif

void DemuxerManager::SetDemuxer(std::unique_ptr<Demuxer> demuxer) {
  DCHECK(!demuxer_);
  CHECK(demuxer);

  demuxer_ = std::move(demuxer);
  if (client_) {
    client_->MakeDemuxerThreadDumper(demuxer_.get());
  }
}

void DemuxerManager::OnEncryptedMediaInitData(
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data) {
  if (client_) {
    client_->OnEncryptedMediaInitData(init_data_type, init_data);
  }
}



void DemuxerManager::OnChunkDemuxerOpened() {
  CHECK(demuxer_);
  CHECK(demuxer_->GetDemuxerType() == DemuxerType::kChunkDemuxer);
  // TODO(crbug.com/40243452) Get rid of this static cast.
  if (client_) {
    client_->OnChunkDemuxerOpened(static_cast<ChunkDemuxer*>(demuxer_.get()));
  }
}

void DemuxerManager::OnProgress() {
  if (client_) {
    client_->OnProgress();
  }
}

#if BUILDFLAG(ENABLE_FFMPEG)
void DemuxerManager::OnFFmpegMediaTracksUpdated(
    std::unique_ptr<MediaTracks> tracks) {
  DCHECK(demuxer_);

  // For MSE/chunk_demuxer case the media track updates are handled by
  // WebSourceBufferImpl.
  DCHECK(GetDemuxerType() != DemuxerType::kChunkDemuxer);

  // we might be in the process of being destroyed when this happens.
  if (!client_) {
    return;
  }

  for (const auto& track : tracks->tracks()) {
    switch (track->type()) {
      case MediaTrack::Type::kAudio:
      case MediaTrack::Type::kVideo:
        client_->AddTrack(*track);
        break;
      default:
        // Text tracks are not supported through this code path.
        break;
    }
  }
}
#endif  // BUILDFLAG(ENABLE_FFMPEG)

#if BUILDFLAG(ENABLE_FFMPEG) || BUILDFLAG(ENABLE_HLS_DEMUXER)
void DemuxerManager::AddTrack(const MediaTrack& track) {
  client_->AddTrack(track);
}
void DemuxerManager::RemoveTrack(const MediaTrack& track) {
  client_->RemoveTrack(track);
}
#endif  // BUILDFLAG(ENABLE_FFMPEG) || BUILDFLAG(ENABLE_HLS_DEMUXER)

void DemuxerManager::DemuxerRequestsSeek(base::TimeDelta time) {
  if (!client_) {
    return;
  }
  client_->DemuxerRequestsSeek(time);
}

}  // namespace media
