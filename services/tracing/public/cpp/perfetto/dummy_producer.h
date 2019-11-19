// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_DUMMY_PRODUCER_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_DUMMY_PRODUCER_H_

#include "services/tracing/public/cpp/perfetto/system_producer.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/producer.h"

namespace tracing {

class COMPONENT_EXPORT(TRACING_CPP) DummyProducer : public SystemProducer {
 public:
  DummyProducer(PerfettoTaskRunner* task_runner);
  ~DummyProducer() override;

  // perfetto::Producer functions.
  void OnConnect() override;
  void OnDisconnect() override;
  void OnTracingSetup() override;
  void SetupDataSource(perfetto::DataSourceInstanceID,
                       const perfetto::DataSourceConfig&) override;
  void StartDataSource(perfetto::DataSourceInstanceID,
                       const perfetto::DataSourceConfig&) override;
  void StopDataSource(perfetto::DataSourceInstanceID) override;
  void Flush(perfetto::FlushRequestID,
             const perfetto::DataSourceInstanceID* data_source_ids,
             size_t num_data_sources) override;
  void ClearIncrementalState(
      const perfetto::DataSourceInstanceID* data_source_ids,
      size_t num_data_sources) override;

  // perfetto::TracingService::ProducerEndpoint functions.
  void RegisterDataSource(const perfetto::DataSourceDescriptor&) override;
  void UnregisterDataSource(const std::string& name) override;
  void RegisterTraceWriter(uint32_t writer_id, uint32_t target_buffer) override;
  void UnregisterTraceWriter(uint32_t writer_id) override;
  void CommitData(const perfetto::CommitDataRequest& commit,
                  CommitDataCallback callback) override;
  perfetto::SharedMemory* shared_memory() const override;
  size_t shared_buffer_page_size_kb() const override;
  perfetto::SharedMemoryArbiter* GetSharedMemoryArbiter() override;
  perfetto::SharedMemoryArbiter* GetInProcessShmemArbiter() override;
  void NotifyFlushComplete(perfetto::FlushRequestID) override;
  void NotifyDataSourceStarted(perfetto::DataSourceInstanceID) override;
  void NotifyDataSourceStopped(perfetto::DataSourceInstanceID) override;
  void ActivateTriggers(const std::vector<std::string>&) override;

  // tracing::PerfettoProducer functions.
  void NewDataSourceAdded(
      const PerfettoTracedProcess::DataSourceBase* const data_source) override;
  bool IsTracingActive() override;

  // Functions expected for SystemProducer
  void DisconnectWithReply(base::OnceClosure on_disconnect_complete) override;
  bool IsDummySystemProducerForTesting() override;
  void ResetSequenceForTesting() override;
};
}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_DUMMY_PRODUCER_H_
