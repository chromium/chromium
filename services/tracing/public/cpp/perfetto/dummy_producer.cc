// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/dummy_producer.h"

namespace tracing {

DummyProducer::DummyProducer(PerfettoTaskRunner* task_runner)
    : SystemProducer(task_runner) {}
DummyProducer::~DummyProducer() {}

// perfetto::Producer functions.
void DummyProducer::OnConnect() {}
void DummyProducer::OnDisconnect() {}
void DummyProducer::OnTracingSetup() {}
void DummyProducer::SetupDataSource(perfetto::DataSourceInstanceID,
                                    const perfetto::DataSourceConfig&) {}
void DummyProducer::StartDataSource(perfetto::DataSourceInstanceID,
                                    const perfetto::DataSourceConfig&) {}
void DummyProducer::StopDataSource(perfetto::DataSourceInstanceID) {}
void DummyProducer::Flush(perfetto::FlushRequestID,
                          const perfetto::DataSourceInstanceID* data_source_ids,
                          size_t num_data_sources) {}
void DummyProducer::ClearIncrementalState(
    const perfetto::DataSourceInstanceID* data_source_ids,
    size_t num_data_sources) {}

// perfetto::TracingService::ProducerEndpoint functions.
void DummyProducer::RegisterDataSource(const perfetto::DataSourceDescriptor&) {}
void DummyProducer::UnregisterDataSource(const std::string& name) {}

void DummyProducer::RegisterTraceWriter(uint32_t writer_id,
                                        uint32_t target_buffer) {}
void DummyProducer::UnregisterTraceWriter(uint32_t writer_id) {}

void DummyProducer::CommitData(const perfetto::CommitDataRequest& commit,
                               CommitDataCallback callback) {}
perfetto::SharedMemory* DummyProducer::shared_memory() const {
  return nullptr;
}
size_t DummyProducer::shared_buffer_page_size_kb() const {
  return 0;
}
perfetto::SharedMemoryArbiter* DummyProducer::GetSharedMemoryArbiter() {
  return nullptr;
}
perfetto::SharedMemoryArbiter* DummyProducer::GetInProcessShmemArbiter() {
  return nullptr;
}
void DummyProducer::NotifyFlushComplete(perfetto::FlushRequestID) {}

void DummyProducer::NotifyDataSourceStarted(perfetto::DataSourceInstanceID) {}
void DummyProducer::NotifyDataSourceStopped(perfetto::DataSourceInstanceID) {}

void DummyProducer::ActivateTriggers(const std::vector<std::string>&) {}

// tracing::PerfettoProducer functions.
void DummyProducer::NewDataSourceAdded(
    const PerfettoTracedProcess::DataSourceBase* const data_source) {}
bool DummyProducer::IsTracingActive() {
  return false;
}

// Functions expected for SystemProducer
void DummyProducer::DisconnectWithReply(
    base::OnceClosure on_disconnect_complete) {
  std::move(on_disconnect_complete).Run();
}

bool DummyProducer::IsDummySystemProducerForTesting() {
  return true;
}

void DummyProducer::ResetSequenceForTesting() {}

}  // namespace tracing
