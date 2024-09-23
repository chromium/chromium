// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_NETWORK_ACCESS_IMPL_H_
#define MEDIA_FILTERS_HLS_NETWORK_ACCESS_IMPL_H_

#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "media/filters/hls_data_source_provider.h"
#include "media/filters/hls_network_access.h"

namespace media {

class MEDIA_EXPORT HlsNetworkAccessImpl final : public HlsNetworkAccess {
 public:
  explicit HlsNetworkAccessImpl(base::SequenceBound<HlsDataSourceProvider> dsp);

  ~HlsNetworkAccessImpl() override;

  // HlsNetworkAccess implementation
  void ReadKey(const hls::MediaSegment::EncryptionData& data,
               HlsDataSourceProvider::ReadCb cb) override;
  void ReadManifest(const GURL& uri, HlsDataSourceProvider::ReadCb cb) override;
  void ReadMediaSegment(const hls::MediaSegment& segment,
                        bool read_chunked,
                        bool include_init_segment,
                        HlsDataSourceProvider::ReadCb cb) override;
  void ReadStream(std::unique_ptr<HlsDataSourceStream> stream,
                  HlsDataSourceProvider::ReadCb cb) override;
  void AbortPendingReads(base::OnceClosure cb) override;

 private:
  void ReadUntilExhausted(HlsDataSourceProvider::ReadCb cb,
                          HlsDataSourceProvider::ReadResult result);
  void ReadSegmentQueueInternal(
      HlsDataSourceProvider::SegmentQueue media_segment_url_queue,
      HlsDataSourceProvider::ReadCb cb);
  void ReadAllInternal(const GURL& uri,
                       HlsDataSourceProvider::ReadCb cb,
                       bool bypass_cache = false);
  void OnKeyFetch(
      scoped_refptr<hls::MediaSegment::EncryptionData> enc_data,
      base::OnceCallback<void(HlsDataSourceProvider::ReadCb)> next_op,
      HlsDataSourceProvider::ReadCb cb,
      HlsDataSourceProvider::ReadResult result);

  // Ensure that safe member fields are only accessed on the media sequence.
  SEQUENCE_CHECKER(media_sequence_checker_);

  base::SequenceBound<HlsDataSourceProvider> data_source_provider_
      GUARDED_BY_CONTEXT(media_sequence_checker_);
  base::WeakPtrFactory<HlsNetworkAccessImpl> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_NETWORK_ACCESS_IMPL_H_
