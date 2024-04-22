// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/threading/thread.h"
#include "base/tracing/perfetto_platform.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/perfetto/producer_host.h"
#include "services/tracing/perfetto/test_utils.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#include "third_party/perfetto/protos/perfetto/common/commit_data_request.pb.h"

// TODO(crbug.com/41457644): Fix memory leaks in tests and re-enable on LSAN.
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

std::string GetPerfettoProducerName() {
  return base::StrCat({mojom::kPerfettoProducerNamePrefix, "123"});
}

class PerfettoIntegrationTest : public TracingUnitTest {
 public:
  void SetUp() override {
    TracingUnitTest::SetUp();
    data_source_ = TestDataSource::CreateAndRegisterDataSource(
        kPerfettoTestDataSourceName, 0);
    perfetto_service_ = std::make_unique<PerfettoService>();
    RunUntilIdle();
  }

  void TearDown() override {
    perfetto_service_.reset();
    TracingUnitTest::TearDown();
  }

  PerfettoService* perfetto_service() const { return perfetto_service_.get(); }

 protected:
  std::unique_ptr<TestDataSource> data_source_;
  std::unique_ptr<PerfettoService> perfetto_service_;
};

TEST_F(PerfettoIntegrationTest, ProducerDatasourceInitialized) {
  std::unique_ptr<MockProducerClient::Handle> dummy_client =
      MockProducerClient::Create();

  base::RunLoop producer_initialized_runloop;
  auto new_producer = std::make_unique<MockProducerHost>(
      GetPerfettoProducerName(), kPerfettoTestDataSourceName, perfetto_service(),
      **dummy_client, producer_initialized_runloop.QuitClosure());

  producer_initialized_runloop.Run();
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
  std::unique_ptr<MockProducerClient::Handle> client =
      MockProducerClient::Create(
          /* num_data_sources = */ 1, client_enabled_callback.QuitClosure(),
          client_disabled_callback.QuitClosure());

  auto producer = std::make_unique<MockProducerHost>(
      GetPerfettoProducerName(), kPerfettoTestDataSourceName, perfetto_service(),
      **client);

  client_enabled_callback.Run();

  RunUntilIdle();

  consumer.StopTracing();

  client_disabled_callback.Run();

  on_trace_packets.Run();
  EXPECT_EQ(0u, consumer.received_test_packets());
}

TEST_F(PerfettoIntegrationTest, PacketsEndToEndProducerFirst) {
  const size_t kNumPackets = 10;
  data_source_->set_send_packet_count(kNumPackets);

  base::RunLoop client_enabled_callback;
  base::RunLoop client_disabled_callback;
  std::unique_ptr<MockProducerClient::Handle> client =
      MockProducerClient::Create(
          /* num_data_sources = */ 1, client_enabled_callback.QuitClosure(),
          client_disabled_callback.QuitClosure());

  auto producer = std::make_unique<MockProducerHost>(
      GetPerfettoProducerName(), kPerfettoTestDataSourceName, perfetto_service(),
      **client);

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
  std::unique_ptr<MockProducerClient::Handle> client =
      MockProducerClient::Create(
          /* num_data_sources = */ 1, client_enabled_callback.QuitClosure());

  auto new_producer = std::make_unique<MockProducerHost>(
      GetPerfettoProducerName(), kPerfettoTestDataSourceName, perfetto_service(),
      **client);

  client_enabled_callback.Run();

  RunUntilIdle();

  consumer.StopTracing();

  no_more_packets_runloop.Run();

  EXPECT_EQ(kNumPackets, consumer.received_test_packets());
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
  std::unique_ptr<MockProducerClient::Handle> client =
      MockProducerClient::Create(
          /* num_data_sources = */ 1, client_enabled_callback.QuitClosure());
  auto new_producer = std::make_unique<MockProducerHost>(
      GetPerfettoProducerName(), kPerfettoTestDataSourceName, perfetto_service(),
      **client);

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

  // {cli,host}_reqs are a std::vector<std::string>. Each entry of the vector
  // is a proto-serialized CommitDataRequest.
  const auto& cli_reqs = (*client)->all_client_commit_data_requests();
  const auto& host_reqs = new_producer->all_host_commit_data_requests();
  ASSERT_EQ(cli_reqs.size(), host_reqs.size());
  for (size_t i = 0; i < cli_reqs.size(); i++) {
    // Note that the proto-encoded strings are not identical. This is because
    // perfetto doesn't emit unset fields. But then when going over mojo these
    // unset fields get copied as 0-value (it's fine), so on the host side we
    // see extra fields being explicitly zero-initialized. Here we force a
    // re-encode using the libprotobuf message for the sake of the comparison.
    // libprotobuf will re-normalize messages before serializing.
    perfetto::protos::CommitDataRequest cli_req;
    ASSERT_TRUE(cli_req.ParseFromString(cli_reqs[i]));
    perfetto::protos::CommitDataRequest host_req;
    ASSERT_TRUE(host_req.ParseFromString(cli_reqs[i]));
    ASSERT_EQ(cli_req.SerializeAsString(), host_req.SerializeAsString());
  }
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
  std::unique_ptr<MockProducerClient::Handle> client =
      MockProducerClient::Create(
          /* num_data_sources = */ 1, client_enabled_callback.QuitClosure(),
          client_disabled_callback.QuitClosure());

  auto new_producer = std::make_unique<MockProducerHost>(
      GetPerfettoProducerName(), kPerfettoTestDataSourceName, perfetto_service(),
      **client);

  client_enabled_callback.Run();

  RunUntilIdle();

  perfetto::SharedMemory* first_session_shm =
      (*client)->shared_memory_for_testing();
  consumer.StopTracing();

  client_disabled_callback.Run();
  no_more_packets_runloop.Run();
  EXPECT_EQ(kNumPackets, consumer.received_test_packets());

  consumer.FreeBuffers();

  base::RunLoop client_reenabled_callback;
  (*client)->SetAgentEnabledCallback(client_reenabled_callback.QuitClosure());

  consumer.StartTracing();
  client_reenabled_callback.Run();

  RunUntilIdle();

  // We should still be using the same shared memory.
  EXPECT_EQ(first_session_shm, (*client)->shared_memory_for_testing());

  base::RunLoop client_redisabled_callback;
  (*client)->SetAgentDisabledCallback(client_redisabled_callback.QuitClosure());

  consumer.StopTracing();
  client_redisabled_callback.Run();

  EXPECT_EQ(kNumPackets * 2, consumer.received_test_packets());
}

TEST_F(PerfettoIntegrationTest, NoPacketsReceivedOnWrongSourceName) {
  const size_t kNumPackets = 10;
  data_source_->set_send_packet_count(kNumPackets);

  base::RunLoop client_enabled_callback;
  base::RunLoop client_disabled_callback;
  std::unique_ptr<MockProducerClient::Handle> client =
      MockProducerClient::Create(
          /* num_data_sources = */ 0, client_enabled_callback.QuitClosure(),
          client_disabled_callback.QuitClosure());

  base::RunLoop producer_initialized_runloop;
  auto new_producer = std::make_unique<MockProducerHost>(
      GetPerfettoProducerName(), "fake_data_source", perfetto_service(), **client);

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
}

TEST_F(PerfettoIntegrationTest,
       MAYBE_DifferentSharedMemoryBuffersForDifferentAgents) {
  base::RunLoop client1_enabled_callback;
  base::RunLoop client2_enabled_callback;
  std::unique_ptr<MockProducerClient::Handle> client1 =
      MockProducerClient::Create(
          /* num_data_sources = */ 1, client1_enabled_callback.QuitClosure());
  auto producer1 = std::make_unique<MockProducerHost>(
      GetPerfettoProducerName(), kPerfettoTestDataSourceName, perfetto_service(),
      **client1);

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
  data_source_->ClearProducerForTesting();

  std::unique_ptr<MockProducerClient::Handle> client2 =
      MockProducerClient::Create(
          /* num_data_sources = */ 1, client2_enabled_callback.QuitClosure());

  auto producer2 = std::make_unique<MockProducerHost>(
      GetPerfettoProducerName(), kPerfettoTestDataSourceName, perfetto_service(),
      **client2);

  client2_enabled_callback.Run();

  EXPECT_TRUE((*client1)->shared_memory_for_testing());
  EXPECT_TRUE((*client2)->shared_memory_for_testing());
  EXPECT_NE((*client1)->shared_memory_for_testing(),
            (*client2)->shared_memory_for_testing());
}

TEST_F(PerfettoIntegrationTest, PerfettoPlatformTest) {
  auto* platform =
      PerfettoTracedProcess::Get()->perfetto_platform_for_testing();
  auto* tls = platform->GetOrCreateThreadLocalObject();
  EXPECT_TRUE(tls);
  EXPECT_EQ(tls, platform->GetOrCreateThreadLocalObject());

  base::Thread thread("TestThread");
  thread.Start();
  thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&platform, &tls] {
        auto* thread_tls = platform->GetOrCreateThreadLocalObject();
        EXPECT_TRUE(thread_tls);
        EXPECT_NE(tls, thread_tls);
      }));
  thread.Stop();
}

TEST_F(PerfettoIntegrationTest, PerfettoClientLibraryTest) {
  // Check that PerfettoTracedProcess initialized the client library. Functional
  // client library tests are in TracingServiceTest.
  EXPECT_TRUE(perfetto::Tracing::IsInitialized());
}

}  // namespace

}  // namespace tracing
