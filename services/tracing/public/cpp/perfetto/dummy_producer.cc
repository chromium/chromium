// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/dummy_producer.h"

namespace tracing {

DummyProducer::DummyProducer(base::tracing::PerfettoTaskRunner* task_runner)
    : SystemProducer(task_runner) {}
DummyProducer::~DummyProducer() = default;

// perfetto::Producer implementation.
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
                          size_t num_data_sources,
                          perfetto::FlushFlags) {}
void DummyProducer::ClearIncrementalState(
    const perfetto::DataSourceInstanceID* data_source_ids,
    size_t num_data_sources) {}

// PerfettoProducer implementation.
perfetto::SharedMemoryArbiter* DummyProducer::MaybeSharedMemoryArbiter() {
  return nullptr;
}
bool DummyProducer::IsTracingActive() {
  return false;
}
void DummyProducer::NewDataSourceAdded(
    const PerfettoTracedProcess::DataSourceBase* const data_source) {}
bool DummyProducer::SetupSharedMemoryForStartupTracing() {
  return false;
}

// SystemProducer implementation.
void DummyProducer::ConnectToSystemService() {}
void DummyProducer::ActivateTriggers(const std::vector<std::string>&) {}
void DummyProducer::DisconnectWithReply(
    base::OnceClosure on_disconnect_complete) {
  std::move(on_disconnect_complete).Run();
}
bool DummyProducer::IsDummySystemProducerForTesting() {
  return true;
}

}  // namespace tracing
