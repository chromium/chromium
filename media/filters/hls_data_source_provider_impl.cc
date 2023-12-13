// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_data_source_provider_impl.h"

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "base/types/pass_key.h"
#include "media/formats/hls/types.h"
#include "media/base/cross_origin_data_source.h"

namespace media {

namespace {

// A small-ish size that it should probably be able to get most manifests in
// a single chunk. Chosen somewhat arbitrarily otherwise.
constexpr size_t kDefaultReadSize = 1024 * 16;

void OnMultiBufferReadComplete(
    std::unique_ptr<HlsDataSourceStream> stream,
    HlsDataSourceProviderImpl::ReadCb callback,
    int requested_read_size,
    int read_size) {
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
      stream->UnlockStreamPostWrite(read_size,
                                    requested_read_size != read_size);
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

void HlsDataSourceProviderImpl::OnDataSourceReady(
    absl::optional<hls::types::ByteRange> range,
    ReadCb callback,
    std::unique_ptr<DataSource> data_source) {
  auto stream_id = stream_id_generator_.GenerateNextId();
  auto it = data_source_map_.try_emplace(stream_id, std::move(data_source));
  // The factory may return a CrossOriginDataSource, which must be initialized.
  // Other implementations of DataSource may not even have an Initialization
  // method. In these cases, the factory should provide instances which are
  // ready to use.
  // TODO(crbug/1266991): Unify the `Initialize` method across DataSource
  // implementations, and remove the check for CrossOriginDataSource here.
  if (auto* cross_origin = it.first->second->GetAsCrossOriginDataSource()) {
    cross_origin->Initialize(base::BindPostTaskToCurrentDefault(base::BindOnce(
        &HlsDataSourceProviderImpl::DataSourceInitialized,
        weak_factory_.GetWeakPtr(), stream_id, range, std::move(callback))));
  } else {
    DataSourceInitialized(stream_id, range, std::move(callback), true);
  }
}

void HlsDataSourceProviderImpl::ReadFromUrl(
    GURL uri,
    absl::optional<hls::types::ByteRange> range,
    ReadCb callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_source_factory_->CreateDataSource(
      std::move(uri),
      base::BindOnce(&HlsDataSourceProviderImpl::OnDataSourceReady,
                     weak_factory_.GetWeakPtr(), std::move(range),
                     std::move(callback)));
}

void HlsDataSourceProviderImpl::ReadFromExistingStream(
    std::unique_ptr<HlsDataSourceStream> stream,
    ReadCb callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(stream);

  auto it = data_source_map_.find(stream->stream_id());
  if (it == data_source_map_.end()) {
    std::move(callback).Run(
        HlsDataSourceProvider::ReadStatus::Codes::kError);
    return;
  }

  if (!stream->CanReadMore()) {
    std::move(callback).Run(std::move(stream));
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
  it->second->Read(base::checked_cast<int64_t>(pos), int_read_size, buffer_data,
                   base::BindOnce(&OnMultiBufferReadComplete, std::move(stream),
                                  std::move(callback), int_read_size));
}

void HlsDataSourceProviderImpl::AbortPendingReads(base::OnceClosure cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& [id, source] : data_source_map_) {
    source->Abort();
    source->Stop();
  }
  std::move(cb).Run();
}

void HlsDataSourceProviderImpl::DataSourceInitialized(
    HlsDataSourceStream::StreamId stream_id,
    absl::optional<hls::types::ByteRange> range,
    ReadCb callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    auto it = data_source_map_.find(stream_id);
    if (it != data_source_map_.end()) {
      data_source_map_.erase(it);
    }
    std::move(callback).Run(
        HlsDataSourceProvider::ReadStatus::Codes::kAborted);
    return;
  }

  auto stream = std::make_unique<HlsDataSourceStream>(
      stream_id,
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&HlsDataSourceProviderImpl::OnStreamReleased,
                         weak_factory_.GetWeakPtr(), stream_id)),
      range);
  ReadFromExistingStream(std::move(stream), std::move(callback));
}

void HlsDataSourceProviderImpl::OnStreamReleased(
    HlsDataSourceStream::StreamId stream_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = data_source_map_.find(stream_id);
  if (it != data_source_map_.end()) {
    it->second->Abort();
    it->second->Stop();
    data_source_map_.erase(it);
  }
}

}  // namespace blink
