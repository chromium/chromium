// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/source_type_mojom_traits.h"

#include "services/network/public/mojom/source_type.mojom.h"

namespace mojo {

network::mojom::SourceType
EnumTraits<network::mojom::SourceType, net::SourceStreamType>::ToMojom(
    net::SourceStreamType type) {
  switch (type) {
    case net::SourceStreamType::kBrotli:
      return network::mojom::SourceType::kBrotli;
    case net::SourceStreamType::kDeflate:
      return network::mojom::SourceType::kDeflate;
    case net::SourceStreamType::kGzip:
      return network::mojom::SourceType::kGzip;
    case net::SourceStreamType::kZstd:
      return network::mojom::SourceType::kZstd;
    case net::SourceStreamType::kNone:
      return network::mojom::SourceType::kNone;
    case net::SourceStreamType::kUnknown:
      return network::mojom::SourceType::kUnknown;
  }
  NOTREACHED();
}

bool EnumTraits<network::mojom::SourceType, net::SourceStreamType>::FromMojom(
    network::mojom::SourceType in,
    net::SourceStreamType* out) {
  switch (in) {
    case network::mojom::SourceType::kBrotli:
      *out = net::SourceStreamType::kBrotli;
      return true;
    case network::mojom::SourceType::kDeflate:
      *out = net::SourceStreamType::kDeflate;
      return true;
    case network::mojom::SourceType::kGzip:
      *out = net::SourceStreamType::kGzip;
      return true;
    case network::mojom::SourceType::kZstd:
      *out = net::SourceStreamType::kZstd;
      return true;
    case network::mojom::SourceType::kNone:
      *out = net::SourceStreamType::kNone;
      return true;
    case network::mojom::SourceType::kUnknown:
      *out = net::SourceStreamType::kUnknown;
      return true;
  }

  return false;
}

}  // namespace mojo
