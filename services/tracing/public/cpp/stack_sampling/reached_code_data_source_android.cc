// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/stack_sampling/reached_code_data_source_android.h"

#include <utility>
#include <vector>

#include "base/android/reached_addresses_bitset.h"
#include "base/android/reached_code_profiler.h"
#include "base/debug/elf_reader.h"
#include "services/tracing/public/cpp/perfetto/perfetto_producer.h"
#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"
#include "third_party/perfetto/include/perfetto/tracing/data_source.h"
#include "third_party/perfetto/protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/profiling/profile_common.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/profiling/profile_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

extern char __ehdr_start;

namespace tracing {

// static
ReachedCodeDataSource* ReachedCodeDataSource::Get() {
  static base::NoDestructor<ReachedCodeDataSource> instance;
  return instance.get();
}

ReachedCodeDataSource::ReachedCodeDataSource()
    : DataSourceBase(mojom::kReachedCodeProfilerSourceName) {}

ReachedCodeDataSource::~ReachedCodeDataSource() {
  NOTREACHED();
}

void ReachedCodeDataSource::StartTracing(
    PerfettoProducer* producer,
    const perfetto::DataSourceConfig& data_source_config) {
  trace_writer_ =
      producer->CreateTraceWriter(data_source_config.target_buffer());
}

void ReachedCodeDataSource::StopTracing(
    base::OnceClosure stop_complete_callback) {
  WriteProfileData();

  producer_ = nullptr;
  trace_writer_.reset();
  std::move(stop_complete_callback).Run();
}

void ReachedCodeDataSource::WriteProfileData() {
  if (!base::android::IsReachedCodeProfilerEnabled()) {
    return;
  }

  auto* bitset = base::android::ReachedAddressesBitset::GetTextBitset();
  // |bitset| is null when the build does not support code ordering.
  if (!bitset) {
    return;
  }

  perfetto::TraceWriter::TracePacketHandle trace_packet =
      trace_writer_->NewTracePacket();

  auto* interned_data = trace_packet->set_interned_data();
  base::debug::ElfBuildIdBuffer buf;
  size_t size = base::debug::ReadElfBuildId(&__ehdr_start, true, buf);
  if (size > 0) {
    std::string module_id(buf, size);
    TracingSamplerProfiler::MangleModuleIDIfNeeded(&module_id);
    auto* str = interned_data->add_build_ids();
    str->set_iid(0);
    str->set_str(module_id);
  }

  base::Optional<base::StringPiece> library_name =
      base::debug::ReadElfLibraryName(&__ehdr_start);
  if (library_name) {
    auto* str = interned_data->add_mapping_paths();
    str->set_iid(0);
    str->set_str(library_name->as_string());
  }

  std::vector<uint32_t> offsets = bitset->GetReachedOffsets();
  // Delta encoded timestamps and interned data require incremental state.
  auto* streaming_profile_packet = trace_packet->set_streaming_profile_packet();
  for (uint32_t offset : offsets) {
    // TODO(ssid): add a new packed field to the trace packet proto.
    streaming_profile_packet->add_callstack_iid(offset);
  }
}

void ReachedCodeDataSource::Flush(
    base::RepeatingClosure flush_complete_callback) {
  flush_complete_callback.Run();
}

void ReachedCodeDataSource::ClearIncrementalState() {}

}  // namespace tracing
