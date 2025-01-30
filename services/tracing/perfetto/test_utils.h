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
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/consumer.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/producer.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/protos/perfetto/common/observable_events.pb.h"

namespace base {
class RunLoop;
}

namespace tracing {

const char kPerfettoTestString[] = "d00df00d";
const size_t kLargeMessageSize = 1 * 1024 * 1024;

// third_party/perfetto/src/tracing/test/mock_producer.h
class MockProducer : public perfetto::Producer {
 public:
  MockProducer();
  ~MockProducer() override;

  inline void Connect(PerfettoService* service,
                      const std::string& producer_name) {
    Connect(service->GetService(), producer_name);
  }
  void Connect(perfetto::TracingService* service,
               const std::string& producer_name,
               uid_t uid = 42,
               pid_t pid = 1025);
  void RegisterDataSource(const std::string& name);
  void UnregisterDataSource(const std::string& name);

  perfetto::TracingService::ProducerEndpoint* endpoint() {
    return service_endpoint_.get();
  }

  std::unique_ptr<perfetto::TraceWriter> CreateTraceWriter(
      perfetto::DataSourceInstanceID,
      perfetto::BufferExhaustedPolicy buffer_exhausted_policy =
          perfetto::BufferExhaustedPolicy::kDefault);

  perfetto::DataSourceConfig GetDataSourceConfig(
      perfetto::DataSourceInstanceID);

  static void WritePackets(perfetto::TraceWriter&, size_t num_packets);
  static void WritePacketBigly(perfetto::TraceWriter&);

  MOCK_METHOD(void,
              OnSetupDataSource,
              (const std::string& name, perfetto::DataSourceInstanceID),
              ());
  MOCK_METHOD(void,
              OnStartDataSource,
              (const std::string& name, perfetto::DataSourceInstanceID),
              ());
  MOCK_METHOD(void,
              OnStopDataSource,
              (const std::string& name, perfetto::DataSourceInstanceID),
              ());
  MOCK_METHOD(void, OnFlush, (), ());

  // perfetto::Producer
  MOCK_METHOD(void, OnConnect, (), (override));
  MOCK_METHOD(void, OnDisconnect, (), (override));
  MOCK_METHOD(void, OnTracingSetup, (), (override));
  void SetupDataSource(perfetto::DataSourceInstanceID,
                       const perfetto::DataSourceConfig&) override;
  void StartDataSource(perfetto::DataSourceInstanceID,
                       const perfetto::DataSourceConfig&) override;
  void StopDataSource(perfetto::DataSourceInstanceID) override;
  void Flush(perfetto::FlushRequestID,
             const perfetto::DataSourceInstanceID*,
             size_t,
             perfetto::FlushFlags) override;
  MOCK_METHOD(void,
              ClearIncrementalState,
              (const perfetto::DataSourceInstanceID*, size_t),
              (override));

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::string producer_name_;
  std::unique_ptr<perfetto::TracingService::ProducerEndpoint> service_endpoint_;
  std::map<perfetto::DataSourceInstanceID, perfetto::DataSourceConfig>
      data_source_instances_;
};

class MockConsumerBase : public perfetto::Consumer {
 public:
  using PacketReceivedCallback = std::function<void(bool)>;
  MockConsumerBase(perfetto::TracingService* service,
                   PacketReceivedCallback packet_received_callback);
  ~MockConsumerBase() override;

  void ReadBuffers();

  void FreeBuffers();

  void StartTracing(const perfetto::TraceConfig& trace_config);

  void StopTracing();

  void CloneSession(const std::string& unique_session_name);

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

 protected:
  std::unique_ptr<perfetto::TracingService::ConsumerEndpoint>
      consumer_endpoint_;
  size_t received_packets_ = 0;
  size_t received_test_packets_ = 0;
  PacketReceivedCallback packet_received_callback_;
};

class MockConsumer : public MockConsumerBase {
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

  void StartTracing();

  // perfetto::Consumer implementation
  void OnConnect() override;
  void OnObservableEvents(const perfetto::ObservableEvents&) override;

  void WaitForAllDataSourcesStarted();
  void WaitForAllDataSourcesStopped();

 private:
  struct DataSourceStatus {
    std::string name;
    perfetto::ObservableEvents::DataSourceInstanceState state;
  };

  void CheckForAllDataSourcesStarted();
  void CheckForAllDataSourcesStopped();

  std::vector<DataSourceStatus> data_sources_;
  raw_ptr<base::RunLoop> on_started_runloop_ = nullptr;
  raw_ptr<base::RunLoop> on_stopped_runloop_ = nullptr;
  perfetto::TraceConfig trace_config_;
};

class TracedProcessForTesting {
 public:
  explicit TracedProcessForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~TracedProcessForTesting();
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_TEST_UTILS_H_
