// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_MANIFEST_DEMUXER_ENGINE_H_
#define MEDIA_FILTERS_HLS_MANIFEST_DEMUXER_ENGINE_H_

#include <vector>

#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "media/filters/hls_data_source_provider.h"
#include "media/filters/manifest_demuxer.h"

namespace media {

// A HLS-Parser/Player implementation of ManifestDemuxer's Engine interface.
// This will use the HLS parsers and rendition selectors to fetch and parse
// playlists, followed by fetching and appending media segments.
class MEDIA_EXPORT HlsManifestDemuxerEngine : public ManifestDemuxer::Engine {
 public:
  HlsManifestDemuxerEngine();

  // ManifestDemuxer::Engine implementation.
  ~HlsManifestDemuxerEngine() override;
  std::string GetName() const override;
  void Initialize(ManifestDemuxerEngineHost* host,
                  PipelineStatusCallback status_cb) override;
  void OnTimeUpdate(base::TimeDelta time,
                    double playback_rate,
                    ManifestDemuxer::DelayCallback cb) override;
  bool Seek(base::TimeDelta time) override;
  void StartWaitingForSeek() override;
  void AbortPendingReads() override;
  bool IsSeekable() override;
  int64_t GetMemoryUsage() const override;
  void Stop() override;
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_MANIFEST_DEMUXER_ENGINE_H_
