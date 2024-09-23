// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_config.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/perfetto/test_utils.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/trace_packet_tokenizer.h"
#include "services/tracing/public/cpp/traced_process_impl.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "services/tracing/public/mojom/traced_process.mojom.h"
#include "services/tracing/public/mojom/tracing_service.mojom.h"
#include "services/tracing/tracing_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"
#include "third_party/perfetto/include/perfetto/tracing/data_source.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#include "third_party/perfetto/protos/perfetto/common/trace_stats.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/test_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace tracing {

class TracingServiceTest : public TracingUnitTest {
 public:
  TracingServiceTest() : service_(&perfetto_service_) {}

  TracingServiceTest(const TracingServiceTest&) = delete;
  TracingServiceTest& operator=(const TracingServiceTest&) = delete;

  void SetUp() override {
    TracingUnitTest::SetUp();
    perfetto_service()->SetActiveServicePidsInitialized();
  }

  void TearDown() override { TracingUnitTest::TearDown(); }

 protected:
  PerfettoService* perfetto_service() { return &perfetto_service_; }
  mojom::TracingService* service() { return &service_; }

  void EnableClientApiConsumer() {
    // Tell PerfettoTracedProcess how to connect to the service. This enables
    // the consumer part of the client API.
    static mojom::TracingService* s_service;
    s_service = service();
    auto factory = []() -> mojom::TracingService& { return *s_service; };
    PerfettoTracedProcess::Get()->SetConsumerConnectionFactory(
        factory, base::SequencedTaskRunner::GetCurrentDefault());
  }

  void EnableClientApiProducer() {
    // Connect the producer part of the client API. AddClient() will end up
    // calling TracedProcessImpl::ConnectToTracingService(), which will in turn
    // route the PerfettoService interface to the client API backend.
    mojo::PendingRemote<tracing::mojom::TracedProcess> traced_process_remote;
    traced_process_receiver_ =
        std::make_unique<mojo::Receiver<tracing::mojom::TracedProcess>>(
            tracing::TracedProcessImpl::GetInstance());
    traced_process_receiver_->Bind(
        traced_process_remote.InitWithNewPipeAndPassReceiver());
    auto client_info = mojom::ClientInfo::New(base::GetCurrentProcId(),
                                              std::move(traced_process_remote));
    service()->AddClient(std::move(client_info));
    perfetto_service()->SetActiveServicePidsInitialized();
  }

  static size_t CountTestPackets(const char* data, size_t length) {
    if (!length)
      return 0;
    size_t test_packet_count = 0;
    perfetto::protos::Trace trace;
    EXPECT_TRUE(trace.ParseFromArray(data, length));
    for (const auto& packet : trace.packet()) {
      if (packet.has_for_testing()) {
        EXPECT_EQ(kPerfettoTestString, packet.for_testing().str());
        test_packet_count++;
      }
    }
    return test_packet_count;
  }

  size_t ReadAndCountTestPackets(perfetto::TracingSession& session) {
    size_t test_packet_count = 0;
    base::RunLoop wait_for_data_loop;
    session.ReadTrace(
        [&wait_for_data_loop, &test_packet_count](
            perfetto::TracingSession::ReadTraceCallbackArgs args) {
          test_packet_count += CountTestPackets(args.data, args.size);
          if (!args.has_more)
            wait_for_data_loop.Quit();
        });
    wait_for_data_loop.Run();
    return test_packet_count;
  }

 private:
  PerfettoService perfetto_service_;
  TracingService service_;

  std::unique_ptr<mojo::Receiver<tracing::mojom::TracedProcess>>
      traced_process_receiver_;
};

class TestTracingClient : public mojom::TracingSessionClient {
 public:
  void StartTracing(mojom::TracingService* service,
                    base::OnceClosure on_tracing_enabled) {
    service->BindConsumerHost(consumer_host_.BindNewPipeAndPassReceiver());

    perfetto::TraceConfig perfetto_config =
        tracing::GetDefaultPerfettoConfig(base::trace_event::TraceConfig(""),
                                          /*privacy_filtering_enabled=*/false);

    consumer_host_->EnableTracing(
        tracing_session_host_.BindNewPipeAndPassReceiver(),
        receiver_.BindNewPipeAndPassRemote(), std::move(perfetto_config),
        base::File());

    tracing_session_host_->RequestBufferUsage(
        base::BindOnce([](base::OnceClosure on_response, bool, float,
                          bool) { std::move(on_response).Run(); },
                       std::move(on_tracing_enabled)));
  }

  void StopTracing(base::OnceClosure on_tracing_stopped) {
    tracing_disabled_callback_ = std::move(on_tracing_stopped);
    tracing_session_host_->DisableTracing();
  }

  // tracing::mojom::TracingSessionClient implementation:
  void OnTracingEnabled() override {}
  void OnTracingDisabled(bool) override {
    std::move(tracing_disabled_callback_).Run();
  }

 private:
  base::OnceClosure tracing_disabled_callback_;

  mojo::Remote<mojom::ConsumerHost> consumer_host_;
  mojo::Remote<mojom::TracingSessionHost> tracing_session_host_;
  mojo::Receiver<mojom::TracingSessionClient> receiver_{this};
};

TEST_F(TracingServiceTest, TracingServiceInstantiate) {
  TestTracingClient tracing_client;

  base::RunLoop tracing_started;
  tracing_client.StartTracing(service(), tracing_started.QuitClosure());
  tracing_started.Run();

  base::RunLoop tracing_stopped;
  tracing_client.StopTracing(tracing_stopped.QuitClosure());
  tracing_stopped.Run();
}

TEST_F(TracingServiceTest, PerfettoClientConsumer) {
  // Set up API bindings.
  EnableClientApiConsumer();

  // Register a mock producer with an in-process Perfetto service.
  auto pid = 123;
  size_t kNumPackets = 10;
  base::RunLoop wait_for_start;
  base::RunLoop wait_for_registration;
  std::unique_ptr<MockProducer> producer = std::make_unique<MockProducer>(
      std::string("org.chromium-") + base::NumberToString(pid),
      "com.example.mock_data_source", perfetto_service(),
      wait_for_registration.QuitClosure(), wait_for_start.QuitClosure(),
      kNumPackets);
  wait_for_registration.Run();

  // Start a tracing session using the client API.
  auto session =
      perfetto::Tracing::NewTrace(perfetto::BackendType::kCustomBackend);
  perfetto::TraceConfig perfetto_config;
  perfetto_config.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = perfetto_config.add_data_sources()->mutable_config();
  ds_cfg->set_name("com.example.mock_data_source");
  session->Setup(perfetto_config);
  session->Start();
  wait_for_start.Run();

  // Stop the session and wait for it to stop. Note that we can't use the
  // blocking API here because the service runs on the current sequence.
  base::RunLoop wait_for_stop_loop;
  session->SetOnStopCallback(
      [&wait_for_stop_loop] { wait_for_stop_loop.Quit(); });
  session->Stop();
  wait_for_stop_loop.Run();

  // Verify tracing session statistics.
  base::RunLoop wait_for_stats_loop;
  perfetto::protos::TraceStats stats;
  auto stats_callback =
      [&wait_for_stats_loop,
       &stats](perfetto::TracingSession::GetTraceStatsCallbackArgs args) {
        EXPECT_TRUE(args.success);
        EXPECT_TRUE(stats.ParseFromArray(args.trace_stats_data.data(),
                                         args.trace_stats_data.size()));
        wait_for_stats_loop.Quit();
      };
  session->GetTraceStats(std::move(stats_callback));
  wait_for_stats_loop.Run();
  EXPECT_EQ(1024u * 1024u, stats.buffer_stats(0).buffer_size());
  EXPECT_GT(stats.buffer_stats(0).bytes_written(), 0u);
  EXPECT_EQ(0u, stats.buffer_stats(0).trace_writer_packet_loss());

  // Read and verify the data.
  EXPECT_EQ(kNumPackets, ReadAndCountTestPackets(*session));
}

TEST_F(TracingServiceTest, PerfettoClientConsumerLegacyJson) {
  // Set up API bindings.
  EnableClientApiConsumer();

  // Start a tracing session with legacy JSON exporting.
  auto session =
      perfetto::Tracing::NewTrace(perfetto::BackendType::kCustomBackend);
  perfetto::TraceConfig perfetto_config = GetDefaultPerfettoConfig(
      base::trace_event::TraceConfig(), /*privacy_filtering_enabled=*/false,
      /*convert_to_legacy_json=*/true);
  session->Setup(perfetto_config);
  session->Start();

  // Stop the session and wait for it to stop. Note that we can't use the
  // blocking API here because the service runs on the current sequence.
  base::RunLoop wait_for_stop_loop;
  session->SetOnStopCallback(
      [&wait_for_stop_loop] { wait_for_stop_loop.Quit(); });
  session->Stop();
  wait_for_stop_loop.Run();

  // Read and verify the JSON data.
  base::RunLoop wait_for_data_loop;
  TracePacketTokenizer tokenizer;
  std::string json;
  session->ReadTrace([&wait_for_data_loop, &tokenizer, &json](
                         perfetto::TracingSession::ReadTraceCallbackArgs args) {
    if (args.size) {
      auto packets = tokenizer.Parse(
          reinterpret_cast<const uint8_t*>(args.data), args.size);
      for (const auto& packet : packets) {
        for (const auto& slice : packet.slices()) {
          json += std::string(reinterpret_cast<const char*>(slice.start),
                              slice.size);
        }
      }
    }
    if (!args.has_more)
      wait_for_data_loop.Quit();
  });
  wait_for_data_loop.Run();
  DCHECK(!tokenizer.has_more());

  std::optional<base::Value> result = base::JSONReader::Read(json);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->GetDict().contains("traceEvents"));
}

class CustomDataSource : public perfetto::DataSource<CustomDataSource> {
 public:
  struct Events {
    base::RunLoop wait_for_setup_loop;
    base::RunLoop wait_for_start_loop;
    base::RunLoop wait_for_stop_loop;
  };

  static void set_events(Events* events) { events_ = events; }

  void OnSetup(const SetupArgs&) override {
    events_->wait_for_setup_loop.Quit();
  }
  void OnStart(const StartArgs&) override {
    events_->wait_for_start_loop.Quit();
  }
  void OnStop(const StopArgs&) override { events_->wait_for_stop_loop.Quit(); }

 private:
  static Events* events_;
};

CustomDataSource::Events* CustomDataSource::events_;

TEST_F(TracingServiceTest, PerfettoClientProducer) {
  // Set up API bindings.
  EnableClientApiConsumer();
  EnableClientApiProducer();

  perfetto::DataSourceDescriptor dsd;
  dsd.set_name("com.example.custom_data_source");
  CustomDataSource::Events ds_events;
  CustomDataSource::set_events(&ds_events);
  CustomDataSource::Register(dsd);

  // Start a tracing session using the client API.
  auto session =
      perfetto::Tracing::NewTrace(perfetto::BackendType::kCustomBackend);
  perfetto::TraceConfig perfetto_config;
  perfetto_config.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = perfetto_config.add_data_sources()->mutable_config();
  ds_cfg->set_name("com.example.custom_data_source");
  session->Setup(perfetto_config);
  session->Start();

  ds_events.wait_for_setup_loop.Run();
  ds_events.wait_for_start_loop.Run();

  // Write more data to check the commit flow works too.
  size_t kNumPackets = 1000;
  CustomDataSource::Trace([kNumPackets](CustomDataSource::TraceContext ctx) {
    for (size_t i = 0; i < kNumPackets / 2; i++) {
      ctx.NewTracePacket()->set_for_testing()->set_str(
          tracing::kPerfettoTestString);
    }
    ctx.Flush();
  });

  // Write half of the data from another thread to check TLS is hooked up
  // properly.
  base::Thread thread("TestThread");
  thread.Start();
  thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([kNumPackets] {
        CustomDataSource::Trace(
            [kNumPackets](CustomDataSource::TraceContext ctx) {
              for (size_t i = 0; i < kNumPackets / 2; i++) {
                ctx.NewTracePacket()->set_for_testing()->set_str(
                    tracing::kPerfettoTestString);
              }
              ctx.Flush();
            });
      }));
  thread.Stop();

  // Stop the session and wait for it to stop. Note that we can't use the
  // blocking variants here because the service runs on the current sequence.
  base::RunLoop wait_for_stop_loop;
  session->SetOnStopCallback(
      [&wait_for_stop_loop] { wait_for_stop_loop.Quit(); });
  session->Stop();
  ds_events.wait_for_stop_loop.Run();
  wait_for_stop_loop.Run();

  // Read and verify the data.
  EXPECT_EQ(kNumPackets, ReadAndCountTestPackets(*session));
}

#if !BUILDFLAG(IS_WIN)
// TODO(crbug.com/40736989): Support tracing to file on Windows.
TEST_F(TracingServiceTest, TraceToFile) {
  // Set up API bindings.
  EnableClientApiConsumer();

  // Register a mock producer with an in-process Perfetto service.
  auto pid = 123;
  size_t kNumPackets = 10;
  base::RunLoop wait_for_start;
  base::RunLoop wait_for_registration;
  std::unique_ptr<MockProducer> producer = std::make_unique<MockProducer>(
      std::string("org.chromium-") + base::NumberToString(pid),
      "com.example.mock_data_source", perfetto_service(),
      wait_for_registration.QuitClosure(), wait_for_start.QuitClosure(),
      kNumPackets);
  wait_for_registration.Run();

  base::FilePath output_file_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&output_file_path));

  base::File output_file;
  output_file.Initialize(output_file_path,
                         base::File::FLAG_OPEN | base::File::FLAG_WRITE);

  // Start a tracing session using the client API.
  auto session =
      perfetto::Tracing::NewTrace(perfetto::BackendType::kCustomBackend);
  perfetto::TraceConfig perfetto_config;
  perfetto_config.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = perfetto_config.add_data_sources()->mutable_config();
  ds_cfg->set_name("com.example.mock_data_source");
  session->Setup(perfetto_config, output_file.TakePlatformFile());
  session->Start();
  wait_for_start.Run();

  // Stop the session and wait for it to stop. Note that we can't use the
  // blocking API here because the service runs on the current sequence.
  base::RunLoop wait_for_stop_loop;
  session->SetOnStopCallback(
      [&wait_for_stop_loop] { wait_for_stop_loop.Quit(); });
  session->Stop();
  wait_for_stop_loop.Run();

  // Read and verify the data.
  std::string trace;
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::ReadFileToString(output_file_path, &trace));
  EXPECT_EQ(kNumPackets, CountTestPackets(trace.data(), trace.length()));
}
#endif

}  // namespace tracing
