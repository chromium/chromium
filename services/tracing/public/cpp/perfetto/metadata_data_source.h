// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_METADATA_DATA_SOURCE_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_METADATA_DATA_SOURCE_H_

#include <string_view>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "third_party/perfetto/include/perfetto/tracing/data_source.h"

namespace tracing {

std::string_view COMPONENT_EXPORT(TRACING_CPP)
    GetClockString(base::TimeTicks::Clock clock);

struct MetadataDataSourceTlsState;
struct MetadataDataSourceTraits : public perfetto::DefaultDataSourceTraits {
  using TlsStateType = MetadataDataSourceTlsState;
};

// This class is a data source that populates a ChromeMetadataPacket field in a
// perfetto trace. It emits the metadata OnStart; to avoid data loss, it should
// write to a buffer in DISCARD mode. This data source supports multiple
// concurrent sessions, unlike TraceEventMetadataSource in
// trace_event_data_source.h
class COMPONENT_EXPORT(TRACING_CPP) MetadataDataSource
    : public perfetto::DataSource<MetadataDataSource,
                                  MetadataDataSourceTraits> {
 public:
  using BundleRecorder = base::RepeatingCallback<void(
      perfetto::protos::pbzero::ChromeEventBundle*)>;
  using PacketRecorder =
      base::RepeatingCallback<void(perfetto::protos::pbzero::TracePacket*,
                                   bool /*privacy_filtering_enabled*/)>;
  static void RecordDefaultBundleMetadata(
      perfetto::protos::pbzero::ChromeEventBundle* bundle);

  static constexpr bool kRequiresCallbacksUnderLock = false;

  static void Register(scoped_refptr<base::SequencedTaskRunner> task_runner,
                       std::vector<BundleRecorder> bundle_recorders,
                       std::vector<PacketRecorder> packet_recorders);

  MetadataDataSource(scoped_refptr<base::SequencedTaskRunner> task_runner,
                     std::vector<BundleRecorder> bundle_recorders,
                     std::vector<PacketRecorder> packet_recorders);
  ~MetadataDataSource() override;

  void OnSetup(const SetupArgs&) override;
  void OnStart(const StartArgs&) override;
  void OnFlush(const FlushArgs&) override;
  void OnStop(const StopArgs&) override;

  bool privacy_filtering_enabled() const { return privacy_filtering_enabled_; }

  static void AddMetadataToBundle(
      std::string_view name,
      std::string_view value,
      perfetto::protos::pbzero::ChromeEventBundle* bundle);
  static void AddMetadataToBundle(
      std::string_view name,
      const base::Value& value,
      perfetto::protos::pbzero::ChromeEventBundle* bundle);

 protected:
  static void WriteMetadata(uintptr_t instance,
                            std::vector<BundleRecorder> bundle_recorders,
                            std::vector<PacketRecorder> packet_recorders);

 private:
  bool privacy_filtering_enabled_ = false;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::vector<BundleRecorder> bundle_recorders_;
  std::vector<PacketRecorder> packet_recorders_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_METADATA_DATA_SOURCE_H_
