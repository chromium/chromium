// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_NETWORK_ACCESS_H_
#define MEDIA_FILTERS_HLS_NETWORK_ACCESS_H_

#include "media/base/media_export.h"
#include "media/filters/hls_data_source_provider.h"
#include "media/filters/hls_demuxer_status.h"
#include "media/formats/hls/media_segment.h"

namespace media {

// Interface for `HlsRendition` to make data requests to avoid having to own or
// create data sources.
class MEDIA_EXPORT HlsNetworkAccess {
 public:
  virtual ~HlsNetworkAccess() = 0;

  // Reads the encryption key from the url specified within, and posts the
  // result back through `cb`.
  virtual void ReadKey(const hls::MediaSegment::EncryptionData& data,
                       HlsDataSourceProvider::ReadCb cb) = 0;

  // Reads the entirety of an HLS manifest from `uri`, and posts the result back
  // through `cb`.
  virtual void ReadManifest(const GURL& uri,
                            HlsDataSourceProvider::ReadCb cb) = 0;

  // Reads media data from a media segment. If `read_chunked` is false, then
  // the resulting stream will be fully read until either EOS, or its optional
  // range is fully satisfied. If `read_chunked` is true, then only some data
  // will be present in the resulting stream, and more data can be requested
  // through the `ReadStream` method. If `include_init_segment` is true, then
  // the init segment data will be prepended to the buffer returned if this
  // segment has an initialization_segment.
  // TODO (crbug.com/1266991): Remove `read_chunked`, which should ideally
  // always be true for segments. HlsRenditionImpl needs to handle chunked reads
  // more effectively first.
  virtual void ReadMediaSegment(const hls::MediaSegment& segment,
                                bool read_chunked,
                                bool include_init_segment,
                                HlsDataSourceProvider::ReadCb cb) = 0;

  // Continue reading from a partially read stream.
  virtual void ReadStream(std::unique_ptr<HlsDataSourceStream> stream,
                          HlsDataSourceProvider::ReadCb cb) = 0;

  // Cancels all pending reads, and evaluates `cb` on completion. Pending reads
  // will respond to their respective callbacks with errors.
  virtual void AbortPendingReads(base::OnceClosure cb) = 0;
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_NETWORK_ACCESS_H_
