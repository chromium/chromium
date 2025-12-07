// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_network_access_impl.h"

#include "base/task/bind_post_task.h"

namespace media {

HlsNetworkAccessImpl::~HlsNetworkAccessImpl() = default;

HlsNetworkAccessImpl::HlsNetworkAccessImpl(
    base::SequenceBound<HlsDataSourceProvider> dsp)
    : data_source_provider_(std::move(dsp)) {
  // This is always created on the main sequence, but used on the media sequence
  DETACH_FROM_SEQUENCE(media_sequence_checker_);
}

void HlsNetworkAccessImpl::ReadSegmentQueueInternal(
    HlsDataSourceProvider::SegmentQueue media_segment_url_queue,
    HlsDataSourceProvider::ReadCb cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  // Callers of `ReadSegmentQueueInternal` should enforce this.
  CHECK(data_source_provider_);

  data_source_provider_
      .AsyncCall(&HlsDataSourceProvider::ReadFromCombinedUrlQueue)
      .WithArgs(std::move(media_segment_url_queue),
                base::BindPostTaskToCurrentDefault(std::move(cb)));
}

void HlsNetworkAccessImpl::ReadAllInternal(const GURL& uri,
                                           HlsDataSourceProvider::ReadCb cb,
                                           bool bypass_cache) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  // Callers of `ReadAllInternal` should enforce this.
  CHECK(data_source_provider_);
  HlsDataSourceProvider::SegmentQueue queue;
  queue.emplace(uri, std::nullopt, bypass_cache);
  ReadSegmentQueueInternal(
      std::move(queue),
      base::BindOnce(&HlsNetworkAccessImpl::ReadUntilExhausted,
                     weak_factory_.GetWeakPtr(), std::move(cb)));
}

void HlsNetworkAccessImpl::OnKeyFetch(
    scoped_refptr<hls::MediaSegment::EncryptionData> enc_data,
    base::OnceCallback<void(HlsDataSourceProvider::ReadCb)> next_op,
    HlsDataSourceProvider::ReadCb cb,
    HlsDataSourceProvider::ReadResult result) {
  if (!result.has_value()) {
    std::move(cb).Run(std::move(result).error().AddHere());
    return;
  }
  auto stream = std::move(result).value();
  enc_data->ImportKey(stream->AsString());
  if (enc_data->NeedsKeyFetch()) {
    std::move(cb).Run({HlsDataSourceProvider::ReadStatus::Codes::kError,
                       "Error importing key in encrypted segment fetch"});
    return;
  }
  std::move(next_op).Run(std::move(cb));
}

void HlsNetworkAccessImpl::ReadManifest(const GURL& uri,
                                        HlsDataSourceProvider::ReadCb cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!data_source_provider_) {
    std::move(cb).Run(HlsDataSourceProvider::ReadStatus::Codes::kStopped);
    return;
  }
  ReadAllInternal(uri, std::move(cb), /*bypass_cache=*/true);
}

void HlsNetworkAccessImpl::ReadKey(
    const hls::MediaSegment::EncryptionData& data,
    HlsDataSourceProvider::ReadCb cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!data_source_provider_) {
    std::move(cb).Run(HlsDataSourceProvider::ReadStatus::Codes::kStopped);
    return;
  }
  ReadAllInternal(data.GetUri(), std::move(cb));
}

void HlsNetworkAccessImpl::ReadMediaSegment(const hls::MediaSegment& segment,
                                            bool read_chunked,
                                            bool include_init,
                                            HlsDataSourceProvider::ReadCb cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!data_source_provider_) {
    std::move(cb).Run(HlsDataSourceProvider::ReadStatus::Codes::kStopped);
    return;
  }

  if (!read_chunked) {
    cb = base::BindOnce(&HlsNetworkAccessImpl::ReadUntilExhausted,
                        weak_factory_.GetWeakPtr(), std::move(cb));
  }

  HlsDataSourceProvider::SegmentQueue queue;
  if (include_init) {
    if (auto init = segment.GetInitializationSegment()) {
      queue.emplace(init->GetUri(), init->GetByteRange(), false);
    }
  }
  queue.emplace(segment.GetUri(), segment.GetByteRange(), false);

  if (auto enc_data = segment.GetEncryptionData()) {
    if (enc_data->NeedsKeyFetch()) {
      ReadKey(
          *enc_data,
          base::BindOnce(
              &HlsNetworkAccessImpl::OnKeyFetch, weak_factory_.GetWeakPtr(),
              enc_data,
              base::BindOnce(&HlsNetworkAccessImpl::ReadSegmentQueueInternal,
                             weak_factory_.GetWeakPtr(), std::move(queue)),
              std::move(cb)));
      return;
    }
  }

  ReadSegmentQueueInternal(std::move(queue), std::move(cb));
}

void HlsNetworkAccessImpl::ReadStream(
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

void HlsNetworkAccessImpl::AbortPendingReads(base::OnceClosure cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  data_source_provider_.AsyncCall(&HlsDataSourceProvider::AbortPendingReads)
      .WithArgs(std::move(cb));
}

void HlsNetworkAccessImpl::ReadUntilExhausted(
    HlsDataSourceProvider::ReadCb cb,
    HlsDataSourceProvider::ReadResult result) {
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
             base::BindOnce(&HlsNetworkAccessImpl::ReadUntilExhausted,
                            weak_factory_.GetWeakPtr(), std::move(cb)));
}

}  // namespace media
