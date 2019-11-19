// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This defines mappings from mojom IPC representations to their native perfetto
// equivalents.

#ifndef SERVICES_TRACING_PUBLIC_MOJOM_TRACE_CONFIG_MOJOM_TRAITS_H_
#define SERVICES_TRACING_PUBLIC_MOJOM_TRACE_CONFIG_MOJOM_TRAITS_H_

#include <string>
#include <vector>

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"

namespace mojo {

// perfetto::TraceConfig::BufferConfig::FillPolicy
template <>
struct EnumTraits<tracing::mojom::BufferFillPolicy,
                  perfetto::TraceConfig::BufferConfig::FillPolicy> {
  static tracing::mojom::BufferFillPolicy ToMojom(
      perfetto::TraceConfig::BufferConfig::FillPolicy input) {
    switch (input) {
      case perfetto::TraceConfig::BufferConfig::UNSPECIFIED:
        return tracing::mojom::BufferFillPolicy::kUnspecified;
      case perfetto::TraceConfig::BufferConfig::RING_BUFFER:
        return tracing::mojom::BufferFillPolicy::kRingBuffer;
      case perfetto::TraceConfig::BufferConfig::DISCARD:
        return tracing::mojom::BufferFillPolicy::kDiscard;
    }
  }

  static bool FromMojom(tracing::mojom::BufferFillPolicy input,
                        perfetto::TraceConfig::BufferConfig::FillPolicy* out) {
    switch (input) {
      case tracing::mojom::BufferFillPolicy::kUnspecified:
        *out = perfetto::TraceConfig::BufferConfig::UNSPECIFIED;
        return true;
      case tracing::mojom::BufferFillPolicy::kRingBuffer:
        *out = perfetto::TraceConfig::BufferConfig::RING_BUFFER;
        return true;
      case tracing::mojom::BufferFillPolicy::kDiscard:
        *out = perfetto::TraceConfig::BufferConfig::DISCARD;
        return true;
    }
  }
};

// perfetto::TraceConfig::BufferConfig
template <>
class StructTraits<tracing::mojom::BufferConfigDataView,
                   perfetto::TraceConfig::BufferConfig> {
 public:
  static uint32_t size_kb(const perfetto::TraceConfig::BufferConfig& src) {
    return src.size_kb();
  }

  static perfetto::TraceConfig::BufferConfig::FillPolicy fill_policy(
      const perfetto::TraceConfig::BufferConfig& src) {
    return src.fill_policy();
  }

  static bool Read(tracing::mojom::BufferConfigDataView data,
                   perfetto::TraceConfig::BufferConfig* out);
};

// perfetto::TraceConfig::DataSource
template <>
class StructTraits<tracing::mojom::DataSourceDataView,
                   perfetto::TraceConfig::DataSource> {
 public:
  static const perfetto::DataSourceConfig& config(
      const perfetto::TraceConfig::DataSource& src) {
    return src.config();
  }

  static const std::vector<std::string>& producer_name_filter(
      const perfetto::TraceConfig::DataSource& src) {
    return src.producer_name_filter();
  }

  static bool Read(tracing::mojom::DataSourceDataView data,
                   perfetto::TraceConfig::DataSource* out);
};

// perfetto::TraceConfig::BuiltinDataSource
template <>
class StructTraits<tracing::mojom::PerfettoBuiltinDataSourceDataView,
                   perfetto::TraceConfig::BuiltinDataSource> {
 public:
  static bool disable_clock_snapshotting(
      const perfetto::TraceConfig::BuiltinDataSource& src) {
    return src.disable_clock_snapshotting();
  }

  static bool disable_trace_config(
      const perfetto::TraceConfig::BuiltinDataSource& src) {
    return src.disable_trace_config();
  }

  static bool disable_system_info(
      const perfetto::TraceConfig::BuiltinDataSource& src) {
    return src.disable_system_info();
  }

  static bool Read(tracing::mojom::PerfettoBuiltinDataSourceDataView data,
                   perfetto::TraceConfig::BuiltinDataSource* out);
};

// perfetto::TraceConfig::IncrementalStateConfig
template <>
class StructTraits<tracing::mojom::IncrementalStateConfigDataView,
                   perfetto::TraceConfig::IncrementalStateConfig> {
 public:
  static uint32_t clear_period_ms(
      const perfetto::TraceConfig::IncrementalStateConfig& src) {
    return src.clear_period_ms();
  }

  static bool Read(tracing::mojom::IncrementalStateConfigDataView data,
                   perfetto::TraceConfig::IncrementalStateConfig* out);
};

// perfetto::TraceConfig
template <>
class StructTraits<tracing::mojom::TraceConfigDataView, perfetto::TraceConfig> {
 public:
  static const std::vector<perfetto::TraceConfig::DataSource>& data_sources(
      const perfetto::TraceConfig& src) {
    return src.data_sources();
  }

  static const perfetto::TraceConfig::BuiltinDataSource&
  perfetto_builtin_data_source(const perfetto::TraceConfig& src) {
    return src.builtin_data_sources();
  }

  static const std::vector<perfetto::TraceConfig::BufferConfig>& buffers(
      const perfetto::TraceConfig& src) {
    return src.buffers();
  }

  static const perfetto::TraceConfig::IncrementalStateConfig&
  incremental_state_config(const perfetto::TraceConfig& src) {
    return src.incremental_state_config();
  }

  static uint32_t duration_ms(const perfetto::TraceConfig& src) {
    return src.duration_ms();
  }

  static bool Read(tracing::mojom::TraceConfigDataView data,
                   perfetto::TraceConfig* out);
};

}  // namespace mojo
#endif  // SERVICES_TRACING_PUBLIC_MOJOM_TRACE_CONFIG_MOJOM_TRAITS_H_
