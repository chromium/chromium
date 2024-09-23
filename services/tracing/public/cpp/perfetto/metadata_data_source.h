// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_METADATA_DATA_SOURCE_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_METADATA_DATA_SOURCE_H_

#include "base/component_export.h"
#include "third_party/perfetto/include/perfetto/tracing/data_source.h"

namespace tracing {

// This class is a data source that populates a ChromeMetadataPacket
// field in a perfetto trace. Currently only field trial hashes
// and version code (for official Android builds) are recorded
// when the tracing session is flushed (to cover the cases when a session is
// cloned) or stopped. This data source supports multiple concurrent sessions,
// unlike TraceEventMetadataSource in trace_event_data_source.h
class COMPONENT_EXPORT(TRACING_CPP) MetadataDataSource
    : public perfetto::DataSource<MetadataDataSource> {
 public:
  static void Register();

  void OnStart(const StartArgs&) override;
  void OnFlush(const FlushArgs&) override;
  void OnStop(const StopArgs&) override;

 private:
  void WriteMetadata();
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_METADATA_DATA_SOURCE_H_
