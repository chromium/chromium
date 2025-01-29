// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/source_type_mojom_traits.h"

#include "services/network/public/mojom/source_type.mojom.h"

namespace mojo {

network::mojom::SourceType
EnumTraits<network::mojom::SourceType, net::SourceStream::SourceType>::ToMojom(
    net::SourceStream::SourceType type) {
  switch (type) {
    case net::SourceStream::SourceType::TYPE_BROTLI:
      return network::mojom::SourceType::kBrotli;
    case net::SourceStream::SourceType::TYPE_DEFLATE:
      return network::mojom::SourceType::kDeflate;
    case net::SourceStream::SourceType::TYPE_GZIP:
      return network::mojom::SourceType::kGzip;
    case net::SourceStream::SourceType::TYPE_ZSTD:
      return network::mojom::SourceType::kZstd;
    case net::SourceStream::SourceType::TYPE_NONE:
      return network::mojom::SourceType::kNone;
    case net::SourceStream::SourceType::TYPE_UNKNOWN:
      return network::mojom::SourceType::kUnknown;
  }
  NOTREACHED();
}

bool EnumTraits<network::mojom::SourceType, net::SourceStream::SourceType>::
    FromMojom(network::mojom::SourceType in,
              net::SourceStream::SourceType* out) {
  switch (in) {
    case network::mojom::SourceType::kBrotli:
      *out = net::SourceStream::SourceType::TYPE_BROTLI;
      return true;
    case network::mojom::SourceType::kDeflate:
      *out = net::SourceStream::SourceType::TYPE_DEFLATE;
      return true;
    case network::mojom::SourceType::kGzip:
      *out = net::SourceStream::SourceType::TYPE_GZIP;
      return true;
    case network::mojom::SourceType::kZstd:
      *out = net::SourceStream::SourceType::TYPE_ZSTD;
      return true;
    case network::mojom::SourceType::kNone:
      *out = net::SourceStream::SourceType::TYPE_NONE;
      return true;
    case network::mojom::SourceType::kUnknown:
      *out = net::SourceStream::SourceType::TYPE_UNKNOWN;
      return true;
  }

  return false;
}

}  // namespace mojo
