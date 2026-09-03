// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_network_access_impl.h"

#include "base/task/bind_post_task.h"

namespace media {

class HlsNetworkAccessImpl::ParallelFetchState
    : public base::RefCountedThreadSafe<ParallelFetchState> {
 public:
  ParallelFetchState(base::WeakPtr<HlsNetworkAccessImpl> network_access,
                     url::Origin manifest_origin,
                     scoped_refptr<hls::MediaSegment::EncryptionData> enc_data,
                     HlsDataSourceProvider::ReadCb cb)
      : network_access_(std::move(network_access)),
        manifest_origin_(std::move(manifest_origin)),
        enc_data_(std::move(enc_data)),
        completion_cb_(std::move(cb)) {}

  void Start(std::optional<GURL> key_uri,
             std::optional<HlsDataSourceProvider::UrlDataSegment> init_segment,
             HlsDataSourceProvider::UrlDataSegment media_segment,
             bool read_chunked) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (key_uri) {
      key_pending_ = true;
      if (network_access_) {
        network_access_->ReadAllInternal(
            *key_uri, base::BindOnce(&ParallelFetchState::OnKeyLoaded, this));
      }
    }

    if (init_segment) {
      init_pending_ = true;
      if (network_access_) {
        auto cb = base::BindOnce(&ParallelFetchState::OnInitLoaded, this);
        cb = base::BindOnce(&HlsNetworkAccessImpl::ReadUntilExhaustedHelper,
                            network_access_, std::move(cb));

        network_access_->ReadSegmentInternal(*std::move(init_segment),
                                             std::move(cb));
      }
    }

    segment_pending_ = true;
    if (network_access_) {
      auto cb = base::BindOnce(&ParallelFetchState::OnSegmentLoaded, this);
      if (!read_chunked) {
        cb = base::BindOnce(&HlsNetworkAccessImpl::ReadUntilExhaustedHelper,
                            network_access_, std::move(cb));
      }
      network_access_->ReadSegmentInternal(media_segment, std::move(cb));
    }
  }

 private:
  friend class base::RefCountedThreadSafe<ParallelFetchState>;
  ~ParallelFetchState() = default;

  void OnKeyLoaded(HlsDataSourceProvider::ReadResult result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (aborted_) {
      return;
    }
    key_pending_ = false;
    key_result_ = std::move(result);
    if (!key_result_->has_value()) {
      OnError(std::move(*key_result_).error());
      return;
    }
    CheckCompleted();
  }

  void OnInitLoaded(HlsDataSourceProvider::ReadResult result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (aborted_) {
      return;
    }
    init_pending_ = false;
    init_result_ = std::move(result);
    if (!init_result_->has_value()) {
      OnError(std::move(*init_result_).error());
      return;
    }
    CheckCompleted();
  }

  void OnSegmentLoaded(HlsDataSourceProvider::ReadResult result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (aborted_) {
      return;
    }
    segment_pending_ = false;
    segment_result_ = std::move(result);
    if (!segment_result_->has_value()) {
      OnError(std::move(*segment_result_).error());
      return;
    }
    CheckCompleted();
  }

  void OnError(HlsDemuxerStatus status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (aborted_) {
      DCHECK(!completion_cb_);
      return;
    }
    aborted_ = true;
    if (completion_cb_) {
      std::move(completion_cb_).Run(std::move(status));
    }
  }

  void CheckCompleted() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (aborted_) {
      return;
    }
    if (key_pending_ || init_pending_ || segment_pending_) {
      return;
    }
    CHECK(completion_cb_);

    if (!network_access_) {
      std::move(completion_cb_)
          .Run(HlsDemuxerStatus::Codes::kNetworkReadAborted);
      aborted_ = true;
      return;
    }

    if (enc_data_ && key_result_.has_value()) {
      auto key_stream = std::move(*key_result_).value();
      enc_data_->ImportKey(key_stream->AsString());
      enc_data_->ImportKeySecurity(key_stream->SecurityInfo());
      if (enc_data_->NeedsKeyFetch()) {
        OnError({HlsDemuxerStatus::Codes::kNetworkReadError,
                 "Error importing key in encrypted segment fetch"});
        return;
      }
    }

    auto segment_stream = std::move(*segment_result_).value();
    if (init_result_.has_value()) {
      auto init_stream = std::move(*init_result_).value();
      segment_stream->PrependInitStream(std::move(init_stream));
    }

    if (enc_data_) {
      const auto& encryption_metadata = enc_data_->GetSecurityMetadata();
      if (encryption_metadata.has_value()) {
        segment_stream->MergeSecurityMetadata(*encryption_metadata);
      }
    }

    network_access_->MediaSegmentSecurityChecks(
        std::move(completion_cb_), manifest_origin_, std::move(segment_stream));
  }

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<HlsNetworkAccessImpl> network_access_
      GUARDED_BY_CONTEXT(sequence_checker_);
  const url::Origin manifest_origin_ GUARDED_BY_CONTEXT(sequence_checker_);
  const scoped_refptr<hls::MediaSegment::EncryptionData> enc_data_
      GUARDED_BY_CONTEXT(sequence_checker_);
  HlsDataSourceProvider::ReadCb completion_cb_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::optional<HlsDataSourceProvider::ReadResult> key_result_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::optional<HlsDataSourceProvider::ReadResult> init_result_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::optional<HlsDataSourceProvider::ReadResult> segment_result_
      GUARDED_BY_CONTEXT(sequence_checker_);

  bool key_pending_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool init_pending_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool segment_pending_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool aborted_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
};

HlsNetworkAccessImpl::~HlsNetworkAccessImpl() = default;

HlsNetworkAccessImpl::HlsNetworkAccessImpl(
    base::SequenceBound<HlsDataSourceProvider> dsp)
    : data_source_provider_(std::move(dsp)) {
  // This is always created on the main sequence, but used on the media sequence
  DETACH_FROM_SEQUENCE(media_sequence_checker_);
}

void HlsNetworkAccessImpl::ReadSegmentInternal(
    HlsDataSourceProvider::UrlDataSegment segment,
    HlsDataSourceProvider::ReadCb cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  // Callers of `ReadSegmentInternal` should enforce this.
  CHECK(data_source_provider_);

  data_source_provider_.AsyncCall(&HlsDataSourceProvider::ReadFromUrl)
      .WithArgs(std::move(segment),
                base::BindPostTaskToCurrentDefault(std::move(cb)));
}

void HlsNetworkAccessImpl::ReadAllInternal(
    const GURL& uri,
    HlsDataSourceProvider::ReadCb cb,
    DataSource::CacheMode cache_mode,
    DataSource::EncodingMode encoding_mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  // Callers of `ReadAllInternal` should enforce this.
  CHECK(data_source_provider_);
  HlsDataSourceProvider::UrlDataSegment segment(uri, std::nullopt, cache_mode,
                                                encoding_mode);
  ReadSegmentInternal(
      std::move(segment),
      base::BindOnce(&HlsNetworkAccessImpl::ReadUntilExhaustedHelper,
                     weak_factory_.GetWeakPtr(), std::move(cb)));
}

void HlsNetworkAccessImpl::ReadManifest(const GURL& uri,
                                        HlsDataSourceProvider::ReadCb cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!data_source_provider_) {
    std::move(cb).Run(HlsDemuxerStatus::Codes::kNetworkReadStopped);
    return;
  }
  ReadAllInternal(uri, std::move(cb), DataSource::CacheMode::kBypassCache,
                  DataSource::EncodingMode::kAllowGzip);
}

void HlsNetworkAccessImpl::MediaSegmentSecurityChecks(
    HlsDataSourceProvider::ReadCb cb,
    url::Origin manifest_origin,
    HlsDataSourceProvider::ReadResult result) {
  if (!result.has_value()) {
    std::move(cb).Run(std::move(result).error().AddHere());
    return;
  }

  auto stream = std::move(result).value();

  // Security considerations:
  //   - The stream may have data from up to three separate origins, including:
  //      - a decryption key (EXT-X-KEY)
  //      - a header (EXT-X-MAP)
  //      - content (EXTINF)
  //   - The header and the content might also include byte ranges as part of
  //     the request. the ranges are not required to have any sort of alignment.
  //   - Most media can be played in a cross-origin-tainted state in which the
  //     media is hosted on a different origin than the frame and where the
  //     media is not served with an appropriate access-control-allow-origin
  //     header. This is not the case with HLS.

  if (!stream->SecurityInfo().would_taint_origin) {
    // This request is considered safe entirely - all the requests happened
    // on either the same origin as the top frame, or the responses included
    // access-control-allow-origin headers that marked the top frame safe. This
    // request should be allowed.
    std::move(cb).Run(std::move(stream));
    return;
  }

  // If every single origin in the security metadata is the same as the
  // manifest's origin, it's also safe to allow, even though the frame will
  // see it as tainted data. Note that media content with an access header
  // allowing the manifest origin _will not_ be acceptable here - the origins
  // must be identical. The prior check for `would_taint_origin` will already
  // allow content served from multiple origins IFF those network responses
  // provide access-control-allow-origin headers for the top frame's origin.
  if (stream->SecurityInfo().IsSafeLoadFromManifestOrigin(manifest_origin)) {
    std::move(cb).Run(std::move(stream));
    return;
  }

  // Anything else is disallowed.
  std::move(cb).Run(
      {HlsDemuxerStatus::Codes::kNetworkReadError, "insecure media request"});
}

void HlsNetworkAccessImpl::ReadMediaSegment(const hls::MediaSegment& segment,
                                            bool read_chunked,
                                            bool include_init,
                                            HlsDataSourceProvider::ReadCb cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  if (!data_source_provider_) {
    std::move(cb).Run(HlsDemuxerStatus::Codes::kNetworkReadStopped);
    return;
  }

  std::optional<GURL> key_uri;
  auto enc_data = segment.GetEncryptionData();
  if (enc_data && enc_data->NeedsKeyFetch()) {
    key_uri = enc_data->GetUri();
  }

  std::optional<HlsDataSourceProvider::UrlDataSegment> init_segment;
  if (include_init) {
    if (auto init = segment.GetInitializationSegment()) {
      init_segment.emplace(init->GetUri(), init->GetByteRange(),
                           DataSource::CacheMode::kHitCache);
    }
  }

  HlsDataSourceProvider::UrlDataSegment media_segment(
      segment.GetUri(), segment.GetByteRange(),
      DataSource::CacheMode::kHitCache);

  auto state = base::MakeRefCounted<ParallelFetchState>(
      weak_factory_.GetWeakPtr(), segment.GetManifestOrigin(), enc_data,
      std::move(cb));

  state->Start(std::move(key_uri), std::move(init_segment),
               std::move(media_segment), read_chunked);
}

void HlsNetworkAccessImpl::ReadStream(
    std::unique_ptr<HlsDataSourceStream> stream,
    HlsDataSourceProvider::ReadCb cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(media_sequence_checker_);
  CHECK(stream);
  if (!data_source_provider_) {
    std::move(cb).Run(HlsDemuxerStatus::Codes::kNetworkReadStopped);
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

// static
void HlsNetworkAccessImpl::ReadUntilExhaustedHelper(
    base::WeakPtr<HlsNetworkAccessImpl> network_access,
    HlsDataSourceProvider::ReadCb cb,
    HlsDataSourceProvider::ReadResult result) {
  if (network_access) {
    network_access->ReadUntilExhausted(std::move(cb), std::move(result));
  } else {
    std::move(cb).Run(HlsDemuxerStatus::Codes::kNetworkReadAborted);
  }
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
             base::BindOnce(&HlsNetworkAccessImpl::ReadUntilExhaustedHelper,
                            weak_factory_.GetWeakPtr(), std::move(cb)));
}

}  // namespace media
