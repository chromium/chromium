// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/mojom/data_source_config_mojom_traits.h"

#include <utility>

#include "mojo/public/cpp/base/byte_string_mojom_traits.h"
#include "services/tracing/public/mojom/chrome_config_mojom_traits.h"
#include "services/tracing/public/mojom/interceptor_config_mojom_traits.h"

namespace mojo {
bool StructTraits<tracing::mojom::DataSourceConfigDataView,
                  perfetto::DataSourceConfig>::
    Read(tracing::mojom::DataSourceConfigDataView data,
         perfetto::DataSourceConfig* out) {
  std::string name, legacy_config, track_event_config_raw, etw_config_raw;
  perfetto::ChromeConfig chrome_config;
  std::optional<perfetto::protos::gen::InterceptorConfig> interceptor_config;
  if (!data.ReadName(&name) || !data.ReadChromeConfig(&chrome_config) ||
      !data.ReadLegacyConfig(&legacy_config) ||
      !data.ReadTrackEventConfigRaw(&track_event_config_raw) ||
      !data.ReadEtwConfigRaw(&etw_config_raw) ||
      !data.ReadInterceptorConfig(&interceptor_config)) {
    return false;
  }
  out->set_name(name);
  out->set_target_buffer(data.target_buffer());
  out->set_trace_duration_ms(data.trace_duration_ms());
  out->set_tracing_session_id(data.tracing_session_id());
  *out->mutable_chrome_config() = std::move(chrome_config);
  if (interceptor_config) {
    *out->mutable_interceptor_config() = std::move(*interceptor_config);
  }
  // Perfetto compares configs based on their serialized representation. Setting
  // a field to an empty value flips the "has_field" bit and makes it a
  // different config from the Perfetto's point of view.
  // Make sure that only non-empty fields are being set.
  if (!legacy_config.empty())
    out->set_legacy_config(legacy_config);
  if (!track_event_config_raw.empty())
    out->set_track_event_config_raw(track_event_config_raw);
  if (!etw_config_raw.empty()) {
    out->set_etw_config_raw(etw_config_raw);
  }
  return true;
}
}  // namespace mojo
