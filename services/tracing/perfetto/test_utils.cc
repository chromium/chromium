// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "services/tracing/perfetto/test_utils.h"

#include <utility>

#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/tracing/perfetto_platform.h"
#include "services/tracing/public/cpp/perfetto/shared_memory.h"
#include "testing/gtest/include/gtest/gtest.h"
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

// static
std::unique_ptr<TestDataSource> TestDataSource::CreateAndRegisterDataSource(
    const std::string& data_source_name,
    size_t send_packet_count) {
  auto data_source = std::unique_ptr<TestDataSource>(
      new TestDataSource(data_source_name, send_packet_count));
  PerfettoTracedProcess::Get()->AddDataSource(data_source.get());
  return data_source;
}

TestDataSource::TestDataSource(const std::string& data_source_name,
                               size_t send_packet_count)
    : DataSourceBase(data_source_name), send_packet_count_(send_packet_count) {
}

TestDataSource::~TestDataSource() = default;

void TestDataSource::WritePacketBigly() {
  auto payload = base::HeapArray<char>::Uninit(kLargeMessageSize);
  std::ranges::fill(payload, '.');
  payload.as_span().back() = 0;

  std::unique_ptr<perfetto::TraceWriter> writer =
      producer_->CreateTraceWriter(config_.target_buffer());
  CHECK(writer);

  writer->NewTracePacket()->set_for_testing()->set_str(payload.data(),
                                                       payload.size());
}

void TestDataSource::StartTracingImpl(
    PerfettoProducer* producer,
    const perfetto::DataSourceConfig& data_source_config) {
  producer_ = producer;
  config_ = data_source_config;

  if (send_packet_count_ > 0) {
    std::unique_ptr<perfetto::TraceWriter> writer =
        producer_->CreateTraceWriter(config_.target_buffer());
    CHECK(writer);

    for (size_t i = 0; i < send_packet_count_; i++) {
      writer->NewTracePacket()->set_for_testing()->set_str(kPerfettoTestString);
    }
  }
  if (!start_tracing_callback_.is_null()) {
    std::move(start_tracing_callback_).Run();
  }
}

void TestDataSource::StopTracingImpl(base::OnceClosure stop_complete_callback) {
  CHECK(producer_);
  producer_ = nullptr;
  std::move(stop_complete_callback).Run();
}

void TestDataSource::Flush(base::RepeatingClosure flush_complete_callback) {
  if (flush_complete_callback) {
    flush_complete_callback.Run();
  }
}
void TestDataSource::set_start_tracing_callback(
    base::OnceClosure start_tracing_callback) {
  start_tracing_callback_ = std::move(start_tracing_callback);
}

MockProducerClient::MockProducerClient(
    uint32_t num_data_sources,
    base::OnceClosure client_enabled_callback,
    base::OnceClosure client_disabled_callback)
    : ProducerClient(PerfettoTracedProcess::Get()->GetTaskRunner()),
      num_data_sources_expected_(num_data_sources),
      client_enabled_callback_(std::move(client_enabled_callback)),
      client_disabled_callback_(std::move(client_disabled_callback)) {
  // Create SMB immediately since we never call ProducerClient's Connect().
  CHECK(InitSharedMemoryIfNeeded());
}

MockProducerClient::~MockProducerClient() = default;

// static
std::unique_ptr<MockProducerClient::Handle> MockProducerClient::Create(
    uint32_t num_data_sources,
    base::OnceClosure client_enabled_callback,
    base::OnceClosure client_disabled_callback) {
  std::unique_ptr<MockProducerClient> client(new MockProducerClient(
      num_data_sources, std::move(client_enabled_callback),
      std::move(client_disabled_callback)));
  auto handle = std::make_unique<Handle>(client.get());

  // Transfer ownership of the client to PerfettoTracedProcess.
  PerfettoTracedProcess::Get()
      ->GetTaskRunner()
      ->GetOrCreateTaskRunner()
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](std::unique_ptr<MockProducerClient> client) {
                auto* raw_client = client.get();
                raw_client->old_producer_ =
                    PerfettoTracedProcess::Get()->SetProducerClientForTesting(
                        std::move(client));
              },
              std::move(client)));
  return handle;
}

MockProducerClient::Handle::~Handle() {
  // Replace the previous client in PerfettoTracedProcess, deleting the mock
  // as a side-effect.
  PerfettoTracedProcess::Get()
      ->GetTaskRunner()
      ->GetOrCreateTaskRunner()
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](std::unique_ptr<ProducerClient> old_producer) {
                PerfettoTracedProcess::Get()->SetProducerClientForTesting(
                    std::move(old_producer));
              },
              std::move(client_->old_producer_)));
}

void MockProducerClient::SetupDataSource(const std::string& data_source_name) {}

void MockProducerClient::StartDataSource(
    uint64_t id,
    const perfetto::DataSourceConfig& data_source_config,
    StartDataSourceCallback callback) {
  ProducerClient::StartDataSource(id, std::move(data_source_config),
                                  std::move(callback));

  CHECK_LT(num_data_sources_active_, num_data_sources_expected_);
  if (++num_data_sources_active_ == num_data_sources_expected_ &&
      client_enabled_callback_) {
    std::move(client_enabled_callback_).Run();
  }
}

void MockProducerClient::StopDataSource(uint64_t id,
                                        StopDataSourceCallback callback) {
  ProducerClient::StopDataSource(id, std::move(callback));

  CHECK_GT(num_data_sources_active_, 0u);
  if (--num_data_sources_active_ == 0 && client_disabled_callback_) {
    std::move(client_disabled_callback_).Run();
  }
}

void MockProducerClient::CommitData(const perfetto::CommitDataRequest& commit,
                                    CommitDataCallback callback) {
  // Only write out commits that have actual data in it; Perfetto
  // might send two commits from different threads (one always empty),
  // which causes TSan to complain.
  if (commit.chunks_to_patch_size() || commit.chunks_to_move_size()) {
    all_client_commit_data_requests_.push_back(commit.SerializeAsString());
  }
  ProducerClient::CommitData(commit, callback);
}

void MockProducerClient::SetAgentEnabledCallback(
    base::OnceClosure client_enabled_callback) {
  client_enabled_callback_ = std::move(client_enabled_callback);
}

void MockProducerClient::SetAgentDisabledCallback(
    base::OnceClosure client_disabled_callback) {
  client_disabled_callback_ = std::move(client_disabled_callback);
}

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
    : packet_received_callback_(packet_received_callback),
      trace_config_(config) {
  for (const auto& source : data_source_names) {
    data_sources_.emplace_back(DataSourceStatus{
        source,
        perfetto::ObservableEvents::DATA_SOURCE_INSTANCE_STATE_STOPPED});
  }
  CHECK(!data_sources_.empty());
  consumer_endpoint_ = service->ConnectConsumer(this, /*uid=*/0);
  CHECK(consumer_endpoint_);
}

MockConsumer::~MockConsumer() = default;

void MockConsumer::ReadBuffers() {
  CHECK(consumer_endpoint_);
  consumer_endpoint_->ReadBuffers();
}

void MockConsumer::StopTracing() {
  ReadBuffers();
  CHECK(consumer_endpoint_);
  consumer_endpoint_->DisableTracing();
}

void MockConsumer::StartTracing() {
  CHECK(consumer_endpoint_);
  consumer_endpoint_->EnableTracing(trace_config_);
}

void MockConsumer::FreeBuffers() {
  CHECK(consumer_endpoint_);
  consumer_endpoint_->FreeBuffers();
}

void MockConsumer::OnConnect() {
  consumer_endpoint_->ObserveEvents(
      perfetto::ObservableEvents::TYPE_DATA_SOURCES_INSTANCES);
  StartTracing();
}
void MockConsumer::OnDisconnect() {}
void MockConsumer::OnTracingDisabled(const std::string& error) {}

void MockConsumer::OnTraceData(std::vector<perfetto::TracePacket> packets,
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

void MockConsumer::OnDetach(bool /*success*/) {}
void MockConsumer::OnAttach(bool /*success*/, const perfetto::TraceConfig&) {}
void MockConsumer::OnTraceStats(bool /*success*/, const perfetto::TraceStats&) {
}
void MockConsumer::OnSessionCloned(const OnSessionClonedArgs&) {}

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

MockProducerHost::MockProducerHost(
    const std::string& producer_name,
    const std::string& data_source_name,
    PerfettoService* service,
    MockProducerClient* producer_client,
    base::OnceClosure datasource_registered_callback)
    : ProducerHost(service->perfetto_task_runner()),
      producer_name_(producer_name),
      datasource_registered_callback_(
          std::move(datasource_registered_callback)) {
  mojo::PendingRemote<mojom::ProducerClient> client;
  mojo::PendingRemote<mojom::ProducerHost> host_remote;
  auto client_receiver = client.InitWithNewPipeAndPassReceiver();
  Initialize(std::move(client), service->GetService(), producer_name_,
             static_cast<ChromeBaseSharedMemory*>(
                 producer_client->shared_memory_for_testing())
                 ->CloneRegion(),
             PerfettoProducer::kSMBPageSizeBytes);
  receiver_.Bind(host_remote.InitWithNewPipeAndPassReceiver());
  producer_client->BindClientAndHostPipesForTesting(std::move(client_receiver),
                                                    std::move(host_remote));
  producer_client->SetupDataSource(data_source_name);
}

MockProducerHost::~MockProducerHost() = default;

void MockProducerHost::RegisterDataSource(
    const perfetto::DataSourceDescriptor& registration_info) {
  ProducerHost::RegisterDataSource(registration_info);

  on_commit_callback_for_testing_ =
      base::BindRepeating(&MockProducerHost::OnCommit, base::Unretained(this));

  if (datasource_registered_callback_) {
    std::move(datasource_registered_callback_).Run();
  }
}

void MockProducerHost::OnConnect() {}

void MockProducerHost::OnCommit(
    const perfetto::CommitDataRequest& commit_data_request) {
  if (!commit_data_request.chunks_to_patch_size() &&
      !commit_data_request.chunks_to_move_size()) {
    return;
  }

  all_host_commit_data_requests_.push_back(
      commit_data_request.SerializeAsString());
}

MockProducer::MockProducer(const std::string& producer_name,
                           const std::string& data_source_name,
                           PerfettoService* service,
                           base::OnceClosure on_datasource_registered,
                           base::OnceClosure on_tracing_started,
                           size_t num_packets) {
  // Construct MockProducerClient before TestDataSource to avoid a race.
  //
  // TestDataSource calls AddDataSource on the global PerfettoTracedProcess,
  // which PostTasks to the threadpool in the task it will access the
  // |producer_client_| pointer that the PerfettoTracedProcess owns. However in
  // the constructor for MockProducerClient we will set the |producer_client_|
  // from the real client to the mock, however this is done on a different
  // sequence and thus we have a race. By setting the pointer before we
  // construct the data source the TestDataSource can not race.
  producer_client_ = MockProducerClient::Create(
      /* num_data_sources = */ 1, std::move(on_tracing_started));
  data_source_ = TestDataSource::CreateAndRegisterDataSource(data_source_name,
                                                             num_packets);

  producer_host_ = std::make_unique<MockProducerHost>(
      producer_name, data_source_name, service, **producer_client_,
      std::move(on_datasource_registered));
}

MockProducer::~MockProducer() = default;

void MockProducer::WritePacketBigly(base::OnceClosure on_write_complete) {
  PerfettoTracedProcess::Get()
      ->GetTaskRunner()
      ->GetOrCreateTaskRunner()
      ->PostTaskAndReply(FROM_HERE,
                         base::BindOnce(&TestDataSource::WritePacketBigly,
                                        base::Unretained(data_source())),
                         std::move(on_write_complete));
}

TracingUnitTest::TracingUnitTest()
    : task_environment_(std::make_unique<base::test::TaskEnvironment>(
          base::test::TaskEnvironment::MainThreadType::IO)),
      tracing_environment_(std::make_unique<base::test::TracingEnvironment>(
          *task_environment_,
          base::SingleThreadTaskRunner::GetCurrentDefault())) {}

TracingUnitTest::~TracingUnitTest() {
  CHECK(setup_called_ && teardown_called_);
}

void TracingUnitTest::SetUp() {
  setup_called_ = true;

  // Also tell PerfettoTracedProcess to use the current task environment.
  test_handle_ = PerfettoTracedProcess::SetupForTesting(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  PerfettoTracedProcess::Get()->OnThreadPoolAvailable(
      /* enable_consumer */ true);

  // Wait for any posted construction tasks to execute.
  RunUntilIdle();
}

void TracingUnitTest::TearDown() {
  teardown_called_ = true;
  tracing_environment_.reset();

  // Wait for any posted destruction tasks to execute.
  RunUntilIdle();

  // From here on, no more tasks should be posted.
  task_environment_.reset();

  // Clear task runner and data sources.
  PerfettoTracedProcess::Get()->GetTaskRunner()->ResetTaskRunnerForTesting(
      nullptr);
  PerfettoTracedProcess::Get()->ClearDataSourcesForTesting();
  test_handle_.reset();
}

}  // namespace tracing
