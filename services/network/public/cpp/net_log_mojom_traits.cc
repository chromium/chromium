// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/net_log_mojom_traits.h"

#include "net/log/file_net_log_observer.h"
#include "net/log/net_log_capture_mode.h"

namespace mojo {

// static
net::NetLogCaptureMode
EnumTraits<network::mojom::NetLogCaptureMode, net::NetLogCaptureMode>::
    FromMojom(network::mojom::NetLogCaptureMode capture_mode) {
  switch (capture_mode) {
    case network::mojom::NetLogCaptureMode::HEAVILY_REDACTED:
      return net::NetLogCaptureMode::kHeavilyRedacted;
    case network::mojom::NetLogCaptureMode::DEFAULT:
      return net::NetLogCaptureMode::kDefault;
    case network::mojom::NetLogCaptureMode::INCLUDE_PRIVACY_INFO:
      return net::NetLogCaptureMode::kIncludeSensitive;
    case network::mojom::NetLogCaptureMode::EVERYTHING:
      return net::NetLogCaptureMode::kEverything;
  }
  NOTREACHED();
}

// static
network::mojom::NetLogCaptureMode
EnumTraits<network::mojom::NetLogCaptureMode, net::NetLogCaptureMode>::ToMojom(
    net::NetLogCaptureMode capture_mode) {
  switch (capture_mode) {
    case net::NetLogCaptureMode::kHeavilyRedacted:
      return network::mojom::NetLogCaptureMode::HEAVILY_REDACTED;
    case net::NetLogCaptureMode::kDefault:
      return network::mojom::NetLogCaptureMode::DEFAULT;
    case net::NetLogCaptureMode::kIncludeSensitive:
      return network::mojom::NetLogCaptureMode::INCLUDE_PRIVACY_INFO;
    case net::NetLogCaptureMode::kEverything:
      return network::mojom::NetLogCaptureMode::EVERYTHING;
  }

  NOTREACHED();
}

// static
net::NetLogEventPhase
EnumTraits<network::mojom::NetLogEventPhase, net::NetLogEventPhase>::FromMojom(
    network::mojom::NetLogEventPhase capture_mode) {
  switch (capture_mode) {
    case network::mojom::NetLogEventPhase::BEGIN:
      return net::NetLogEventPhase::BEGIN;
    case network::mojom::NetLogEventPhase::END:
      return net::NetLogEventPhase::END;
    case network::mojom::NetLogEventPhase::NONE:
      return net::NetLogEventPhase::NONE;
  }
  NOTREACHED();
}

// static
network::mojom::NetLogEventPhase
EnumTraits<network::mojom::NetLogEventPhase, net::NetLogEventPhase>::ToMojom(
    net::NetLogEventPhase capture_mode) {
  switch (capture_mode) {
    case net::NetLogEventPhase::BEGIN:
      return network::mojom::NetLogEventPhase::BEGIN;
    case net::NetLogEventPhase::END:
      return network::mojom::NetLogEventPhase::END;
    case net::NetLogEventPhase::NONE:
      return network::mojom::NetLogEventPhase::NONE;
  }

  NOTREACHED();
}

// static
net::NetLogFileFormat
EnumTraits<network::mojom::NetLogFileFormat, net::NetLogFileFormat>::FromMojom(
    network::mojom::NetLogFileFormat file_format) {
  switch (file_format) {
    case network::mojom::NetLogFileFormat::JSON:
      return net::NetLogFileFormat::kJson;
    case network::mojom::NetLogFileFormat::NDJSON:
      return net::NetLogFileFormat::kNdjson;
  }
  NOTREACHED();
}

// static
network::mojom::NetLogFileFormat
EnumTraits<network::mojom::NetLogFileFormat, net::NetLogFileFormat>::ToMojom(
    net::NetLogFileFormat file_format) {
  switch (file_format) {
    case net::NetLogFileFormat::kJson:
      return network::mojom::NetLogFileFormat::JSON;
    case net::NetLogFileFormat::kNdjson:
      return network::mojom::NetLogFileFormat::NDJSON;
  }

  NOTREACHED();
}

}  // namespace mojo
