// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_DATA_SOURCE_PROVIDER_IMPL_H_
#define MEDIA_FILTERS_HLS_DATA_SOURCE_PROVIDER_IMPL_H_

#include <memory>
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/types/pass_key.h"
#include "media/base/data_source.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/filters/hls_data_source_provider.h"

namespace media {

class MEDIA_EXPORT HlsDataSourceProviderImpl : public HlsDataSourceProvider {
 public:
  // An instance of DataSourceFactory allows separation of DataSource creation
  // and DataSourceStream buffer management for easier testing.
  class DataSourceFactory {
   public:
    using DataSourceCb = base::OnceCallback<void(std::unique_ptr<DataSource>)>;
    virtual ~DataSourceFactory() = 0;
    virtual void CreateDataSource(GURL uri,
                                  bool ignore_cache,
                                  DataSourceCb cb) = 0;
  };

  ~HlsDataSourceProviderImpl() override;
  explicit HlsDataSourceProviderImpl(
      std::unique_ptr<DataSourceFactory> factory);

  // HlsDataSourceProvider implementation
  void ReadFromCombinedUrlQueue(SegmentQueue segments,
                                ReadCb callback) override;

  void ReadFromExistingStream(std::unique_ptr<HlsDataSourceStream> stream,
                              ReadCb callback) override;
  void AbortPendingReads(base::OnceClosure cb) override;

 private:
  void UpdateStreamMetadata(HlsDataSourceStream::StreamId,
                            HlsDataSourceStream& stream);
  void OnDataSourceCreated(std::unique_ptr<HlsDataSourceStream> stream,
                           ReadCb callback,
                           std::unique_ptr<DataSource> data_source);
  void OnStreamReleased(HlsDataSourceStream::StreamId stream_id);
  void DataSourceInitialized(std::unique_ptr<HlsDataSourceStream> stream,
                             ReadCb callback,
                             bool success);

  std::unique_ptr<DataSourceFactory> data_source_factory_;

  HlsDataSourceStream::StreamId::Generator stream_id_generator_;

  base::flat_map<HlsDataSourceStream::StreamId, std::unique_ptr<DataSource>>
      data_source_map_;

  bool would_taint_origin_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<HlsDataSourceProviderImpl> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_DATA_SOURCE_PROVIDER_IMPL_H_
