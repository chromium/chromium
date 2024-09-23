// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This defines mappings from mojom IPC representations to their native perfetto
// equivalents.

#ifndef SERVICES_TRACING_PUBLIC_MOJOM_DATA_SOURCE_CONFIG_MOJOM_TRAITS_H_
#define SERVICES_TRACING_PUBLIC_MOJOM_DATA_SOURCE_CONFIG_MOJOM_TRAITS_H_

#include <optional>
#include <string>

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/tracing/public/mojom/perfetto_service.mojom-shared.h"
#include "third_party/perfetto/include/perfetto/tracing/core/chrome_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_config.h"
#include "third_party/perfetto/protos/perfetto/config/interceptor_config.gen.h"

namespace mojo {
template <>
class StructTraits<tracing::mojom::DataSourceConfigDataView,
                   perfetto::DataSourceConfig> {
 public:
  static const std::string& name(const perfetto::DataSourceConfig& src) {
    return src.name();
  }
  static uint32_t target_buffer(const perfetto::DataSourceConfig& src) {
    return src.target_buffer();
  }
  static uint32_t trace_duration_ms(const perfetto::DataSourceConfig& src) {
    return src.trace_duration_ms();
  }
  static uint64_t tracing_session_id(const perfetto::DataSourceConfig& src) {
    return src.tracing_session_id();
  }
  static const perfetto::ChromeConfig& chrome_config(
      const perfetto::DataSourceConfig& src) {
    return src.chrome_config();
  }
  static const std::string& legacy_config(
      const perfetto::DataSourceConfig& src) {
    return src.legacy_config();
  }
  static const std::string& track_event_config_raw(
      const perfetto::DataSourceConfig& src) {
    return src.track_event_config_raw();
  }
  static const std::string& etw_config_raw(
      const perfetto::DataSourceConfig& src) {
    return src.etw_config_raw();
  }

  static std::optional<perfetto::protos::gen::InterceptorConfig>
  interceptor_config(const perfetto::DataSourceConfig& src) {
    if (src.has_interceptor_config()) {
      return src.interceptor_config();
    }
    return std::nullopt;
  }

  static bool Read(tracing::mojom::DataSourceConfigDataView data,
                   perfetto::DataSourceConfig* out);
};
}  // namespace mojo
#endif  // SERVICES_TRACING_PUBLIC_MOJOM_DATA_SOURCE_CONFIG_MOJOM_TRAITS_H_
