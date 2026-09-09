// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONTENT_DECODING_UTIL_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONTENT_DECODING_UTIL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "net/filter/source_stream_type.h"

namespace network {

class DataBufferList;
class DataBufferFactory;

// A utility class for synchronously decoding compressed HTTP response bodies
// (e.g., DEFLATE, GZIP, BROTLI, ZSTD) into a DataBufferList.
//
// Note: This is introduced for the Renderer-Accessible HTTP Cache to
// decode cached response bodies directly in the renderer process.
class COMPONENT_EXPORT(NETWORK_CPP) ContentDecodingUtil {
 public:
  ContentDecodingUtil() = delete;

  // Decodes `upstream_data` using the provided `types` (e.g., DEFLATE, GZIP,
  // BROTLI, ZSTD).
  // The `types` vector represents the encodings in the order they appear in the
  // Content-Encoding header; decoding is performed in the reverse order.
  // `types` must not contain SourceStreamType::kNone or
  // SourceStreamType::kUnknown. Callers typically obtain `types` via
  // net::FilterSourceStream::GetContentEncodingTypes(), which guarantees that
  // only valid compression types are returned.
  //
  // Returns a DataBufferList containing the decoded data, or nullptr if an
  // error occurred during decoding.
  static std::unique_ptr<network::DataBufferList> Decode(
      base::span<const uint8_t> upstream_data,
      const std::vector<net::SourceStreamType>& types,
      network::DataBufferFactory& data_buffer_factory,
      uint32_t buffer_size = 64 * 1024);
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONTENT_DECODING_UTIL_H_
