// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PERFETTO_TEST_UTILS_H_
#define SERVICES_TRACING_PERFETTO_TEST_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/trace_test_utils.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/perfetto/producer_host.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/consumer.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/protos/perfetto/common/observable_events.pb.h"

namespace base {
class RunLoop;
}

namespace tracing {

const char kPerfettoTestString[] = "d00df00d";
const size_t kLargeMessageSize = 1 * 1024 * 1024;

class TestDataSource : public PerfettoTracedProcess::DataSourceBase {
 public:
  static std::unique_ptr<TestDataSource> CreateAndRegisterDataSource(
      const std::string& data_source_name,
      size_t send_packet_count);
  ~TestDataSource() override;

  void WritePacketBigly();

  // DataSourceBase implementation
  void StartTracingImpl(
      PerfettoProducer* producer,
      const perfetto::DataSourceConfig& data_source_config) override;
  void StopTracingImpl(
      base::OnceClosure stop_complete_callback = base::OnceClosure()) override;
  void Flush(base::RepeatingClosure flush_complete_callback) override;

  const perfetto::DataSourceConfig& config() { return config_; }

  void set_send_packet_count(size_t count) { send_packet_count_ = count; }

  void set_start_tracing_callback(base::OnceClosure start_tracing_callback);

 private:
  TestDataSource(const std::string& data_source_name, size_t send_packet_count);

  size_t send_packet_count_;
  raw_ptr<tracing::PerfettoProducer, AcrossTasksDanglingUntriaged> producer_ =
      nullptr;
  perfetto::DataSourceConfig config_;
  base::OnceClosure start_tracing_callback_ = base::OnceClosure();
};

// This class is owned by PerfettoTracedProcess, and its lifetime is indirectly
// controlled by the handle returned from Create().
class MockProducerClient : public ProducerClient {
 public:
  class Handle {
   public:
    explicit Handle(MockProducerClient* client) : client_(client) {}
    ~Handle();

    MockProducerClient* operator->() { return client_; }
    MockProducerClient* operator*() { return client_; }

   private:
    const raw_ptr<MockProducerClient, AcrossTasksDanglingUntriaged> client_;
  };

  ~MockProducerClient() override;

  static std::unique_ptr<Handle> Create(
      uint32_t num_data_sources = 0,
      base::OnceClosure client_enabled_callback = base::OnceClosure(),
      base::OnceClosure client_disabled_callback = base::OnceClosure());

  void SetupDataSource(const std::string& data_source_name);

  void StartDataSource(uint64_t id,
                       const perfetto::DataSourceConfig& data_source_config,
                       StartDataSourceCallback callback) override;

  void StopDataSource(uint64_t id, StopDataSourceCallback callback) override;

  void CommitData(const perfetto::CommitDataRequest& commit,
                  CommitDataCallback callback = {}) override;

  void SetAgentEnabledCallback(base::OnceClosure client_enabled_callback);

  void SetAgentDisabledCallback(base::OnceClosure client_disabled_callback);

  const std::vector<std::string>& all_client_commit_data_requests() const {
    return all_client_commit_data_requests_;
  }

 private:
  MockProducerClient(uint32_t num_data_sources,
                     base::OnceClosure client_enabled_callback,
                     base::OnceClosure client_disabled_callback);

  uint32_t num_data_sources_active_ = 0;
  uint32_t num_data_sources_expected_;
  base::OnceClosure client_enabled_callback_;
  base::OnceClosure client_disabled_callback_;
  std::vector<std::string> all_client_commit_data_requests_;
  std::unique_ptr<ProducerClient> old_producer_;
};

class MockConsumer : public perfetto::Consumer {
 public:
  using PacketReceivedCallback = std::function<void(bool)>;
  MockConsumer(std::vector<std::string> data_source_names,
               perfetto::TracingService* service,
               PacketReceivedCallback packet_received_callback);
  MockConsumer(std::vector<std::string> data_source_names,
               perfetto::TracingService* service,
               PacketReceivedCallback packet_received_callback,
               const perfetto::TraceConfig& config);
  ~MockConsumer() override;

  void ReadBuffers();

  void StopTracing();

  void StartTracing();

  void FreeBuffers();

  size_t received_packets() const { return received_packets_; }
  size_t received_test_packets() const { return received_test_packets_; }

  // perfetto::Consumer implementation
  void OnConnect() override;
  void OnDisconnect() override;
  void OnTracingDisabled(const std::string& error) override;

  void OnTraceData(std::vector<perfetto::TracePacket> packets,
                   bool has_more) override;
  void OnDetach(bool success) override;
  void OnAttach(bool success, const perfetto::TraceConfig&) override;
  void OnTraceStats(bool success, const perfetto::TraceStats&) override;
  void OnObservableEvents(const perfetto::ObservableEvents&) override;
  void OnSessionCloned(const OnSessionClonedArgs&) override;

  void WaitForAllDataSourcesStarted();
  void WaitForAllDataSourcesStopped();

 private:
  struct DataSourceStatus {
    std::string name;
    perfetto::ObservableEvents::DataSourceInstanceState state;
  };

  void CheckForAllDataSourcesStarted();
  void CheckForAllDataSourcesStopped();

  std::unique_ptr<perfetto::TracingService::ConsumerEndpoint>
      consumer_endpoint_;
  size_t received_packets_ = 0;
  size_t received_test_packets_ = 0;
  PacketReceivedCallback packet_received_callback_;
  std::vector<DataSourceStatus> data_sources_;
  raw_ptr<base::RunLoop> on_started_runloop_ = nullptr;
  raw_ptr<base::RunLoop> on_stopped_runloop_ = nullptr;
  perfetto::TraceConfig trace_config_;
};

class MockProducerHost : public ProducerHost {
 public:
  MockProducerHost(
      const std::string& producer_name,
      const std::string& data_source_name,
      PerfettoService* service,
      MockProducerClient* producer_client,
      base::OnceClosure datasource_registered_callback = base::OnceClosure());
  ~MockProducerHost() override;

  void RegisterDataSource(
      const perfetto::DataSourceDescriptor& registration_info) override;

  void OnConnect() override;

  void OnCommit(const perfetto::CommitDataRequest& commit_data_request);

  const std::vector<std::string>& all_host_commit_data_requests() const {
    return all_host_commit_data_requests_;
  }

 protected:
  const std::string producer_name_;
  base::OnceClosure datasource_registered_callback_;
  std::vector<std::string> all_host_commit_data_requests_;
  mojo::Receiver<mojom::ProducerHost> receiver_{this};
};

class MockProducer {
 public:
  MockProducer(const std::string& producer_name,
               const std::string& data_source_name,
               PerfettoService* service,
               base::OnceClosure on_datasource_registered,
               base::OnceClosure on_tracing_started,
               size_t num_packets = 10);
  virtual ~MockProducer();

  void WritePacketBigly(base::OnceClosure on_write_complete);

  MockProducerClient* producer_client() { return **producer_client_; }

  TestDataSource* data_source() { return data_source_.get(); }

 private:
  std::unique_ptr<TestDataSource> data_source_;
  std::unique_ptr<MockProducerClient::Handle> producer_client_;
  std::unique_ptr<MockProducerHost> producer_host_;
};

// Base class for various tracing unit tests, ensuring cleanup of
// PerfettoTracedProcess. Tracing tasks are run on the test thread.
class TracingUnitTest : public testing::Test {
 public:
  TracingUnitTest();
  ~TracingUnitTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  void RunUntilIdle() { task_environment_->RunUntilIdle(); }

 private:
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  std::unique_ptr<base::test::TracingEnvironment> tracing_environment_;
  std::unique_ptr<PerfettoTracedProcess::TestHandle> test_handle_;
  bool setup_called_ = false;
  bool teardown_called_ = false;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_TEST_UTILS_H_
