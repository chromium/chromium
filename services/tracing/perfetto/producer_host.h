// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PERFETTO_PRODUCER_HOST_H_
#define SERVICES_TRACING_PERFETTO_PRODUCER_HOST_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/perfetto/producer_host.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/producer.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/tracing_service.h"

namespace perfetto {
class CommitDataRequest;
}  // namespace perfetto

namespace tracing {

// This class is the service-side part of the Perfetto Producer pair
// and is responsible for registering any available DataSources
// with Perfetto (like ChromeTracing) in OnConnect(). It will forward
// control messages from Perfetto to its per-process ProducerClient
// counterpart, like starting tracing with a specific shared memory buffer,
// create/teardown instances of specific data sources, etc.
// It's managed by PerfettoService which is responsible for
// creating a ProducerHost when a ProducerClient registers itself
// and connects them together.
class ProducerHost : public tracing::mojom::ProducerHost,
                     public perfetto::Producer {
 public:
  ProducerHost();
  ~ProducerHost() override;

  // Called by the ProducerService to register the
  // Producer with Perfetto and connect to the
  // corresponding remote ProducerClient.
  void Initialize(mojo::PendingRemote<mojom::ProducerClient> producer_client,
                  perfetto::TracingService* service,
                  const std::string& name);

  // perfetto::Producer implementation.
  // Gets called by perfetto::TracingService to toggle specific data sources
  // when requested by a Perfetto Consumer.
  void OnConnect() override;
  void OnDisconnect() override;

  void SetupDataSource(perfetto::DataSourceInstanceID id,
                       const perfetto::DataSourceConfig& config) override;

  void StartDataSource(perfetto::DataSourceInstanceID id,
                       const perfetto::DataSourceConfig& config) override;

  void StopDataSource(perfetto::DataSourceInstanceID) override;
  void OnTracingSetup() override;
  void Flush(perfetto::FlushRequestID,
             const perfetto::DataSourceInstanceID* raw_data_source_ids,
             size_t num_data_sources) override;
  void ClearIncrementalState(
      const perfetto::DataSourceInstanceID* data_source_ids,
      size_t num_data_sources) override;

  // mojom::ProducerHost implementation.
  // This interface gets called by the per-process ProducerClients
  // to signal that there's changes to be committed to the
  // Shared Memory buffer (like finished chunks).
  void CommitData(const perfetto::CommitDataRequest& data_request,
                  CommitDataCallback callback) override;

  // Called by the ProducerClient to signal the Host that it can
  // provide a specific data source.
  void RegisterDataSource(
      const perfetto::DataSourceDescriptor& registration_info) override;

  // Called by the ProducerClient to associate a TraceWriter with a target
  // buffer, which is required to support scraping of the SMB by the service.
  void RegisterTraceWriter(uint32_t writer_id, uint32_t target_buffer) override;
  void UnregisterTraceWriter(uint32_t writer_id) override;

 protected:

  base::RepeatingCallback<void(const perfetto::CommitDataRequest&)>
      on_commit_callback_for_testing_;

 private:
  mojo::Remote<mojom::ProducerClient> producer_client_;
  bool is_in_process_ = false;

 protected:
  // Perfetto guarantees that no OnXX callbacks are invoked on |this|
  // immediately after |producer_endpoint_| is destroyed.
  std::unique_ptr<perfetto::TracingService::ProducerEndpoint>
      producer_endpoint_;

  DISALLOW_COPY_AND_ASSIGN(ProducerHost);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_PRODUCER_HOST_H_
