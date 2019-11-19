// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/mojom/trace_config_mojom_traits.h"

#include <utility>

#include "services/tracing/public/mojom/data_source_config_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<tracing::mojom::BufferConfigDataView,
                  perfetto::TraceConfig::BufferConfig>::
    Read(tracing::mojom::BufferConfigDataView data,
         perfetto::TraceConfig::BufferConfig* out) {
  out->set_size_kb(data.size_kb());
  perfetto::TraceConfig::BufferConfig::FillPolicy policy;
  if (data.ReadFillPolicy(&policy)) {
    out->set_fill_policy(policy);
  }

  return true;
}

// static
bool StructTraits<tracing::mojom::DataSourceDataView,
                  perfetto::TraceConfig::DataSource>::
    Read(tracing::mojom::DataSourceDataView data,
         perfetto::TraceConfig::DataSource* out) {
  perfetto::DataSourceConfig config;
  if (!data.ReadConfig(&config)) {
    return false;
  }

  *out->mutable_config() = std::move(config);

  std::vector<std::string> producer_name_filter;
  if (!data.ReadProducerNameFilter(&producer_name_filter)) {
    return false;
  }

  for (auto&& filter : producer_name_filter) {
    *out->add_producer_name_filter() = std::move(filter);
  }

  return true;
}

// static
bool StructTraits<tracing::mojom::PerfettoBuiltinDataSourceDataView,
                  perfetto::TraceConfig::BuiltinDataSource>::
    Read(tracing::mojom::PerfettoBuiltinDataSourceDataView data,
         perfetto::TraceConfig::BuiltinDataSource* out) {
  out->set_disable_clock_snapshotting(data.disable_clock_snapshotting());
  out->set_disable_trace_config(data.disable_trace_config());
  out->set_disable_system_info(data.disable_system_info());
  return true;
}

// static
bool StructTraits<tracing::mojom::IncrementalStateConfigDataView,
                  perfetto::TraceConfig::IncrementalStateConfig>::
    Read(tracing::mojom::IncrementalStateConfigDataView data,
         perfetto::TraceConfig::IncrementalStateConfig* out) {
  out->set_clear_period_ms(data.clear_period_ms());
  return true;
}

// static
bool StructTraits<tracing::mojom::TraceConfigDataView, perfetto::TraceConfig>::
    Read(tracing::mojom::TraceConfigDataView data, perfetto::TraceConfig* out) {
  std::vector<perfetto::TraceConfig::DataSource> data_sources;
  std::vector<perfetto::TraceConfig::BufferConfig> buffers;
  if (!data.ReadDataSources(&data_sources) || !data.ReadBuffers(&buffers) ||
      !data.ReadPerfettoBuiltinDataSource(
          out->mutable_builtin_data_sources()) ||
      !data.ReadIncrementalStateConfig(
          out->mutable_incremental_state_config())) {
    return false;
  }

  for (auto&& data_source : data_sources) {
    *out->add_data_sources() = std::move(data_source);
  }

  for (auto&& buffer : buffers) {
    *out->add_buffers() = std::move(buffer);
  }

  out->set_duration_ms(data.duration_ms());
  return true;
}

}  // namespace mojo
