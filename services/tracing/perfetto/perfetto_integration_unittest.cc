// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/perfetto/producer_host.h"
#include "services/tracing/perfetto/test_utils.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug.com/961066): Fix memory leaks in tests and re-enable on LSAN.
#ifdef LEAK_SANITIZER
#define MAYBE_DifferentSharedMemoryBuffersForDifferentAgents \
  DISABLED_DifferentSharedMemoryBuffersForDifferentAgents
#else
#define MAYBE_DifferentSharedMemoryBuffersForDifferentAgents \
  DifferentSharedMemoryBuffersForDifferentAgents
#endif

namespace tracing {

namespace {

const char kPerfettoTestDataSourceName[] =
    "org.chromium.chrome_integration_unittest";
const char kPerfettoProducerName[] = "org.chromium.perfetto_producer.123";

class PerfettoIntegrationTest : public testing::Test {
 public:
  void SetUp() override {
    PerfettoTracedProcess::ResetTaskRunnerForTesting();
    PerfettoTracedProcess::Get()->ClearDataSourcesForTesting();
    data_source_ = TestDataSource::CreateAndRegisterDataSource(
        kPerfettoTestDataSourceName, 0);
    perfetto_service_ = std::make_unique<PerfettoService>();
    RunUntilIdle();
  }

  void TearDown() override { perfetto_service_.reset(); }

  PerfettoService* perfetto_service() const { return perfetto_service_.get(); }
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 protected:
  std::unique_ptr<TestDataSource> data_source_;
  std::unique_ptr<PerfettoService> perfetto_service_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(PerfettoIntegrationTest, ProducerDatasourceInitialized) {
  auto dummy_client = std::make_unique<MockProducerClient>();

  base::RunLoop producer_initialized_runloop;
  auto new_producer = std::make_unique<MockProducerHost>(
      kPerfettoProducerName, kPerfettoTestDataSourceName,
      perfetto_service()->GetService(), dummy_client.get(),
      producer_initialized_runloop.QuitClosure());

  producer_initialized_runloop.Run();

  ProducerClient::DeleteSoonForTesting(std::move(dummy_client));
}

TEST_F(PerfettoIntegrationTest, ClientEnabledAndDisabled) {
  base::RunLoop on_trace_packets;
  MockConsumer consumer({kPerfettoTestDataSourceName},
                        perfetto_service()->GetService(),
                        [&on_trace_packets](bool has_more) {
                          EXPECT_FALSE(has_more);
                          on_trace_packets.Quit();
                        });

  base::RunLoop client_enabled_callback;
  base::RunLoop client_disabled_callback;
  auto client = std::make_unique<MockProducerClient>(
      /* num_data_sources = */ 1, client_enabled_callback.QuitClosure(),
      client_disabled_callback.QuitClosure());

  auto producer = std::make_unique<MockProducerHost>(
      kPerfettoProducerName, kPerfettoTestDataSourceName,
      perfetto_service()->GetService(), client.get());

  client_enabled_callback.Run();

  RunUntilIdle();

  consumer.StopTracing();

  client_disabled_callback.Run();

  on_trace_packets.Run();
  EXPECT_EQ(0u, consumer.received_test_packets());

  ProducerClient::DeleteSoonForTesting(std::move(client));
}

TEST_F(PerfettoIntegrationTest, PacketsEndToEndProducerFirst) {
  const size_t kNumPackets = 10;
  data_source_->set_send_packet_count(kNumPackets);

  base::RunLoop client_enabled_callback;
  base::RunLoop client_disabled_callback;
  auto client = std::make_unique<MockProducerClient>(
      /* num_data_sources = */ 1, client_enabled_callback.QuitClosure(),
      client_disabled_callback.QuitClosure());

  auto producer = std::make_unique<MockProducerHost>(
      kPerfettoProducerName, kPerfettoTestDataSourceName,
      perfetto_service()->GetService(), client.get());

  base::RunLoop no_more_packets_runloop;
  MockConsumer consumer({kPerfettoTestDataSourceName},
                        perfetto_service()->GetService(),
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

  EXPECT_EQ(kNumPackets, consumer.received_test_packets());

  ProducerClient::DeleteSoonForTesting(std::move(client));
}

TEST_F(PerfettoIntegrationTest, PacketsEndToEndConsumerFirst) {
  const size_t kNumPackets = 10;
  data_source_->set_send_packet_count(kNumPackets);

  base::RunLoop no_more_packets_runloop;
  MockConsumer consumer({kPerfettoTestDataSourceName},
                        perfetto_service()->GetService(),
                        [&no_more_packets_runloop](bool has_more) {
                          if (!has_more) {
                            no_more_packets_runloop.Quit();
                          }
                        });

  base::RunLoop client_enabled_callback;
  auto client = std::make_unique<MockProducerClient>(
      /* num_data_sources = */ 1, client_enabled_callback.QuitClosure());

  auto new_producer = std::make_unique<MockProducerHost>(
      kPerfettoProducerName, kPerfettoTestDataSourceName,
      perfetto_service()->GetService(), client.get());

  client_enabled_callback.Run();

  RunUntilIdle();

  consumer.StopTracing();

  no_more_packets_runloop.Run();

  EXPECT_EQ(kNumPackets, consumer.received_test_packets());
  ProducerClient::DeleteSoonForTesting(std::move(client));
}

TEST_F(PerfettoIntegrationTest, CommitDataRequestIsMaybeComplete) {
  const size_t kNumPackets = 100;
  data_source_->set_send_packet_count(kNumPackets);

  base::RunLoop no_more_packets_runloop;
  MockConsumer consumer({kPerfettoTestDataSourceName},
                        perfetto_service()->GetService(),
                        [&no_more_packets_runloop](bool has_more) {
                          if (!has_more) {
                            no_more_packets_runloop.Quit();
                          }
                        });

  base::RunLoop client_enabled_callback;
  auto client = std::make_unique<MockProducerClient>(
      /* num_data_sources = */ 1, client_enabled_callback.QuitClosure());
  auto new_producer = std::make_unique<MockProducerHost>(
      kPerfettoProducerName, kPerfettoTestDataSourceName,
      perfetto_service()->GetService(), client.get());

  client_enabled_callback.Run();

  base::RunLoop wait_for_packet_write;
  PerfettoTracedProcess::Get()
      ->GetTaskRunner()
      ->GetOrCreateTaskRunner()
      ->PostTaskAndReply(FROM_HERE,
                         base::BindOnce(&TestDataSource::WritePacketBigly,
                                        base::Unretained(data_source_.get())),
                         wait_for_packet_write.QuitClosure());
  wait_for_packet_write.Run();

  RunUntilIdle();

  consumer.StopTracing();

  no_more_packets_runloop.Run();

  EXPECT_EQ(client->all_client_commit_data_requests(),
            new_producer->all_host_commit_data_requests());

  ProducerClient::DeleteSoonForTesting(std::move(client));
}

TEST_F(PerfettoIntegrationTest, TracingRestarted) {
  const size_t kNumPackets = 10;
  data_source_->set_send_packet_count(kNumPackets);

  base::RunLoop no_more_packets_runloop;
  MockConsumer consumer({kPerfettoTestDataSourceName},
                        perfetto_service()->GetService(),
                        [&no_more_packets_runloop](bool has_more) {
                          if (!has_more) {
                            no_more_packets_runloop.Quit();
                          }
                        });

  base::RunLoop client_enabled_callback;
  base::RunLoop client_disabled_callback;
  auto client = std::make_unique<MockProducerClient>(
      /* num_data_sources = */ 1, client_enabled_callback.QuitClosure(),
      client_disabled_callback.QuitClosure());

  auto new_producer = std::make_unique<MockProducerHost>(
      kPerfettoProducerName, kPerfettoTestDataSourceName,
      perfetto_service()->GetService(), client.get());

  client_enabled_callback.Run();

  RunUntilIdle();

  perfetto::SharedMemory* first_session_shm = client->shared_memory();
  consumer.StopTracing();

  client_disabled_callback.Run();
  no_more_packets_runloop.Run();
  EXPECT_EQ(kNumPackets, consumer.received_test_packets());

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

  EXPECT_EQ(kNumPackets * 2, consumer.received_test_packets());

  ProducerClient::DeleteSoonForTesting(std::move(client));
}

TEST_F(PerfettoIntegrationTest, NoPacketsReceivedOnWrongSourceName) {
  const size_t kNumPackets = 10;
  data_source_->set_send_packet_count(kNumPackets);

  base::RunLoop client_enabled_callback;
  base::RunLoop client_disabled_callback;
  auto client = std::make_unique<MockProducerClient>(
      /* num_data_sources = */ 0, client_enabled_callback.QuitClosure(),
      client_disabled_callback.QuitClosure());

  base::RunLoop producer_initialized_runloop;
  auto new_producer = std::make_unique<MockProducerHost>(
      kPerfettoProducerName, "fake_data_source",
      perfetto_service()->GetService(), client.get());

  base::RunLoop no_more_packets_runloop;
  MockConsumer consumer({"fake_data_source"}, perfetto_service()->GetService(),
                        [&no_more_packets_runloop](bool has_more) {
                          if (!has_more) {
                            no_more_packets_runloop.Quit();
                          }
                        });

  RunUntilIdle();

  consumer.StopTracing();

  no_more_packets_runloop.Run();

  EXPECT_EQ(0u, consumer.received_test_packets());
  ProducerClient::DeleteSoonForTesting(std::move(client));
}

TEST_F(PerfettoIntegrationTest,
       MAYBE_DifferentSharedMemoryBuffersForDifferentAgents) {
  base::RunLoop client1_enabled_callback;
  base::RunLoop client2_enabled_callback;
  auto client1 = std::make_unique<MockProducerClient>(
      /* num_data_sources = */ 1, client1_enabled_callback.QuitClosure());
  auto producer1 = std::make_unique<MockProducerHost>(
      kPerfettoProducerName, kPerfettoTestDataSourceName,
      perfetto_service()->GetService(), client1.get());

  // Start the trace here, this is because we need to send the EnableTracing
  // call to client1, but constructing client2 will override the
  // |producer_client_| pointer in PerfettoTracedProcess::Get() so we wait until
  // client1 has been enabled before constructing the second producer client.
  MockConsumer consumer({kPerfettoTestDataSourceName},
                        perfetto_service()->GetService(), nullptr);

  client1_enabled_callback.Run();

  // client2 will trigger a StartTracing call without shutting down the data
  // source first, to prevent this hitting a DCHECK set the previous producer to
  // null.
  data_source_->SetSystemProducerToNullptr();

  auto client2 = std::make_unique<MockProducerClient>(
      /* num_data_sources = */ 1, client2_enabled_callback.QuitClosure());

  auto producer2 = std::make_unique<MockProducerHost>(
      kPerfettoProducerName, kPerfettoTestDataSourceName,
      perfetto_service()->GetService(), client2.get());

  client2_enabled_callback.Run();

  EXPECT_TRUE(client1->shared_memory());
  EXPECT_TRUE(client2->shared_memory());
  EXPECT_NE(client1->shared_memory(), client2->shared_memory());

  ProducerClient::DeleteSoonForTesting(std::move(client1));
  ProducerClient::DeleteSoonForTesting(std::move(client2));
}

}  // namespace

}  // namespace tracing
