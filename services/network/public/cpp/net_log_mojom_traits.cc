// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/net_log_mojom_traits.h"

namespace mojo {

// static
bool EnumTraits<network::mojom::NetLogCaptureMode, net::NetLogCaptureMode>::
    FromMojom(network::mojom::NetLogCaptureMode capture_mode,
              net::NetLogCaptureMode* out) {
  switch (capture_mode) {
    case network::mojom::NetLogCaptureMode::DEFAULT:
      *out = net::NetLogCaptureMode::kDefault;
      return true;
    case network::mojom::NetLogCaptureMode::INCLUDE_PRIVACY_INFO:
      *out = net::NetLogCaptureMode::kIncludeSensitive;
      return true;
    case network::mojom::NetLogCaptureMode::EVERYTHING:
      *out = net::NetLogCaptureMode::kEverything;
      return true;
  }
  return false;
}

// static
network::mojom::NetLogCaptureMode
EnumTraits<network::mojom::NetLogCaptureMode, net::NetLogCaptureMode>::ToMojom(
    net::NetLogCaptureMode capture_mode) {
  switch (capture_mode) {
    case net::NetLogCaptureMode::kDefault:
      return network::mojom::NetLogCaptureMode::DEFAULT;
    case net::NetLogCaptureMode::kIncludeSensitive:
      return network::mojom::NetLogCaptureMode::INCLUDE_PRIVACY_INFO;
    case net::NetLogCaptureMode::kEverything:
      return network::mojom::NetLogCaptureMode::EVERYTHING;
  }

  NOTREACHED();
  return network::mojom::NetLogCaptureMode::DEFAULT;
}

}  // namespace mojo
