// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_PRODUCER_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_PRODUCER_H_

#include <memory>

#include "base/component_export.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/tracing_service.h"

namespace perfetto {
class SharedMemoryArbiter;
class StartupTraceWriterRegistry;
}  // namespace perfetto

namespace tracing {

// This class represents the perfetto producer endpoint which is used for
// producers to talk to the Perfetto service. It also provides methods to
// interact with the shared memory buffer by binding and creating TraceWriters.
//
// In addition to the PerfettoProducers' pure virtual methods, subclasses must
// implement the remaining methods of the ProducerEndpoint interface.
class COMPONENT_EXPORT(TRACING_CPP) PerfettoProducer
    : public perfetto::TracingService::ProducerEndpoint {
 public:
  PerfettoProducer(PerfettoTaskRunner* task_runner);

  ~PerfettoProducer() override;

  // Binds the registry and its trace writers to the ProducerClient's SMB, to
  // write into the given target buffer. The ownership of |registry| is
  // transferred to PerfettoProducer (and its SharedMemoryArbiter).
  //
  // Should only be called while a tracing session is active and a
  // SharedMemoryArbiter exists.
  void BindStartupTraceWriterRegistry(
      std::unique_ptr<perfetto::StartupTraceWriterRegistry> registry,
      perfetto::BufferID target_buffer);

  // Used by the DataSource implementations to create TraceWriters
  // for writing their protobufs, and respond to flushes.
  //
  // Should only be called while a tracing session is active and a
  // SharedMemoryArbiter exists.
  std::unique_ptr<perfetto::TraceWriter> CreateTraceWriter(
      perfetto::BufferID target_buffer,
      perfetto::BufferExhaustedPolicy =
          perfetto::BufferExhaustedPolicy::kDefault) override;

  // Informs the PerfettoProducer a new Data Source was added. This instance
  // will also be found in |data_sources| having just be inserted before this
  // method is called by PerfettoTracedProcess. This enables the
  // PerfettoProducer to perform initialization on new data sources.
  virtual void NewDataSourceAdded(
      const PerfettoTracedProcess::DataSourceBase* const data_source) = 0;

  // Returns true if this PerfettoProducer is currently tracing.
  virtual bool IsTracingActive() = 0;

  static void DeleteSoonForTesting(
      std::unique_ptr<PerfettoProducer> perfetto_producer);

 protected:
  // Returns the SMA of the SharedMemory from the perfetto service or nullptr if
  // not initialized (no trace has ever been started).
  virtual perfetto::SharedMemoryArbiter* GetSharedMemoryArbiter() = 0;

  PerfettoTaskRunner* task_runner();

 private:
  PerfettoTaskRunner* const task_runner_;
};
}  // namespace tracing
#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_PRODUCER_H_
