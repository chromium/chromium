// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_DUMMY_PRODUCER_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_DUMMY_PRODUCER_H_

#include "services/tracing/public/cpp/perfetto/system_producer.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/producer.h"

namespace tracing {

class COMPONENT_EXPORT(TRACING_CPP) DummyProducer : public SystemProducer {
 public:
  explicit DummyProducer(base::tracing::PerfettoTaskRunner*);
  ~DummyProducer() override;

  // perfetto::Producer implementation.
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
             size_t num_data_sources,
             perfetto::FlushFlags) override;
  void ClearIncrementalState(
      const perfetto::DataSourceInstanceID* data_source_ids,
      size_t num_data_sources) override;

  // PerfettoProducer implementation.
  perfetto::SharedMemoryArbiter* MaybeSharedMemoryArbiter() override;
  bool IsTracingActive() override;
  void NewDataSourceAdded(
      const PerfettoTracedProcess::DataSourceBase* const data_source) override;

  // SystemProducer implementation.
  void ConnectToSystemService() override;
  void ActivateTriggers(const std::vector<std::string>& triggers) override;
  void DisconnectWithReply(base::OnceClosure on_disconnect_complete) override;
  bool IsDummySystemProducerForTesting() override;

 protected:
  // perfetto::Producer implementation.
  bool SetupSharedMemoryForStartupTracing() override;
};
}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_DUMMY_PRODUCER_H_
