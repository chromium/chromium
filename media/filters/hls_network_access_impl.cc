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

void HlsNetworkAccessImpl::ReadManifest(const GURL& uri,
                                        HlsDataSourceProvider::ReadCb cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!data_source_provider_) {
    std::move(cb).Run(HlsDataSourceProvider::ReadStatus::Codes::kStopped);
    return;
  }
  HlsDataSourceProvider::UrlDataSegment segment = {std::move(uri),
                                                   std::nullopt};
  data_source_provider_.AsyncCall(&HlsDataSourceProvider::ReadFromUrl)
      .WithArgs(std::move(segment),
                base::BindPostTaskToCurrentDefault(
                    base::BindOnce(&HlsNetworkAccessImpl::ReadUntilExhausted,
                                   weak_factory_.GetWeakPtr(), std::move(cb))));
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

  // Optionally attach the init segment
  HlsDataSourceProvider::SegmentQueue queue;
  if (include_init) {
    if (auto init = segment.GetInitializationSegment()) {
      queue.emplace(init->GetUri(), init->GetByteRange());
    }
  }
  queue.emplace(segment.GetUri(), segment.GetByteRange());

  if (!read_chunked) {
    cb = base::BindOnce(&HlsNetworkAccessImpl::ReadUntilExhausted,
                        weak_factory_.GetWeakPtr(), std::move(cb));
  }

  data_source_provider_
      .AsyncCall(&HlsDataSourceProvider::ReadFromCombinedUrlQueue)
      .WithArgs(std::move(queue),
                base::BindPostTaskToCurrentDefault(std::move(cb)));
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
