// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_HLS_DATA_SOURCE_PROVIDER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_HLS_DATA_SOURCE_PROVIDER_IMPL_H_

#include <deque>
#include <memory>
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/types/pass_key.h"
#include "media/base/data_source.h"
#include "media/base/media_log.h"
#include "media/filters/hls_data_source_provider.h"
#include "third_party/blink/public/platform/media/url_index.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class BufferedDataSourceHostImpl;

class PLATFORM_EXPORT HlsDataSourceProviderImpl
    : public media::HlsDataSourceProvider {
 public:
  // An instance of DataSourceFactory allows separation of DataSource creation
  // and DataSourceStream buffer management for easier testing.
  class DataSourceFactory {
   public:
    using DataSourceCb =
        base::OnceCallback<void(std::unique_ptr<media::CrossOriginDataSource>)>;
    virtual ~DataSourceFactory() = 0;
    virtual void CreateDataSource(GURL uri, DataSourceCb cb) = 0;
  };

  ~HlsDataSourceProviderImpl() override;
  explicit HlsDataSourceProviderImpl(
      std::unique_ptr<DataSourceFactory> factory);

  // media::HlsDataSourceProvider implementation
  void ReadFromUrl(GURL uri,
                   absl::optional<media::hls::types::ByteRange> range,
                   ReadCb callback) override;
  void ReadFromExistingStream(
      std::unique_ptr<media::HlsDataSourceStream> stream,
      ReadCb callback) override;
  void AbortPendingReads(base::OnceClosure cb) override;

 private:
  void OnDataSourceReady(
      absl::optional<media::hls::types::ByteRange> range,
      ReadCb callback,
      std::unique_ptr<media::CrossOriginDataSource> data_source);
  void OnStreamReleased(media::HlsDataSourceStream::StreamId stream_id);
  void DataSourceInitialized(media::HlsDataSourceStream::StreamId stream_id,
                             absl::optional<media::hls::types::ByteRange> range,
                             ReadCb callback,
                             bool success);

  std::unique_ptr<DataSourceFactory> data_source_factory_;

  media::HlsDataSourceStream::StreamId::Generator stream_id_generator_;

  base::flat_map<media::HlsDataSourceStream::StreamId,
                 std::unique_ptr<media::CrossOriginDataSource>>
      data_source_map_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<HlsDataSourceProviderImpl> weak_factory_{this};
};

class PLATFORM_EXPORT MultiBufferDataSourceFactory
    : public HlsDataSourceProviderImpl::DataSourceFactory {
 public:
  using UrlDataCb = base::RepeatingCallback<
      void(const GURL& url, base::OnceCallback<void(scoped_refptr<UrlData>)>)>;

  ~MultiBufferDataSourceFactory() override;
  MultiBufferDataSourceFactory(
      media::MediaLog* media_log,
      UrlDataCb get_url_data,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      const base::TickClock* tick_clock);

  void CreateDataSource(GURL uri, DataSourceCb cb) override;

 private:
  void OnUrlData(DataSourceCb cb,
                 base::RepeatingCallback<void(bool)> download_cb,
                 scoped_refptr<UrlData> data);

  std::unique_ptr<media::MediaLog> media_log_;
  UrlDataCb get_url_data_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  std::unique_ptr<BufferedDataSourceHostImpl> buffered_data_source_host_;
  base::WeakPtrFactory<MultiBufferDataSourceFactory> weak_factory_{this};
};

}  // namespace blink

#endif
