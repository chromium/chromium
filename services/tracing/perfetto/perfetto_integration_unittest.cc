// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/perfetto/producer_host.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/tracing/core/commit_data_request.h"
#include "third_party/perfetto/include/perfetto/tracing/core/consumer.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_descriptor.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_packet.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/tracing/core/tracing_service.h"
#include "third_party/perfetto/protos/perfetto/common/commit_data_request.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/test_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace tracing {

namespace {

const char kPerfettoTestDataSourceName[] =
    "org.chromium.chrome_integration_unittest";
const char kPerfettoProducerName[] = "chrome_producer_test";
const char kPerfettoTestString[] = "d00df00d";

class PerfettoIntegrationTest : public testing::Test {
 public:
  void SetUp() override {
    perfetto_service_ = std::make_unique<PerfettoService>(
        base::SequencedTaskRunnerHandle::Get());
    // The actual Perfetto service is created async on the given task_runner;
    // wait until that's done.
    RunUntilIdle();
    ProducerClient::ResetTaskRunnerForTesting();
  }

  void TearDown() override { perfetto_service_.reset(); }

  PerfettoService* perfetto_service() const { return perfetto_service_.get(); }

  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

 private:
  std::unique_ptr<PerfettoService> perfetto_service_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

class TestDataSource {
 public:
  TestDataSource(ProducerClient* producer_client,
                 size_t send_packet_count,
                 const std::string& trace_config,
                 uint32_t target_buffer)
      : producer_client_(producer_client),
        send_packet_count_(send_packet_count),
        target_buffer_(target_buffer) {
    if (send_packet_count_ > 0) {
      std::unique_ptr<perfetto::TraceWriter> writer =
          producer_client_->CreateTraceWriter(target_buffer);
      CHECK(writer);

      for (size_t i = 0; i < send_packet_count_; i++) {
        writer->NewTracePacket()->set_for_testing()->set_str(
            kPerfettoTestString);
      }
    }
  }

  void WritePacketBigly() {
    const size_t kMessageSize = 10 * 1024;
    std::unique_ptr<char[]> payload(new char[kMessageSize]);
    memset(payload.get(), '.', kMessageSize);
    payload.get()[kMessageSize - 1] = 0;

    std::unique_ptr<perfetto::TraceWriter> writer =
        producer_client_->CreateTraceWriter(target_buffer_);
    CHECK(writer);

    writer->NewTracePacket()->set_for_testing()->set_str(payload.get(),
                                                         kMessageSize);
  }

 private:
  ProducerClient* producer_client_;
  const size_t send_packet_count_;
  uint32_t target_buffer_;
};

class MockProducerClient : public ProducerClient {
 public:
  MockProducerClient(
      size_t send_packet_count,
      base::OnceClosure client_enabled_callback = base::OnceClosure(),
      base::OnceClosure client_disabled_callback = base::OnceClosure())
      : client_enabled_callback_(std::move(client_enabled_callback)),
        client_disabled_callback_(std::move(client_disabled_callback)),
        send_packet_count_(send_packet_count) {}

  size_t send_packet_count() const { return send_packet_count_; }

  void StartDataSource(uint64_t id,
                       mojom::DataSourceConfigPtr data_source_config) override {
    enabled_data_source_ = std::make_unique<TestDataSource>(
        this, send_packet_count_, data_source_config->trace_config,
        data_source_config->target_buffer);

    if (client_enabled_callback_) {
      std::move(client_enabled_callback_).Run();
    }
  }

  void StopDataSource(uint64_t id, StopDataSourceCallback callback) override {
    enabled_data_source_.reset();

    if (client_disabled_callback_) {
      std::move(client_disabled_callback_).Run();
    }

    std::move(callback).Run();
  }

  void CommitData(const perfetto::CommitDataRequest& commit,
                  CommitDataCallback callback = {}) override {
    // Only write out commits that have actual data in it; Perfetto
    // might send two commits from different threads (one always empty),
    // which causes TSan to complain.
    if (commit.chunks_to_patch_size() || commit.chunks_to_move_size()) {
      perfetto::protos::CommitDataRequest proto;
      commit.ToProto(&proto);
      std::string proto_string;
      CHECK(proto.SerializeToString(&proto_string));
      all_client_commit_data_requests_ += proto_string;
    }
    ProducerClient::CommitData(commit, callback);
  }

  void SetAgentEnabledCallback(base::OnceClosure client_enabled_callback) {
    client_enabled_callback_ = std::move(client_enabled_callback);
  }

  void SetAgentDisabledCallback(base::OnceClosure client_disabled_callback) {
    client_disabled_callback_ = std::move(client_disabled_callback);
  }

  const std::string& all_client_commit_data_requests() const {
    return all_client_commit_data_requests_;
  }

  TestDataSource* data_source() { return enabled_data_source_.get(); }

 private:
  base::OnceClosure client_enabled_callback_;
  base::OnceClosure client_disabled_callback_;
  const size_t send_packet_count_;

  std::string all_client_commit_data_requests_;
  std::unique_ptr<TestDataSource> enabled_data_source_;
};

class MockConsumer : public perfetto::Consumer {
 public:
  using PacketReceivedCallback = std::function<void(bool)>;
  MockConsumer(perfetto::TracingService* service,
               std::string data_source_name,
               PacketReceivedCallback packet_received_callback)
      : packet_received_callback_(packet_received_callback),
        data_source_name_(data_source_name) {
    consumer_endpoint_ = service->ConnectConsumer(this);
  }

  void ReadBuffers() { consumer_endpoint_->ReadBuffers(); }

  void StopTracing() {
    ReadBuffers();
    consumer_endpoint_->DisableTracing();
  }

  void StartTracing() {
    perfetto::TraceConfig trace_config;
    trace_config.add_buffers()->set_size_kb(4096 * 100);
    auto* ds_config = trace_config.add_data_sources()->mutable_config();
    ds_config->set_name(data_source_name_);
    ds_config->set_target_buffer(0);

    consumer_endpoint_->EnableTracing(trace_config);
  }

  void FreeBuffers() { consumer_endpoint_->FreeBuffers(); }

  size_t received_packets() const { return received_packets_; }

  // perfetto::Consumer implementation
  void OnConnect() override { StartTracing(); }
  void OnDisconnect() override {}
  void OnTracingDisabled() override {}

  void OnTraceData(std::vector<perfetto::TracePacket> packets,
                   bool has_more) override {
    for (auto& encoded_packet : packets) {
      perfetto::protos::TracePacket packet;
      EXPECT_TRUE(encoded_packet.Decode(&packet));
      if (packet.for_testing().str() == kPerfettoTestString) {
        received_packets_++;
      }
    }

    packet_received_callback_(has_more);
  }

 private:
  std::unique_ptr<perfetto::TracingService::ConsumerEndpoint>
      consumer_endpoint_;
  size_t received_packets_ = 0;
  PacketReceivedCallback packet_received_callback_;
  std::string data_source_name_;
};

class MockProducer : public ProducerHost {
 public:
  MockProducer(
      std::string data_source_name,
      base::OnceClosure datasource_registered_callback = base::OnceClosure())
      : datasource_registered_callback_(
            std::move(datasource_registered_callback)),
        data_source_name_(data_source_name) {}

  void OnConnect() override {
    on_commit_callback_for_testing_ =
        base::BindRepeating(&MockProducer::OnCommit, base::Unretained(this));

    perfetto::DataSourceDescriptor descriptor;
    descriptor.set_name(data_source_name_);
    producer_endpoint_->RegisterDataSource(descriptor);

    if (datasource_registered_callback_) {
      std::move(datasource_registered_callback_).Run();
    }
  }

  void OnCommit(const perfetto::CommitDataRequest& commit_data_request) {
    if (!commit_data_request.chunks_to_patch_size() &&
        !commit_data_request.chunks_to_move_size()) {
      return;
    }

    perfetto::protos::CommitDataRequest proto;
    commit_data_request.ToProto(&proto);
    std::string proto_string;
    CHECK(proto.SerializeToString(&proto_string));
    all_host_commit_data_requests_ += proto_string;
  }

  void OnMessagepipesReadyCallback(
      perfetto::TracingService* perfetto_service,
      mojom::ProducerClientPtr producer_client_pipe,
      mojom::ProducerHostRequest producer_host_pipe) {
    Initialize(std::move(producer_client_pipe), std::move(producer_host_pipe),
               perfetto_service, kPerfettoProducerName);
  }

  const std::string& all_host_commit_data_requests() const {
    return all_host_commit_data_requests_;
  }

 protected:
  base::OnceClosure datasource_registered_callback_;
  const std::string data_source_name_;
  std::string all_host_commit_data_requests_;
};

TEST_F(PerfettoIntegrationTest, ProducerDatasourceInitialized) {
  auto dummy_client =
      std::make_unique<MockProducerClient>(0 /* send_packet_count */);

  base::RunLoop producer_initialized_runloop;
  auto new_producer = std::make_unique<MockProducer>(
      kPerfettoTestDataSourceName, producer_initialized_runloop.QuitClosure());
  dummy_client->CreateMojoMessagepipes(
      base::BindOnce(&MockProducer::OnMessagepipesReadyCallback,
                     base::Unretained(new_producer.get()),
                     base::Unretained(perfetto_service()->GetService())));

  producer_initialized_runloop.Run();

  ProducerClient::DeleteSoonForTesting(std::move(dummy_client));
}

TEST_F(PerfettoIntegrationTest, ClientEnabledAndDisabled) {
  base::RunLoop on_trace_packets;
  MockConsumer consumer(perfetto_service()->GetService(),
                        kPerfettoTestDataSourceName,
                        [&on_trace_packets](bool has_more) {
                          EXPECT_FALSE(has_more);
                          on_trace_packets.Quit();
                        });

  base::RunLoop client_enabled_callback;
  base::RunLoop client_disabled_callback;
  auto client = std::make_unique<MockProducerClient>(
      0 /* send_packet_count */, client_enabled_callback.QuitClosure(),
      client_disabled_callback.QuitClosure());

  auto producer = std::make_unique<MockProducer>(kPerfettoTestDataSourceName);
  client->CreateMojoMessagepipes(
      base::BindOnce(&MockProducer::OnMessagepipesReadyCallback,
                     base::Unretained(producer.get()),
                     base::Unretained(perfetto_service()->GetService())));

  client_enabled_callback.Run();

  RunUntilIdle();

  consumer.StopTracing();

  client_disabled_callback.Run();

  on_trace_packets.Run();
  EXPECT_EQ(0u, consumer.received_packets());

  ProducerClient::DeleteSoonForTesting(std::move(client));
}

TEST_F(PerfettoIntegrationTest, PacketsEndToEndProducerFirst) {
  const size_t kNumPackets = 10;

  base::RunLoop client_enabled_callback;
  base::RunLoop client_disabled_callback;
  auto client = std::make_unique<MockProducerClient>(
      kNumPackets, client_enabled_callback.QuitClosure(),
      client_disabled_callback.QuitClosure());

  auto producer = std::make_unique<MockProducer>(kPerfettoTestDataSourceName);
  client->CreateMojoMessagepipes(
      base::BindOnce(&MockProducer::OnMessagepipesReadyCallback,
                     base::Unretained(producer.get()),
                     base::Unretained(perfetto_service()->GetService())));

  base::RunLoop no_more_packets_runloop;
  MockConsumer consumer(perfetto_service()->GetService(),
                        kPerfettoTestDataSourceName,
                        [&no_more_packets_runloop](bool has_more) {
                          if (!has_more) {
                            no_more_packets_runloop.Quit();
                          }
                        });

  client_enabled_callback.Run();

  RunUntilIdle();

  consumer.StopTracing();
  client_disabled_callback.Run();

  no_more_packets_runloop.Run();

  EXPECT_EQ(kNumPackets, consumer.received_packets());

  ProducerClient::DeleteSoonForTesting(std::move(client));
}

TEST_F(PerfettoIntegrationTest, PacketsEndToEndConsumerFirst) {
  const size_t kNumPackets = 10;

  base::RunLoop no_more_packets_runloop;
  MockConsumer consumer(perfetto_service()->GetService(),
                        kPerfettoTestDataSourceName,
                        [&no_more_packets_runloop](bool has_more) {
                          if (!has_more) {
                            no_more_packets_runloop.Quit();
                          }
                        });

  base::RunLoop client_enabled_callback;
  auto client = std::make_unique<MockProducerClient>(
      kNumPackets, client_enabled_callback.QuitClosure());

  auto new_producer =
      std::make_unique<MockProducer>(kPerfettoTestDataSourceName);
  client->CreateMojoMessagepipes(
      base::BindOnce(&MockProducer::OnMessagepipesReadyCallback,
                     base::Unretained(new_producer.get()),
                     base::Unretained(perfetto_service()->GetService())));

  client_enabled_callback.Run();

  RunUntilIdle();

  consumer.StopTracing();

  no_more_packets_runloop.Run();

  EXPECT_EQ(kNumPackets, consumer.received_packets());
  ProducerClient::DeleteSoonForTesting(std::move(client));
}

#if defined(THREAD_SANITIZER)
#define MAYBE_CommitDataRequestIsMaybeComplete \
  DISABLED_CommitDataRequestIsMaybeComplete
#else
#define MAYBE_CommitDataRequestIsMaybeComplete CommitDataRequestIsMaybeComplete
#endif
TEST_F(PerfettoIntegrationTest, MAYBE_CommitDataRequestIsMaybeComplete) {
  const size_t kNumPackets = 100;

  base::RunLoop no_more_packets_runloop;
  MockConsumer consumer(perfetto_service()->GetService(),
                        kPerfettoTestDataSourceName,
                        [&no_more_packets_runloop](bool has_more) {
                          if (!has_more) {
                            no_more_packets_runloop.Quit();
                          }
                        });

  base::RunLoop client_enabled_callback;
  auto client = std::make_unique<MockProducerClient>(
      kNumPackets, client_enabled_callback.QuitClosure());
  auto new_producer =
      std::make_unique<MockProducer>(kPerfettoTestDataSourceName);
  client->CreateMojoMessagepipes(
      base::BindOnce(&MockProducer::OnMessagepipesReadyCallback,
                     base::Unretained(new_producer.get()),
                     base::Unretained(perfetto_service()->GetService())));

  client_enabled_callback.Run();

  client->data_source()->WritePacketBigly();

  RunUntilIdle();

  consumer.StopTracing();

  no_more_packets_runloop.Run();

  EXPECT_EQ(client->all_client_commit_data_requests(),
            new_producer->all_host_commit_data_requests());

  ProducerClient::DeleteSoonForTesting(std::move(client));
}

TEST_F(PerfettoIntegrationTest, TracingRestarted) {
  const size_t kNumPackets = 10;

  base::RunLoop no_more_packets_runloop;
  MockConsumer consumer(perfetto_service()->GetService(),
                        kPerfettoTestDataSourceName,
                        [&no_more_packets_runloop](bool has_more) {
                          if (!has_more) {
                            no_more_packets_runloop.Quit();
                          }
                        });

  base::RunLoop client_enabled_callback;
  auto client = std::make_unique<MockProducerClient>(
      kNumPackets, client_enabled_callback.QuitClosure());

  auto new_producer =
      std::make_unique<MockProducer>(kPerfettoTestDataSourceName);
  client->CreateMojoMessagepipes(
      base::BindOnce(&MockProducer::OnMessagepipesReadyCallback,
                     base::Unretained(new_producer.get()),
                     base::Unretained(perfetto_service()->GetService())));

  client_enabled_callback.Run();

  RunUntilIdle();

  perfetto::SharedMemory* first_session_shm = client->shared_memory();
  consumer.StopTracing();

  no_more_packets_runloop.Run();
  EXPECT_EQ(kNumPackets, consumer.received_packets());

  consumer.FreeBuffers();

  base::RunLoop client_reenabled_callback;
  client->SetAgentEnabledCallback(client_reenabled_callback.QuitClosure());

  consumer.StartTracing();
  client_reenabled_callback.Run();

  RunUntilIdle();

  // We should still be using the same shared memory.
  EXPECT_EQ(first_session_shm, client->shared_memory());

  base::RunLoop client_redisabled_callback;
  client->SetAgentDisabledCallback(client_redisabled_callback.QuitClosure());

  consumer.StopTracing();
  client_redisabled_callback.Run();

  EXPECT_EQ(kNumPackets * 2, consumer.received_packets());

  ProducerClient::DeleteSoonForTesting(std::move(client));
}

TEST_F(PerfettoIntegrationTest, NoPacketsReceivedOnWrongSourceName) {
  const size_t kNumPackets = 10;

  base::RunLoop client_enabled_callback;
  base::RunLoop client_disabled_callback;
  auto client = std::make_unique<MockProducerClient>(
      kNumPackets, client_enabled_callback.QuitClosure(),
      client_disabled_callback.QuitClosure());

  base::RunLoop producer_initialized_runloop;
  auto new_producer = std::make_unique<MockProducer>(
      "fake", producer_initialized_runloop.QuitClosure());
  client->CreateMojoMessagepipes(
      base::BindOnce(&MockProducer::OnMessagepipesReadyCallback,
                     base::Unretained(new_producer.get()),
                     base::Unretained(perfetto_service()->GetService())));
  producer_initialized_runloop.Run();

  base::RunLoop no_more_packets_runloop;
  MockConsumer consumer(perfetto_service()->GetService(),
                        kPerfettoTestDataSourceName,
                        [&no_more_packets_runloop](bool has_more) {
                          if (!has_more) {
                            no_more_packets_runloop.Quit();
                          }
                        });

  RunUntilIdle();

  consumer.StopTracing();

  no_more_packets_runloop.Run();

  EXPECT_EQ(0u, consumer.received_packets());
  ProducerClient::DeleteSoonForTesting(std::move(client));
}

TEST_F(PerfettoIntegrationTest,
       DifferentSharedMemoryBuffersForDifferentAgents) {
  base::RunLoop client1_enabled_callback;
  base::RunLoop client2_enabled_callback;
  auto client1 = std::make_unique<MockProducerClient>(
      0 /* send_packet_count */, client1_enabled_callback.QuitClosure());
  auto client2 = std::make_unique<MockProducerClient>(
      0 /* send_packet_count */, client2_enabled_callback.QuitClosure());

  auto producer1 = std::make_unique<MockProducer>(kPerfettoTestDataSourceName);
  client1->CreateMojoMessagepipes(
      base::BindOnce(&MockProducer::OnMessagepipesReadyCallback,
                     base::Unretained(producer1.get()),
                     base::Unretained(perfetto_service()->GetService())));

  auto producer2 = std::make_unique<MockProducer>(kPerfettoTestDataSourceName);
  client2->CreateMojoMessagepipes(
      base::BindOnce(&MockProducer::OnMessagepipesReadyCallback,
                     base::Unretained(producer2.get()),
                     base::Unretained(perfetto_service()->GetService())));

  MockConsumer consumer(perfetto_service()->GetService(),
                        kPerfettoTestDataSourceName, nullptr);

  client1_enabled_callback.Run();
  client2_enabled_callback.Run();

  EXPECT_TRUE(client1->shared_memory());
  EXPECT_TRUE(client2->shared_memory());
  EXPECT_NE(client1->shared_memory(), client2->shared_memory());

  ProducerClient::DeleteSoonForTesting(std::move(client1));
  ProducerClient::DeleteSoonForTesting(std::move(client2));
}

}  // namespace

}  // namespace tracing
