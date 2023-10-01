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
#include "media/base/media_log.h"
#include "media/filters/hls_data_source_provider.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class UrlIndex;
class BufferedDataSourceHostImpl;
class MultiBufferDataSource;
class HlsDataSourceImpl;

class PLATFORM_EXPORT HlsDataSourceProviderImpl
    : public media::HlsDataSourceProvider {
  using Self = HlsDataSourceProviderImpl;

 public:
  ~HlsDataSourceProviderImpl() override;
  HlsDataSourceProviderImpl(
      media::MediaLog* media_log,
      UrlIndex* url_index,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SequencedTaskRunner> media_task_runner,
      const base::TickClock* tick_clock);

  void RequestMockDataSourceForTesting(
      std::unique_ptr<MultiBufferDataSource> mock_ds,
      RequestCb callback) {
    RequestDataSourceInternal(std::move(mock_ds), std::move(callback));
  }

  // `media::DataSourceProvider` implementation
  void RequestDataSource(GURL uri,
                         absl::optional<media::hls::types::ByteRange> range,
                         RequestCb callback) override;

  // Returns the set of currently active data sources created by this provider.
  // This must be called on the main thread.
  const std::deque<MultiBufferDataSource*>& GetActiveDataSources();

  // Called by the destructor of `HlsDataSourceImpl`, both to notify this of its
  // destruction and to ensure destruction of the underlying
  // `MultiBufferDataSource` on the main thread.
  void NotifyDataSourceDestroyed(base::PassKey<HlsDataSourceImpl>,
                                 std::unique_ptr<MultiBufferDataSource>);

 private:
  void RequestDataSourceInternal(
      std::unique_ptr<MultiBufferDataSource> data_source,
      RequestCb callback);

  void NotifyDataSourceProgress();

  void NotifyDownloading(const std::string& uri, bool is_downloading);

  void DataSourceInitialized(std::unique_ptr<MultiBufferDataSource> data_source,
                             absl::optional<media::hls::types::ByteRange> range,
                             RequestCb callback,
                             bool success);

  std::unique_ptr<media::MediaLog> media_log_;
  raw_ptr<UrlIndex, ExperimentalRenderer> url_index_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  std::unique_ptr<BufferedDataSourceHostImpl> buffered_data_source_host_;

  // Set of all active `MultiBufferDataSource`s belonging to the
  // `HlsDataSourceImpl`s created by this provider. This uses a
  // `MultiBufferDataSource*` (as opposed to `HlsDataSourceImpl*`) because those
  // are both created and destroyed on the main thread, unlike
  // `HlsDataSourceImpl` which is created on the main thread and destroyed on
  // the media thread.
  // Using a `std::deque` instead of a set-like data structure for this since
  // size will be low, and primary operations are guaranteed-unique (and mostly
  // FIFO) insertion and removal rather than existence-checking.
  std::deque<MultiBufferDataSource*> active_data_sources_;

  base::WeakPtrFactory<Self> weak_factory_{this};
};

}  // namespace blink

#endif
