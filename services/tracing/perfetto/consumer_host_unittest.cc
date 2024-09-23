// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/consumer_host.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/extend.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/trace_event/trace_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/perfetto/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/protos/perfetto/config/trace_config.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pb.h"

namespace tracing {

const std::string kDataSourceName = "track_event";

constexpr base::ProcessId kProducerPid = 1234;

// This is here so we can properly simulate this running on three
// different sequences (ProducerClient side, Service side, and
// whatever connects via Mojo to the Producer). This is needed
// so we don't get into read/write locks.
class ThreadedPerfettoService : public mojom::TracingSessionClient {
 public:
  ThreadedPerfettoService()
      : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
             base::WithBaseSyncPrimitives(),
             base::TaskPriority::BEST_EFFORT})) {
    perfetto_service_ = std::make_unique<PerfettoService>(task_runner_);
    tracing_session_host_ =
        std::make_unique<mojo::Remote<mojom::TracingSessionHost>>();
    base::RunLoop wait_for_construct;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&ThreadedPerfettoService::CreateConsumerOnSequence,
                       base::Unretained(this)),
        wait_for_construct.QuitClosure());
    wait_for_construct.Run();
  }

  ~ThreadedPerfettoService() override {
    if (receiver_) {
      task_runner_->DeleteSoon(FROM_HERE, std::move(receiver_));
    }

    task_runner_->DeleteSoon(FROM_HERE, std::move(producer_));
    if (consumer_) {
      task_runner_->DeleteSoon(FROM_HERE, std::move(consumer_));
    }

    if (tracing_session_host_)
      task_runner_->DeleteSoon(FROM_HERE, std::move(tracing_session_host_));

    {
      base::RunLoop wait_for_destruction;
      task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                     wait_for_destruction.QuitClosure());
      wait_for_destruction.Run();
    }

    {
      base::RunLoop wait_for_destruction;
      task_runner_->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(
              [](std::unique_ptr<PerfettoService> service) { service.reset(); },
              std::move(perfetto_service_)),
          wait_for_destruction.QuitClosure());
      wait_for_destruction.Run();
    }

    {
      base::RunLoop wait_for_destruction;
      PerfettoTracedProcess::GetTaskRunner()
          ->GetOrCreateTaskRunner()
          ->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                             wait_for_destruction.QuitClosure());
      wait_for_destruction.Run();
    }
  }

  // mojom::TracingSessionClient implementation:
  void OnTracingEnabled() override {
    EXPECT_FALSE(tracing_enabled_);
    tracing_enabled_ = true;
  }

  void OnTracingDisabled(bool) override {}

  void CreateProducer(const std::string& data_source_name,
                      size_t num_packets,
                      base::OnceClosure on_tracing_started) {
    base::RunLoop wait_for_datasource_registration;
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ThreadedPerfettoService::CreateProducerOnSequence,
                       base::Unretained(this), data_source_name,
                       wait_for_datasource_registration.QuitClosure(),
                       std::move(on_tracing_started), num_packets));
    wait_for_datasource_registration.Run();
  }

  void CreateConsumerOnSequence() {
    consumer_ = std::make_unique<ConsumerHost>(perfetto_service_.get());
  }

  void CreateProducerOnSequence(const std::string& data_source_name,
                                base::OnceClosure on_datasource_registered,
                                base::OnceClosure on_tracing_started,
                                size_t num_packets) {
    producer_ = std::make_unique<MockProducer>(
        base::StrCat({mojom::kPerfettoProducerNamePrefix,
                      base::NumberToString(kProducerPid)}),
        data_source_name, perfetto_service_.get(),
        std::move(on_datasource_registered), std::move(on_tracing_started),
        num_packets);
  }

  void EnableTracingWithConfig(const perfetto::TraceConfig& config) {
    base::RunLoop wait_for_call;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&ThreadedPerfettoService::EnableTracingOnSequence,
                       base::Unretained(this), std::move(config)),
        wait_for_call.QuitClosure());
    wait_for_call.Run();
  }

  void EnableTracingOnSequence(const perfetto::TraceConfig& config) {
    mojo::PendingRemote<tracing::mojom::TracingSessionClient>
        tracing_session_client;
    receiver_ = std::make_unique<mojo::Receiver<mojom::TracingSessionClient>>(
        this, tracing_session_client.InitWithNewPipeAndPassReceiver());

    consumer_->EnableTracing(
        tracing_session_host_->BindNewPipeAndPassReceiver(),
        std::move(tracing_session_client), std::move(config), base::File());
  }

  void ReadBuffers(mojo::ScopedDataPipeProducerHandle stream,
                   ConsumerHost::TracingSession::ReadBuffersCallback callback) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ConsumerHost::TracingSession::ReadBuffers,
            base::Unretained(consumer_.get()->tracing_session_for_testing()),
            std::move(stream), std::move(callback)));
  }

  void DisableTracing() {
    base::RunLoop wait_for_call;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            &ConsumerHost::TracingSession::DisableTracing,
            base::Unretained(consumer_.get()->tracing_session_for_testing())),
        wait_for_call.QuitClosure());
    wait_for_call.Run();
  }

  void DisableTracingAndEmitJson(
      mojo::ScopedDataPipeProducerHandle stream,
      ConsumerHost::TracingSession::DisableTracingAndEmitJsonCallback callback,
      bool enable_privacy_filtering) {
    base::RunLoop wait_for_call;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            &ConsumerHost::TracingSession::DisableTracingAndEmitJson,
            base::Unretained(consumer_.get()->tracing_session_for_testing()),
            std::string(), std::move(stream), enable_privacy_filtering,
            std::move(callback)),
        wait_for_call.QuitClosure());
    wait_for_call.Run();
  }

  void WritePacketBigly() {
    base::RunLoop wait_for_call;
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&MockProducer::WritePacketBigly,
                                          base::Unretained(producer_.get()),
                                          wait_for_call.QuitClosure()));
    wait_for_call.Run();
  }

  void Flush(base::OnceClosure on_flush_complete) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ConsumerHost::TracingSession::Flush,
            base::Unretained(consumer_.get()->tracing_session_for_testing()),
            10000u,
            base::BindOnce(
                [](base::OnceClosure callback, bool success) {
                  EXPECT_TRUE(success);
                  std::move(callback).Run();
                },
                std::move(on_flush_complete))));
  }

  void ExpectPid(base::ProcessId pid) {
    base::RunLoop wait_for_call;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&PerfettoService::AddActiveServicePid,
                       base::Unretained(perfetto_service_.get()), pid),
        wait_for_call.QuitClosure());
    wait_for_call.Run();
  }

  void SetPidsInitialized() {
    base::RunLoop wait_for_call;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&PerfettoService::SetActiveServicePidsInitialized,
                       base::Unretained(perfetto_service_.get())),
        wait_for_call.QuitClosure());
    wait_for_call.Run();
  }

  void RemovePid(base::ProcessId pid) {
    base::RunLoop wait_for_call;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&PerfettoService::RemoveActiveServicePid,
                       base::Unretained(perfetto_service_.get()), pid),
        wait_for_call.QuitClosure());
    wait_for_call.Run();
  }

  bool IsTracingEnabled() const {
    bool tracing_enabled;
    base::RunLoop wait_for_call;
    task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&ThreadedPerfettoService::GetTracingEnabledOnSequence,
                       base::Unretained(this), &tracing_enabled),
        wait_for_call.QuitClosure());
    wait_for_call.Run();
    return tracing_enabled;
  }

  void GetTracingEnabledOnSequence(bool* tracing_enabled) const {
    *tracing_enabled = tracing_enabled_;
  }

  perfetto::DataSourceConfig GetProducerClientConfig() {
    perfetto::DataSourceConfig config;
    base::RunLoop wait_loop;
    task_runner_->PostTaskAndReply(FROM_HERE, base::BindLambdaForTesting([&]() {
                                     config =
                                         producer_->data_source()->config();
                                   }),
                                   wait_loop.QuitClosure());
    wait_loop.Run();
    return config;
  }

  void ClearConsumer() {
    base::RunLoop wait_loop;
    task_runner_->PostTaskAndReply(
        FROM_HERE, base::BindLambdaForTesting([&]() { consumer_.reset(); }),
        wait_loop.QuitClosure());
    wait_loop.Run();
  }

  PerfettoService* perfetto_service() const { return perfetto_service_.get(); }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<PerfettoService> perfetto_service_;
  std::unique_ptr<ConsumerHost> consumer_;
  std::unique_ptr<MockProducer> producer_;
  std::unique_ptr<mojo::Receiver<mojom::TracingSessionClient>> receiver_;
  std::unique_ptr<mojo::Remote<tracing::mojom::TracingSessionHost>>
      tracing_session_host_;
  bool tracing_enabled_ = false;
};

// TODO(crbug.com/42050015): Switch this to use TracingUnitTest.
class TracingConsumerTest : public testing::Test,
                            public mojo::DataPipeDrainer::Client {
 public:
  void SetUp() override {
    task_environment_ = std::make_unique<base::test::TaskEnvironment>(
        base::test::TaskEnvironment::MainThreadType::IO);
    tracing_environment_ = std::make_unique<base::test::TracingEnvironment>(
        *task_environment_, base::SingleThreadTaskRunner::GetCurrentDefault());
    test_handle_ = tracing::PerfettoTracedProcess::SetupForTesting();
    PerfettoTracedProcess::Get()->ClearDataSourcesForTesting();
    threaded_service_ = std::make_unique<ThreadedPerfettoService>();

    matching_packet_count_ = 0;
    total_bytes_received_ = 0;
  }

  void TearDown() override {
    tracing_environment_.reset();
    threaded_service_.reset();
    task_environment_->RunUntilIdle();
    test_handle_.reset();
    task_environment_.reset();
  }

  // mojo::DataPipeDrainer::Client
  void OnDataAvailable(base::span<const uint8_t> data) override {
    total_bytes_received_ += data.size();
    base::Extend(received_data_, data);
  }

  // mojo::DataPipeDrainer::Client
  void OnDataComplete() override {
    if (expect_json_data_) {
      std::string output(reinterpret_cast<const char*>(received_data_.data()),
                         received_data_.size());
      if (output.find(packet_testing_str_) != std::string::npos) {
        matching_packet_count_++;
      }
    } else {
      auto proto = std::make_unique<perfetto::protos::Trace>();
      EXPECT_TRUE(
          proto->ParseFromArray(received_data_.data(), received_data_.size()));

      for (int i = 0; i < proto->packet_size(); ++i) {
        if (proto->packet(i).for_testing().str() == packet_testing_str_) {
          matching_packet_count_++;
        }
      }
    }

    if (on_data_complete_) {
      std::move(on_data_complete_).Run();
    }
  }

  void ExpectPackets(const std::string& testing_str,
                     base::OnceClosure on_data_complete) {
    on_data_complete_ = std::move(on_data_complete);
    packet_testing_str_ = testing_str;
    matching_packet_count_ = 0;
  }

  void ReadBuffers() {
    MojoCreateDataPipeOptions options = {sizeof(MojoCreateDataPipeOptions),
                                         MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1, 0};
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    MojoResult rv = mojo::CreateDataPipe(&options, producer, consumer);
    ASSERT_EQ(MOJO_RESULT_OK, rv);
    threaded_service_->ReadBuffers(std::move(producer), base::OnceClosure());
    drainer_ =
        std::make_unique<mojo::DataPipeDrainer>(this, std::move(consumer));
  }

  void DisableTracingAndEmitJson(base::OnceClosure write_callback,
                                 bool enable_privacy_filtering = false) {
    expect_json_data_ = true;
    MojoCreateDataPipeOptions options = {sizeof(MojoCreateDataPipeOptions),
                                         MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1, 0};
    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    MojoResult rv = mojo::CreateDataPipe(&options, producer, consumer);
    ASSERT_EQ(MOJO_RESULT_OK, rv);
    threaded_service_->DisableTracingAndEmitJson(std::move(producer),
                                                 std::move(write_callback),
                                                 enable_privacy_filtering);
    drainer_ =
        std::make_unique<mojo::DataPipeDrainer>(this, std::move(consumer));
  }

  perfetto::TraceConfig GetDefaultTraceConfig(
      const std::string& data_source_name,
      perfetto::protos::gen::ChromeConfig::ClientPriority priority =
          perfetto::protos::gen::ChromeConfig::UNKNOWN) {
    perfetto::TraceConfig trace_config;
    trace_config.add_buffers()->set_size_kb(32 * 1024);

    auto* trace_event_config =
        trace_config.add_data_sources()->mutable_config();
    trace_event_config->set_name(data_source_name);
    trace_event_config->set_target_buffer(0);
    trace_event_config->mutable_chrome_config()->set_client_priority(priority);

    return trace_config;
  }

  void EnableTracingWithDataSourceName(const std::string& data_source_name,
                                       bool enable_privacy_filtering = false,
                                       bool convert_to_legacy_json = false) {
    perfetto::TraceConfig config = GetDefaultTraceConfig(data_source_name);
    if (enable_privacy_filtering) {
      for (auto& source : *config.mutable_data_sources()) {
        source.mutable_config()
            ->mutable_chrome_config()
            ->set_privacy_filtering_enabled(true);
      }
    }
    if (convert_to_legacy_json) {
      for (auto& source : *config.mutable_data_sources()) {
        source.mutable_config()
            ->mutable_chrome_config()
            ->set_convert_to_legacy_json(true);
      }
    }
    threaded_service_->EnableTracingWithConfig(config);
  }

  bool IsTracingEnabled() {
    // Flush any other pending tasks on the perfetto task runner to ensure that
    // any pending data source start callbacks have propagated.
    task_environment_->RunUntilIdle();

    return threaded_service_->IsTracingEnabled();
  }

  size_t matching_packet_count() const { return matching_packet_count_; }
  size_t total_bytes_received() const { return total_bytes_received_; }
  ThreadedPerfettoService* threaded_perfetto_service() const {
    return threaded_service_.get();
  }

 private:
  std::unique_ptr<ThreadedPerfettoService> threaded_service_;
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  std::unique_ptr<base::test::TracingEnvironment> tracing_environment_;
  std::unique_ptr<PerfettoTracedProcess::TestHandle> test_handle_;
  base::OnceClosure on_data_complete_;
  std::unique_ptr<mojo::DataPipeDrainer> drainer_;
  std::vector<uint8_t> received_data_;
  std::string packet_testing_str_;
  size_t matching_packet_count_ = 0;
  size_t total_bytes_received_ = 0;
  bool expect_json_data_ = false;
};

TEST_F(TracingConsumerTest, EnableAndDisableTracing) {
  EnableTracingWithDataSourceName(kDataSourceName);

  base::RunLoop no_more_data;
  ExpectPackets(kPerfettoTestString, no_more_data.QuitClosure());

  threaded_perfetto_service()->DisableTracing();
  ReadBuffers();

  no_more_data.Run();

  EXPECT_EQ(0u, matching_packet_count());
}

TEST_F(TracingConsumerTest, ReceiveTestPackets) {
  EnableTracingWithDataSourceName(kDataSourceName);

  base::RunLoop wait_for_tracing_start;
  threaded_perfetto_service()->CreateProducer(
      kDataSourceName, 10u, wait_for_tracing_start.QuitClosure());

  wait_for_tracing_start.Run();

  base::RunLoop no_more_data;
  ExpectPackets(kPerfettoTestString, no_more_data.QuitClosure());

  threaded_perfetto_service()->DisableTracing();
  ReadBuffers();

  no_more_data.Run();

  EXPECT_EQ(10u, matching_packet_count());
}

TEST_F(TracingConsumerTest, DeleteConsumerWhenReceiving) {
  EnableTracingWithDataSourceName(kDataSourceName);

  base::RunLoop wait_for_tracing_start;
  threaded_perfetto_service()->CreateProducer(
      kDataSourceName, 100u, wait_for_tracing_start.QuitClosure());

  wait_for_tracing_start.Run();

  base::RunLoop no_more_data;
  ExpectPackets(kPerfettoTestString, no_more_data.QuitClosure());

  threaded_perfetto_service()->DisableTracing();
  ReadBuffers();

  threaded_perfetto_service()->ClearConsumer();
  no_more_data.Run();
}

TEST_F(TracingConsumerTest, FlushProducers) {
  EnableTracingWithDataSourceName(kDataSourceName);

  base::RunLoop wait_for_tracing_start;
  threaded_perfetto_service()->CreateProducer(
      kDataSourceName, 10u, wait_for_tracing_start.QuitClosure());

  wait_for_tracing_start.Run();

  base::RunLoop wait_for_packets;
  ExpectPackets(kPerfettoTestString, wait_for_packets.QuitClosure());

  base::RunLoop wait_for_flush;
  threaded_perfetto_service()->Flush(wait_for_flush.QuitClosure());
  ReadBuffers();

  wait_for_flush.Run();
  wait_for_packets.Run();

  EXPECT_EQ(10u, matching_packet_count());
}

TEST_F(TracingConsumerTest, LargeDataSize) {
  EnableTracingWithDataSourceName(kDataSourceName);

  base::RunLoop wait_for_tracing_start;
  threaded_perfetto_service()->CreateProducer(
      kDataSourceName, 0u, wait_for_tracing_start.QuitClosure());

  wait_for_tracing_start.Run();

  base::RunLoop no_more_data;
  ExpectPackets(kPerfettoTestString, no_more_data.QuitClosure());

  threaded_perfetto_service()->WritePacketBigly();

  threaded_perfetto_service()->DisableTracing();
  ReadBuffers();

  no_more_data.Run();

  EXPECT_GE(total_bytes_received(), kLargeMessageSize);
}

TEST_F(TracingConsumerTest, NotifiesOnTracingEnabled) {
  threaded_perfetto_service()->SetPidsInitialized();

  EnableTracingWithDataSourceName(kDataSourceName);
  EXPECT_TRUE(IsTracingEnabled());
}

TEST_F(TracingConsumerTest, NotifiesOnTracingEnabledWaitsForProducer) {
  threaded_perfetto_service()->ExpectPid(kProducerPid);
  threaded_perfetto_service()->SetPidsInitialized();

  EnableTracingWithDataSourceName(kDataSourceName);

  // Tracing is only marked as enabled once the expected producer has acked that
  // its data source has started.
  EXPECT_FALSE(IsTracingEnabled());

  base::RunLoop wait_for_tracing_start;
  threaded_perfetto_service()->CreateProducer(
      kDataSourceName, 0u, wait_for_tracing_start.QuitClosure());
  wait_for_tracing_start.Run();

  EXPECT_TRUE(IsTracingEnabled());
}

TEST_F(TracingConsumerTest, NotifiesOnTracingEnabledWaitsForFilteredProducer) {
  threaded_perfetto_service()->ExpectPid(kProducerPid);
  threaded_perfetto_service()->SetPidsInitialized();

  // Filter for the expected producer.
  auto config = GetDefaultTraceConfig(kDataSourceName);
  *config.mutable_data_sources()->front().add_producer_name_filter() =
      base::StrCat({mojom::kPerfettoProducerNamePrefix,
                    base::NumberToString(kProducerPid)});
  threaded_perfetto_service()->EnableTracingWithConfig(config);

  // Tracing is only marked as enabled once the expected producer has acked that
  // its data source has started.
  EXPECT_FALSE(IsTracingEnabled());

  base::RunLoop wait_for_tracing_start;
  threaded_perfetto_service()->CreateProducer(
      kDataSourceName, 0u, wait_for_tracing_start.QuitClosure());
  wait_for_tracing_start.Run();

  EXPECT_TRUE(IsTracingEnabled());
}

TEST_F(TracingConsumerTest,
       NotifiesOnTracingEnabledDoesNotWaitForUnfilteredProducer) {
  threaded_perfetto_service()->ExpectPid(kProducerPid);
  threaded_perfetto_service()->SetPidsInitialized();

  // Filter for an unexpected producer whose PID is not active.
  auto config = GetDefaultTraceConfig(kDataSourceName);
  *config.mutable_data_sources()->front().add_producer_name_filter() =
      base::StrCat({mojom::kPerfettoProducerNamePrefix,
                    base::NumberToString(kProducerPid + 1)});
  threaded_perfetto_service()->EnableTracingWithConfig(config);

  // Tracing should already have been enabled even though the host was told
  // about a service with kProducerPid. Since kProducerPid is not included in
  // the producer_name_filter, the host should not wait for it.
  EXPECT_TRUE(IsTracingEnabled());
}

TEST_F(TracingConsumerTest,
       NotifiesOnTracingEnabledWaitsForProducerAndInitializedPids) {
  threaded_perfetto_service()->ExpectPid(kProducerPid);

  EnableTracingWithDataSourceName(kDataSourceName);

  // Tracing is only marked as enabled once the expected producer has acked that
  // its data source has started and once the PIDs are initialized.
  EXPECT_FALSE(IsTracingEnabled());

  base::RunLoop wait_for_tracing_start;
  threaded_perfetto_service()->CreateProducer(
      kDataSourceName, 0u, wait_for_tracing_start.QuitClosure());
  wait_for_tracing_start.Run();

  EXPECT_FALSE(IsTracingEnabled());

  threaded_perfetto_service()->SetPidsInitialized();
  EXPECT_TRUE(IsTracingEnabled());
}

TEST_F(TracingConsumerTest, PrivacyFilterConfig) {
  EnableTracingWithDataSourceName(kDataSourceName,
                                  /* enable_privacy_filtering =*/true,
                                  /* convert_to_legacy_json =*/false);

  base::RunLoop wait_for_tracing_start;
  threaded_perfetto_service()->CreateProducer(
      kDataSourceName, 10u, wait_for_tracing_start.QuitClosure());

  wait_for_tracing_start.Run();
  EXPECT_TRUE(threaded_perfetto_service()
                  ->GetProducerClientConfig()
                  .chrome_config()
                  .privacy_filtering_enabled());
  base::trace_event::TraceConfig base_config(threaded_perfetto_service()
                                                 ->GetProducerClientConfig()
                                                 .chrome_config()
                                                 .trace_config());
  EXPECT_FALSE(base_config.IsArgumentFilterEnabled());
}

TEST_F(TracingConsumerTest, NoPrivacyFilterWithJsonConversion) {
  EnableTracingWithDataSourceName(kDataSourceName,
                                  /* enable_privacy_filtering =*/false,
                                  /* convert_to_legacy_json =*/true);

  base::RunLoop wait_for_tracing_start;
  threaded_perfetto_service()->CreateProducer(
      kDataSourceName, 10u, wait_for_tracing_start.QuitClosure());

  wait_for_tracing_start.Run();

  EXPECT_FALSE(threaded_perfetto_service()
                   ->GetProducerClientConfig()
                   .chrome_config()
                   .privacy_filtering_enabled());
  base::trace_event::TraceConfig base_config(threaded_perfetto_service()
                                                 ->GetProducerClientConfig()
                                                 .chrome_config()
                                                 .trace_config());
  EXPECT_FALSE(base_config.IsArgumentFilterEnabled());
}

TEST_F(TracingConsumerTest, PrivacyFilterConfigInJson) {
  EnableTracingWithDataSourceName(kDataSourceName,
                                  /* enable_privacy_filtering =*/true,
                                  /* convert_to_legacy_json =*/true);

  base::RunLoop wait_for_tracing_start;
  threaded_perfetto_service()->CreateProducer(
      kDataSourceName, 10u, wait_for_tracing_start.QuitClosure());

  wait_for_tracing_start.Run();

  EXPECT_FALSE(threaded_perfetto_service()
                   ->GetProducerClientConfig()
                   .chrome_config()
                   .privacy_filtering_enabled());
  base::trace_event::TraceConfig base_config(threaded_perfetto_service()
                                                 ->GetProducerClientConfig()
                                                 .chrome_config()
                                                 .trace_config());
  EXPECT_TRUE(base_config.IsArgumentFilterEnabled());

  base::RunLoop no_more_data;
  ExpectPackets("\"trace_processor_stats\":\"__stripped__\"",
                no_more_data.QuitClosure());

  base::RunLoop write_done;
  DisableTracingAndEmitJson(write_done.QuitClosure(),
                            /* enable_privacy_filtering =*/true);

  no_more_data.Run();
  write_done.Run();

  EXPECT_EQ(1u, matching_packet_count());
}

class MockConsumerHost : public mojom::TracingSessionClient {
 public:
  explicit MockConsumerHost(PerfettoService* service)
      : consumer_host_(std::make_unique<ConsumerHost>(service)) {}

  void EnableTracing(const perfetto::TraceConfig& config) {
    mojo::PendingRemote<tracing::mojom::TracingSessionClient>
        tracing_session_client;
    receiver_.Bind(tracing_session_client.InitWithNewPipeAndPassReceiver());

    receiver_.set_disconnect_handler(base::BindOnce(
        &MockConsumerHost::OnConnectionLost, base::Unretained(this)));

    consumer_host_->EnableTracing(
        tracing_session_host_.BindNewPipeAndPassReceiver(),
        std::move(tracing_session_client), config, base::File());
    tracing_session_host_.set_disconnect_handler(base::BindOnce(
        &MockConsumerHost::OnConnectionLost, base::Unretained(this)));
  }

  void DisableTracing() { tracing_session_host_->DisableTracing(); }

  void OnConnectionLost() {
    CloseTracingSession();
    wait_for_connection_lost_.Quit();
  }

  void CloseTracingSession() {
    tracing_session_host_.reset();
    receiver_.reset();
  }

  // mojom::TracingSessionClient implementation:
  void OnTracingEnabled() override { wait_for_tracing_enabled_.Quit(); }

  void OnTracingDisabled(bool) override { wait_for_tracing_disabled_.Quit(); }

  void WaitForConnectionLost() { wait_for_connection_lost_.Run(); }

  void WaitForTracingEnabled() { wait_for_tracing_enabled_.Run(); }

  void WaitForTracingDisabled() { wait_for_tracing_disabled_.Run(); }

 private:
  mojo::Remote<tracing::mojom::TracingSessionHost> tracing_session_host_;
  mojo::Receiver<mojom::TracingSessionClient> receiver_{this};
  std::unique_ptr<ConsumerHost> consumer_host_;
  base::RunLoop wait_for_connection_lost_;
  base::RunLoop wait_for_tracing_enabled_;
  base::RunLoop wait_for_tracing_disabled_;
};

}  // namespace tracing
