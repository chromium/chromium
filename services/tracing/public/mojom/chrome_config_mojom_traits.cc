// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/mojom/chrome_config_mojom_traits.h"

#include <utility>

namespace mojo {
// static
bool StructTraits<
    tracing::mojom::ChromeConfigDataView,
    perfetto::ChromeConfig>::Read(tracing::mojom::ChromeConfigDataView data,
                                  perfetto::ChromeConfig* out) {
  std::string config;
  if (!data.ReadTraceConfig(&config)) {
    return false;
  }
  out->set_trace_config(std::move(config));
  out->set_privacy_filtering_enabled(data.privacy_filtering_enabled());
  out->set_convert_to_legacy_json(data.convert_to_legacy_json());
  switch (data.client_priority()) {
    case tracing::mojom::TracingClientPriority::kBackground:
      out->set_client_priority(perfetto::protos::gen::ChromeConfig::BACKGROUND);
      break;
    case tracing::mojom::TracingClientPriority::kUserInitiated:
      out->set_client_priority(
          perfetto::protos::gen::ChromeConfig::USER_INITIATED);
      break;
    case tracing::mojom::TracingClientPriority::kUnknown:
      out->set_client_priority(perfetto::protos::gen::ChromeConfig::UNKNOWN);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  return true;
}
}  // namespace mojo
