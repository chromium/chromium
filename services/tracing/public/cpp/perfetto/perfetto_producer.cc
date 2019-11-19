// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/perfetto_producer.h"

#include "third_party/perfetto/include/perfetto/ext/tracing/core/shared_memory_arbiter.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/startup_trace_writer_registry.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"

namespace tracing {

PerfettoProducer::PerfettoProducer(PerfettoTaskRunner* task_runner)
    : task_runner_(task_runner) {
  DCHECK(task_runner_);
}

PerfettoProducer::~PerfettoProducer() {}

void PerfettoProducer::BindStartupTraceWriterRegistry(
    std::unique_ptr<perfetto::StartupTraceWriterRegistry> registry,
    perfetto::BufferID target_buffer) {
  DCHECK(GetSharedMemoryArbiter());
  return GetSharedMemoryArbiter()->BindStartupTraceWriterRegistry(
      std::move(registry), target_buffer);
}

std::unique_ptr<perfetto::TraceWriter> PerfettoProducer::CreateTraceWriter(
    perfetto::BufferID target_buffer,
    perfetto::BufferExhaustedPolicy buffer_exhausted_policy) {
  DCHECK(GetSharedMemoryArbiter());
  // Chromium uses BufferExhaustedPolicy::kDrop to avoid stalling trace writers
  // when the chunks in the SMB are exhausted. Stalling could otherwise lead to
  // deadlocks in chromium, because a stalled mojo IPC thread could prevent
  // CommitRequest messages from reaching the perfetto service.
  buffer_exhausted_policy = perfetto::BufferExhaustedPolicy::kDrop;
  return GetSharedMemoryArbiter()->CreateTraceWriter(target_buffer,
                                                     buffer_exhausted_policy);
}

PerfettoTaskRunner* PerfettoProducer::task_runner() {
  return task_runner_;
}

void PerfettoProducer::DeleteSoonForTesting(
    std::unique_ptr<PerfettoProducer> perfetto_producer) {
  PerfettoTracedProcess::GetTaskRunner()->GetOrCreateTaskRunner()->DeleteSoon(
      FROM_HERE, std::move(perfetto_producer));
}

}  // namespace tracing
