// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DEMUXER_MANAGER_H_
#define MEDIA_FILTERS_DEMUXER_MANAGER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/data_source.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace media {

// This class manages both an implementation of media::Demuxer and of
// media::DataSource. DataSource, in particular may be null, since both MSE
// playback and Android's MediaPlayerRenderer do not make use of it. In the
// case that DataSource is present, these objects should have a similar
// lifetime, and both must be destroyed on the media thread, so owning them
// together makes sense. Additionally, the demuxer or data source can change
// during the lifetime of the player that owns them, so encapsulating that
// change logic separately lets the media player impl (WMPI) be a bit simpler,
// and dedicate a higher percentage of its complexity to managing playback
// state.
class MEDIA_EXPORT DemuxerManager {
 public:
  class Client {
    // TODO(crbug/1377053): To be implemented in a future CL.
  };

  explicit DemuxerManager(Client* client);
  ~DemuxerManager();

  void SetLoadedUrl(GURL url);

  // Methods that help manage or access |data_source_|
  const DataSource* GetDataSourceForTesting() const;
  void SetDataSource(std::unique_ptr<DataSource> data_source);
  void OnBufferingHaveEnough(bool enough);
  void SetPreload(DataSource::Preload preload);
  std::unique_ptr<DataSource> StopAndGetDataSourceForDestruction();
  int64_t GetDataSourceMemoryUsage();
  void OnDataSourcePlaybackRateChange(double rate, bool paused);
#if BUILDFLAG(IS_ANDROID)
  PipelineStatus StartAndRecordHLSFallback(bool is_frame_url_cryptographic);
#endif

  // This will go away as soon as we move the creation of the demuxers into
  // this manager file, in the next patchset.
  DataSource* GetRawDataSource() const;

  bool WouldTaintOrigin() const;
  bool HasDataSource() const;
  absl::optional<GURL> GetDataSourceUrlAfterRedirects() const;
  bool DataSourceFullyBuffered() const;
  bool IsStreamingDataSource() const;
  bool PassedDataSourceTimingAllowOriginCheck() const;

 private:
  // This is usually just the WebMediaPlayerImpl.
  raw_ptr<Client> client_;

  // Used for MediaUrlDemuxer when playing HLS content, as well as FFmpegDemuxer
  // in most cases. Also used for creating MemoryDataSource objects.
  GURL loaded_url_;

  // The data source for creating a demuxer. This should be null when using
  // ChunkDemuxer.
  std::unique_ptr<DataSource> data_source_;

  // Weak pointer implementation.
  base::WeakPtrFactory<DemuxerManager> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_DEMUXER_MANAGER_H_
