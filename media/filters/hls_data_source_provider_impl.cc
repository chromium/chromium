// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_data_source_provider_impl.h"

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"
#include "base/types/pass_key.h"
#include "media/base/cross_origin_data_source.h"
#include "media/formats/hls/types.h"

namespace media {

namespace {

// A small-ish size that it should probably be able to get most manifests in
// a single chunk. Chosen somewhat arbitrarily otherwise.
constexpr size_t kDefaultReadSize = 1024 * 16;

void OnMultiBufferReadComplete(
    std::unique_ptr<HlsDataSourceStream> stream,
    HlsDataSourceProviderImpl::ReadCb callback,
    base::OnceCallback<void(HlsDataSourceStream&)> update_metadata,
    int requested_read_size,
    uint64_t trace_key,
    int read_size) {
  TRACE_EVENT_NESTABLE_ASYNC_END1("media", "HLS::ReadExistingStream", trace_key,
                                  "size", read_size);
  switch (read_size) {
    case DataSource::kReadError: {
      stream->UnlockStreamPostWrite(0, true);
      return std::move(callback).Run(
          HlsDataSourceProvider::ReadStatus::Codes::kError);
    }
    case DataSource::kAborted: {
      stream->UnlockStreamPostWrite(0, true);
      return std::move(callback).Run(
          HlsDataSourceProvider::ReadStatus::Codes::kAborted);
    }
    default: {
      CHECK_GE(read_size, 0);
      stream->UnlockStreamPostWrite(read_size, 0 == read_size);
      std::move(update_metadata).Run(*stream);
      std::move(callback).Run(std::move(stream));
    }
  }
}

}  // namespace

HlsDataSourceProviderImpl::DataSourceFactory::~DataSourceFactory() = default;

HlsDataSourceProviderImpl::HlsDataSourceProviderImpl(
    std::unique_ptr<DataSourceFactory> factory)
    : data_source_factory_(std::move(factory)) {}

HlsDataSourceProviderImpl::~HlsDataSourceProviderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& [id, source] : data_source_map_) {
    source->Stop();
  }

  // The map _must_ be cleared after aborting all sources and before the factory
  // is reset.
  data_source_map_.clear();
  data_source_factory_.reset();
}

void HlsDataSourceProviderImpl::ReadFromCombinedUrlQueue(SegmentQueue segments,
                                                         ReadCb callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!segments.empty());
  auto stream_id = stream_id_generator_.GenerateNextId();
  auto stream = std::make_unique<HlsDataSourceStream>(
      stream_id, std::move(segments),
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&HlsDataSourceProviderImpl::OnStreamReleased,
                         weak_factory_.GetWeakPtr(), stream_id)));
  ReadFromExistingStream(std::move(stream), std::move(callback));
}

void HlsDataSourceProviderImpl::UpdateStreamMetadata(
    HlsDataSourceStream::StreamId stream_id,
    HlsDataSourceStream& stream) {
  uint64_t usage = 0;
  for (const auto& it : data_source_map_) {
    usage += it.second->GetMemoryUsage();
    would_taint_origin_ |= it.second->WouldTaintOrigin();
  }
  stream.set_total_memory_usage(usage);
  if (would_taint_origin_) {
    stream.set_would_taint_origin();
  }
}

void HlsDataSourceProviderImpl::ReadFromExistingStream(
    std::unique_ptr<HlsDataSourceStream> stream,
    ReadCb callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(stream);
  // There might be no data source attached to the stream yet, so we should
  // try to make one. Creating a new data source will re-enter this function to
  // complete `callback`.
  if (stream->RequiresNextDataSource()) {
    auto [new_uri, bypass_cache] = stream->GetNextSegmentURIAndCacheStatus();
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("media", "HLS::CreateDataSource", this,
                                      "uri", new_uri);
    data_source_factory_->CreateDataSource(
        std::move(new_uri), bypass_cache,
        base::BindOnce(&HlsDataSourceProviderImpl::OnDataSourceCreated,
                       weak_factory_.GetWeakPtr(), std::move(stream),
                       std::move(callback)));
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("media", "HLS::ReadExistingStream", this);
  // A finished stream may have removed any attached data source, so it might
  // not be present in the map.
  if (!stream->CanReadMore()) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("media", "HLS::ReadExistingStream", this);
    std::move(callback).Run(std::move(stream));
    return;
  }

  // Any stream which can read more _must_ have an active data source attached.
  auto it = data_source_map_.find(stream->stream_id());
  if (it == data_source_map_.end()) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("media", "HLS::ReadExistingStream", this);
    std::move(callback).Run(ReadStatus::Codes::kError);
    return;
  }

  size_t read_size = kDefaultReadSize;
  auto pos = stream->read_position();
  auto max_position = stream->max_read_position();
  if (max_position.has_value()) {
    // If the read head is greater than max, `CanReadMore` should have returned
    // false.
    CHECK_GT(max_position.value(), pos);
    read_size = std::min(read_size, max_position.value() - pos);
  }

  auto int_read_size = base::checked_cast<int>(read_size);
  auto* buffer_data = stream->LockStreamForWriting(int_read_size);
  auto stream_id = stream->stream_id();
  uint64_t async_event_key = reinterpret_cast<std::uintptr_t>(this);

  it->second->Read(
      base::checked_cast<int64_t>(pos), int_read_size, buffer_data,
      base::BindOnce(
          &OnMultiBufferReadComplete, std::move(stream), std::move(callback),
          base::BindOnce(&HlsDataSourceProviderImpl::UpdateStreamMetadata,
                         weak_factory_.GetWeakPtr(), stream_id),
          int_read_size, async_event_key));
}

void HlsDataSourceProviderImpl::OnDataSourceCreated(
    std::unique_ptr<HlsDataSourceStream> stream,
    ReadCb callback,
    std::unique_ptr<DataSource> data_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto stream_id = stream->stream_id();
  auto old_data_source = data_source_map_.find(stream_id);
  if (old_data_source != data_source_map_.end()) {
    old_data_source->second->Stop();
    data_source_map_.erase(old_data_source);
  }
  would_taint_origin_ |= data_source->WouldTaintOrigin();
  auto pair = data_source_map_.try_emplace(stream_id, std::move(data_source));
  // Cross origin data sources have an asynchronous initialize method which
  // must be called after they're put into `data_source_map_`. Other types of
  // data source (including the test framework ones) come from the factory ready
  // to go, and don't have an async init process.
  if (auto* cross_origin = pair.first->second->GetAsCrossOriginDataSource()) {
    cross_origin->Initialize(base::BindPostTaskToCurrentDefault(base::BindOnce(
        &HlsDataSourceProviderImpl::DataSourceInitialized,
        weak_factory_.GetWeakPtr(), std::move(stream), std::move(callback))));
  } else {
    DataSourceInitialized(std::move(stream), std::move(callback), true);
  }
}

void HlsDataSourceProviderImpl::DataSourceInitialized(
    std::unique_ptr<HlsDataSourceStream> stream,
    ReadCb callback,
    bool success) {
  if (!success) {
    auto it = data_source_map_.find(stream->stream_id());
    CHECK(it != data_source_map_.end());
    data_source_map_.erase(it);
    std::move(callback).Run(ReadStatus::Codes::kStopped);
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0("media", "HLS::CreateDataSource", this);
  ReadFromExistingStream(std::move(stream), std::move(callback));
}

void HlsDataSourceProviderImpl::AbortPendingReads(base::OnceClosure cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& [id, source] : data_source_map_) {
    source->Abort();
    source->Stop();
  }
  std::move(cb).Run();
}

void HlsDataSourceProviderImpl::OnStreamReleased(
    HlsDataSourceStream::StreamId stream_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = data_source_map_.find(stream_id);
  if (it != data_source_map_.end()) {
    it->second->Stop();
    data_source_map_.erase(it);
  }
}

}  // namespace blink
