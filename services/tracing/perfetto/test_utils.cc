// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "services/tracing/perfetto/test_utils.h"

#include <utility>

#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/client_identity.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/commit_data_request.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/protos/perfetto/trace/test_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace tracing {
namespace {

perfetto::TraceConfig GetDefaultTraceConfig(
    const std::vector<std::string>& data_sources) {
  perfetto::TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(1024 * 32);
  for (const auto& data_source : data_sources) {
    auto* ds_config = trace_config.add_data_sources()->mutable_config();
    ds_config->set_name(data_source);
    ds_config->set_target_buffer(0);
  }
  return trace_config;
}

}  // namespace

MockProducer::MockProducer() = default;
MockProducer::~MockProducer() = default;

void MockProducer::Connect(perfetto::TracingService* service,
                           const std::string& producer_name,
                           uid_t uid,
                           pid_t pid) {
  producer_name_ = producer_name;
  service_endpoint_ = service->ConnectProducer(
      this, perfetto::ClientIdentity(uid, pid), producer_name, 0,
      /*in_process=*/true,
      perfetto::TracingService::ProducerSMBScrapingMode::kDefault, 0, nullptr);
}
void MockProducer::RegisterDataSource(const std::string& name) {
  perfetto::DataSourceDescriptor ds_desc;
  ds_desc.set_name(name);
  service_endpoint_->RegisterDataSource(ds_desc);
}

void MockProducer::UnregisterDataSource(const std::string& name) {
  service_endpoint_->UnregisterDataSource(name);
}

void MockProducer::SetupDataSource(perfetto::DataSourceInstanceID ds_id,
                                   const perfetto::DataSourceConfig& cfg) {
  EXPECT_FALSE(data_source_instances_.count(ds_id));
  data_source_instances_.emplace(ds_id, cfg);
  OnSetupDataSource(cfg.name(), ds_id);
}

void MockProducer::StartDataSource(perfetto::DataSourceInstanceID ds_id,
                                   const perfetto::DataSourceConfig& cfg) {
  // The data source might have been seen already through
  // WaitForDataSourceSetup().
  if (data_source_instances_.count(ds_id) == 0) {
    data_source_instances_.emplace(ds_id, cfg);
  }
  OnStartDataSource(cfg.name(), ds_id);
}

void MockProducer::StopDataSource(perfetto::DataSourceInstanceID ds_id) {
  ASSERT_EQ(1u, data_source_instances_.count(ds_id));
  std::string name = data_source_instances_[ds_id].name();
  data_source_instances_.erase(ds_id);
  OnStopDataSource(name, ds_id);
}

std::unique_ptr<perfetto::TraceWriter> MockProducer::CreateTraceWriter(
    perfetto::DataSourceInstanceID ds_id,
    perfetto::BufferExhaustedPolicy buffer_exhausted_policy) {
  PERFETTO_DCHECK(data_source_instances_.count(ds_id));
  perfetto::BufferID buf_id = static_cast<perfetto::BufferID>(
      data_source_instances_[ds_id].target_buffer());
  return service_endpoint_->CreateTraceWriter(buf_id, buffer_exhausted_policy);
}

perfetto::DataSourceConfig MockProducer::GetDataSourceConfig(
    perfetto::DataSourceInstanceID ds_id) {
  PERFETTO_DCHECK(data_source_instances_.count(ds_id));
  return data_source_instances_.at(ds_id);
}

void MockProducer::WritePackets(perfetto::TraceWriter& writer,
                                size_t num_packets) {
  for (size_t i = 0; i < 10; i++) {
    auto tp = writer.NewTracePacket();
    tp->set_for_testing()->set_str(kPerfettoTestString);
  }
}

void MockProducer::WritePacketBigly(perfetto::TraceWriter& writer) {
  auto payload = base::HeapArray<char>::Uninit(kLargeMessageSize);
  std::ranges::fill(payload, '.');
  payload.as_span().back() = 0;

  writer.NewTracePacket()->set_for_testing()->set_str(payload.data(),
                                                      payload.size());
}

void MockProducer::Flush(perfetto::FlushRequestID flush_req_id,
                         const perfetto::DataSourceInstanceID* ds_id,
                         size_t,
                         perfetto::FlushFlags) {
  OnFlush();
  service_endpoint_->NotifyFlushComplete(flush_req_id);
}

MockConsumerBase::MockConsumerBase(
    perfetto::TracingService* service,
    PacketReceivedCallback packet_received_callback)
    : packet_received_callback_(packet_received_callback) {
  consumer_endpoint_ = service->ConnectConsumer(this, /*uid=*/0);
  CHECK(consumer_endpoint_);
}

MockConsumerBase::~MockConsumerBase() = default;

void MockConsumerBase::ReadBuffers() {
  CHECK(consumer_endpoint_);
  consumer_endpoint_->ReadBuffers();
}

void MockConsumerBase::StopTracing() {
  ReadBuffers();
  CHECK(consumer_endpoint_);
  consumer_endpoint_->DisableTracing();
}

void MockConsumerBase::CloneSession(const std::string& unique_session_name) {
  CHECK(consumer_endpoint_);
  perfetto::TracingService::ConsumerEndpoint::CloneSessionArgs args;
  args.unique_session_name = unique_session_name;
  consumer_endpoint_->CloneSession(std::move(args));
}

void MockConsumerBase::StartTracing(const perfetto::TraceConfig& trace_config) {
  CHECK(consumer_endpoint_);
  consumer_endpoint_->EnableTracing(trace_config);
}

void MockConsumerBase::FreeBuffers() {
  CHECK(consumer_endpoint_);
  consumer_endpoint_->FreeBuffers();
}

void MockConsumerBase::OnConnect() {}
void MockConsumerBase::OnDisconnect() {}
void MockConsumerBase::OnTracingDisabled(const std::string& error) {}

void MockConsumerBase::OnTraceData(std::vector<perfetto::TracePacket> packets,
                                   bool has_more) {
  for (auto& encoded_packet : packets) {
    perfetto::protos::TracePacket packet;
    EXPECT_TRUE(packet.ParseFromString(encoded_packet.GetRawBytesForTesting()));
    ++received_packets_;
    if (packet.for_testing().str() == kPerfettoTestString) {
      ++received_test_packets_;
    }
  }

  packet_received_callback_(has_more);
}

void MockConsumerBase::OnDetach(bool /*success*/) {}
void MockConsumerBase::OnAttach(bool /*success*/,
                                const perfetto::TraceConfig&) {}
void MockConsumerBase::OnTraceStats(bool /*success*/,
                                    const perfetto::TraceStats&) {}
void MockConsumerBase::OnObservableEvents(
    const perfetto::ObservableEvents& events) {}
void MockConsumerBase::OnSessionCloned(const OnSessionClonedArgs&) {}

MockConsumer::MockConsumer(std::vector<std::string> data_source_names,
                           perfetto::TracingService* service,
                           PacketReceivedCallback packet_received_callback)
    : MockConsumer(data_source_names,
                   service,
                   std::move(packet_received_callback),
                   GetDefaultTraceConfig(data_source_names)) {}

MockConsumer::MockConsumer(std::vector<std::string> data_source_names,
                           perfetto::TracingService* service,
                           PacketReceivedCallback packet_received_callback,
                           const perfetto::TraceConfig& config)
    : MockConsumerBase(service, std::move(packet_received_callback)),
      trace_config_(config) {
  for (const auto& source : data_source_names) {
    data_sources_.emplace_back(DataSourceStatus{
        source,
        perfetto::ObservableEvents::DATA_SOURCE_INSTANCE_STATE_STOPPED});
  }
  CHECK(!data_sources_.empty());
}

MockConsumer::~MockConsumer() = default;

void MockConsumer::StartTracing() {
  MockConsumerBase::StartTracing(trace_config_);
}

void MockConsumer::OnConnect() {
  consumer_endpoint_->ObserveEvents(
      perfetto::ObservableEvents::TYPE_DATA_SOURCES_INSTANCES);
  StartTracing();
}

void MockConsumer::OnObservableEvents(
    const perfetto::ObservableEvents& events) {
  for (const auto& change : events.instance_state_changes()) {
    for (auto& data_source_status : data_sources_) {
      if (change.data_source_name() != data_source_status.name) {
        continue;
      }
      data_source_status.state = change.state();
    }
    CheckForAllDataSourcesStarted();
    CheckForAllDataSourcesStopped();
  }
}

void MockConsumer::WaitForAllDataSourcesStarted() {
  base::RunLoop on_started;
  on_started_runloop_ = &on_started;
  CheckForAllDataSourcesStarted();
  if (on_started_runloop_) {
    on_started_runloop_->Run();
  }
}

void MockConsumer::WaitForAllDataSourcesStopped() {
  base::RunLoop on_stopped;
  on_stopped_runloop_ = &on_stopped;
  CheckForAllDataSourcesStopped();
  if (on_stopped_runloop_) {
    on_stopped_runloop_->Run();
  }
}

void MockConsumer::CheckForAllDataSourcesStarted() {
  for (auto& data_source_status : data_sources_) {
    if (data_source_status.state !=
        perfetto::ObservableEvents::DATA_SOURCE_INSTANCE_STATE_STARTED) {
      return;
    }
  }
  if (on_started_runloop_) {
    on_started_runloop_->Quit();
    on_started_runloop_ = nullptr;
  }
}

void MockConsumer::CheckForAllDataSourcesStopped() {
  for (auto& data_source_status : data_sources_) {
    if (data_source_status.state !=
        perfetto::ObservableEvents::DATA_SOURCE_INSTANCE_STATE_STOPPED) {
      return;
    }
  }
  if (on_stopped_runloop_) {
    on_stopped_runloop_->Quit();
    on_stopped_runloop_ = nullptr;
  }
}

TracedProcessForTesting::TracedProcessForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  PerfettoTracedProcess::MaybeCreateInstanceForTesting().SetupForTesting(
      task_runner);
}

TracedProcessForTesting::~TracedProcessForTesting() {
  base::RunLoop().RunUntilIdle();
  PerfettoTracedProcess::Get().ResetForTesting();
}

}  // namespace tracing
