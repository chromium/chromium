// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/demuxer_manager.h"

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "media/base/cross_origin_data_source.h"
#include "media/base/data_source.h"
#include "media/base/media_switches.h"
#include "url/gurl.h"

namespace media {

namespace {

#if BUILDFLAG(IS_ANDROID)

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

MimeType TranslateMimeTypeToHistogramEnum(const base::StringPiece& mime_type) {
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

#endif

}  // namespace

DemuxerManager::DemuxerManager(Client* client) : client_(client) {
  DCHECK(client_);
}

DemuxerManager::~DemuxerManager() {
  // |data_source_| MUST have been moved out of DemuxerManager prior to it
  // being deleted!
  DCHECK(!data_source_);
}

// This will go away as soon as we move the creation of the demuxers into
// this manager file, in the next patchset.
DataSource* DemuxerManager::GetRawDataSource() const {
  return data_source_.get();
}

void DemuxerManager::SetLoadedUrl(GURL url) {
  // The URL might be a rather large data:// url, so move it to prevent a
  // copy.
  loaded_url_ = std::move(url);
}

const DataSource* DemuxerManager::GetDataSourceForTesting() const {
  return data_source_.get();
}

void DemuxerManager::SetDataSource(std::unique_ptr<DataSource> data_source) {
  data_source_ = std::move(data_source);
}

void DemuxerManager::OnBufferingHaveEnough(bool enough) {
  CHECK(data_source_);
  data_source_->OnBufferingHaveEnough(enough);
}

void DemuxerManager::SetPreload(DataSource::Preload preload) {
  if (data_source_) {
    data_source_->SetPreload(preload);
  }
}

std::unique_ptr<DataSource>
DemuxerManager::StopAndGetDataSourceForDestruction() {
  if (data_source_) {
    data_source_->Stop();
  }
  return std::move(data_source_);
}

int64_t DemuxerManager::GetDataSourceMemoryUsage() {
  return data_source_ ? data_source_->GetMemoryUsage() : 0;
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

#if BUILDFLAG(IS_ANDROID)
PipelineStatus DemuxerManager::StartAndRecordHLSFallback(
    bool is_frame_url_cryptographic) {
  // If HLS isn't enabled, HLS detection should be the error.
  if (!base::FeatureList::IsEnabled(kHlsPlayer)) {
    return DEMUXER_ERROR_DETECTED_HLS;
  }

  // If we've gotten a request to start HLS fallback and logging, we can assert
  // that data source has been set.
  CHECK(data_source_);

  // |data_source_| might be a MemoryDataSource if our URL is a data:// url.
  // Since MediaPlayer doesn't support this type of URL, we can't fall back to
  // android's HLS implementation. Since HLS is enabled, we should report a
  // failed external renderer, since we know MediaPlayerRenderer would fail
  // anyway here.
  // TODO(crbug/1266991): Consider allowing data:// URLs for the native HLS
  // implementation.
  const auto* co_data_source = data_source_->GetAsCrossOriginDataSource();
  if (!co_data_source) {
    return PIPELINE_ERROR_EXTERNAL_RENDERER_FAILED;
  }

  bool manifest_url_is_cryptographic =
      loaded_url_.SchemeIsCryptographic() &&
      GetDataSourceUrlAfterRedirects()->SchemeIsCryptographic();
  MimeType mime_type =
      TranslateMimeTypeToHistogramEnum(co_data_source->GetMimeType());
  bool is_cross_origin = co_data_source->IsCorsCrossOrigin();
  base::UmaHistogramEnumeration("Media.WebMediaPlayerImpl.HLS.MimeType",
                                mime_type);
  UMA_HISTOGRAM_BOOLEAN("Media.WebMediaPlayerImpl.HLS.IsCorsCrossOrigin",
                        is_cross_origin);
  UMA_HISTOGRAM_BOOLEAN(
      "Media.WebMediaPlayerImpl.HLS.IsMixedContent",
      is_frame_url_cryptographic && !manifest_url_is_cryptographic);
  if (is_cross_origin) {
    UMA_HISTOGRAM_BOOLEAN("Media.WebMediaPlayerImpl.HLS.HasAccessControl",
                          co_data_source->HasAccessControl());
    base::UmaHistogramEnumeration(
        "Media.WebMediaPlayerImpl.HLS.CorsCrossOrigin.MimeType", mime_type);
  }

  return PIPELINE_OK;
}
#endif

bool DemuxerManager::WouldTaintOrigin() const {
  // TODO(crbug/1377053): The default |false| value might have to be
  // re-considered for MediaPlayerRenderer, but for now, leave behavior the
  // same as it was.
  return data_source_ ? data_source_->WouldTaintOrigin() : false;
}

bool DemuxerManager::HasDataSource() const {
  return data_source_ != nullptr;
}

absl::optional<GURL> DemuxerManager::GetDataSourceUrlAfterRedirects() const {
  if (data_source_) {
    return data_source_->GetUrlAfterRedirects();
  }
  return absl::nullopt;
}

bool DemuxerManager::DataSourceFullyBuffered() const {
  return data_source_ && data_source_->AssumeFullyBuffered();
}

bool DemuxerManager::IsStreamingDataSource() const {
  return data_source_ && data_source_->IsStreaming();
}

bool DemuxerManager::PassedDataSourceTimingAllowOriginCheck() const {
  // If there is no MultiBuffer, then there are no HTTP responses, and so this
  // can safely return true. Specifically for the MSE case, the app itself
  // sources the ArrayBuffer[Views], possibly not even from HTTP responses. Any
  // TAO checks which are present to prevent deduction of the resource content
  // can be assumed to have passed, as the content is already readable by the
  // app. TAO checks which would be used to determine other network timing
  // info, such as DNS lookup time, are not relevant as the media data is far
  // removed from the network itself at this point, and so that info cannot be
  // revealed via the MediaSource or WebMediaPlayer that's using MSE.
  // TODO(1266991): Ensure that this returns the correct value for HLS media,
  // based on the TAO checks performed on those resources.
  return data_source_ ? data_source_->PassedTimingAllowOriginCheck() : true;
}

}  // namespace media
