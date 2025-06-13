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
#include "base/test/gmock_callback_support.h"
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

using testing::_;

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

  void CreateProducer() {
    base::RunLoop wait_for_producer;
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ThreadedPerfettoService::CreateProducerOnSequence,
                       base::Unretained(this),
                       wait_for_producer.QuitClosure()));
    wait_for_producer.Run();
  }

  std::unique_ptr<perfetto::TraceWriter> RegisterDataSource(
      const std::string& data_source_name) {
    base::RunLoop wait_for_datasource;
    std::unique_ptr<perfetto::TraceWriter> trace_writer;
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ThreadedPerfettoService::RegisterDataSourceOnSequence,
                       base::Unretained(this), data_source_name,
                       wait_for_datasource.QuitClosure()));
    wait_for_datasource.Run();
    return trace_writer;
  }

  std::unique_ptr<perfetto::TraceWriter> CreateTraceWriter(
      const std::string& data_source_name) {
    base::RunLoop wait_for_datasource;
    std::unique_ptr<perfetto::TraceWriter> trace_writer;
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ThreadedPerfettoService::CreateTraceWriterOnSequence,
                       base::Unretained(this), data_source_name,
                       base::BindLambdaForTesting(
                           [&](std::unique_ptr<perfetto::TraceWriter> writer) {
                             trace_writer = std::move(writer);
                             wait_for_datasource.Quit();
                           })));
    wait_for_datasource.Run();
    return trace_writer;
  }

  perfetto::DataSourceConfig GetDataSourceConfig(
      const std::string& data_source_name) {
    base::RunLoop wait_for_datasource;
    perfetto::DataSourceConfig ds_config;
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ThreadedPerfettoService::GetDataSourceConfigOnSequence,
            base::Unretained(this), data_source_name,
            base::BindLambdaForTesting([&](perfetto::DataSourceConfig config) {
              ds_config = config;
              wait_for_datasource.Quit();
            })));
    wait_for_datasource.Run();
    return ds_config;
  }

  void CreateConsumerOnSequence() {
    consumer_ = std::make_unique<ConsumerHost>(perfetto_service_.get());
  }

  void CreateProducerOnSequence(base::OnceClosure on_producer_connected) {
    producer_ = std::make_unique<MockProducer>();
    producer_->Connect(perfetto_service_.get(),
                       base::StrCat({mojom::kPerfettoProducerNamePrefix,
                                     base::NumberToString(kProducerPid)}));
    EXPECT_CALL(*producer_, OnConnect())
        .WillOnce(base::test::RunOnceClosure(std::move(on_producer_connected)));
  }

  void RegisterDataSourceOnSequence(const std::string& data_source_name,
                                    base::OnceClosure on_data_source_start) {
    producer_->RegisterDataSource(data_source_name);
    EXPECT_CALL(*producer_, OnStartDataSource(data_source_name, _))
        .WillOnce(base::test::RunOnceClosure(std::move(on_data_source_start)));
  }

  void CreateTraceWriterOnSequence(
      const std::string& data_source_name,
      base::RepeatingCallback<void(std::unique_ptr<perfetto::TraceWriter>)>
          on_data_source_start) {
    producer_->RegisterDataSource(data_source_name);
    EXPECT_CALL(*producer_, OnStartDataSource(data_source_name, _))
        .WillOnce(
            [on_data_source_start, this](const std::string& name,
                                         perfetto::DataSourceInstanceID ds_id) {
              on_data_source_start.Run(producer_->CreateTraceWriter(ds_id));
            });
  }

  void GetDataSourceConfigOnSequence(
      const std::string& data_source_name,
      base::RepeatingCallback<void(perfetto::DataSourceConfig)>
          on_data_source_start) {
    producer_->RegisterDataSource(data_source_name);
    EXPECT_CALL(*producer_, OnStartDataSource(data_source_name, _))
        .WillOnce(
            [on_data_source_start, this](const std::string& name,
                                         perfetto::DataSourceInstanceID ds_id) {
              on_data_source_start.Run(producer_->GetDataSourceConfig(ds_id));
            });
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

  void Flush(perfetto::TraceWriter& writer_to_flush,
             base::OnceClosure on_flush_complete) {
    EXPECT_CALL(*producer_, OnFlush()).WillOnce([&]() {
      writer_to_flush.Flush();
    });
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

class TracingConsumerTest : public testing::Test,
                            public mojo::DataPipeDrainer::Client {
 public:
  void SetUp() override {
    threaded_service_ = std::make_unique<ThreadedPerfettoService>();

    matching_packet_count_ = 0;
    total_bytes_received_ = 0;
  }

  void TearDown() override {
    threaded_service_.reset();
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
    task_environment_.RunUntilIdle();

    return threaded_service_->IsTracingEnabled();
  }

  size_t matching_packet_count() const { return matching_packet_count_; }
  size_t total_bytes_received() const { return total_bytes_received_; }
  ThreadedPerfettoService* threaded_perfetto_service() const {
    return threaded_service_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  TracedProcessForTesting traced_process_{
      base::SingleThreadTaskRunner::GetCurrentDefault()};
  std::unique_ptr<ThreadedPerfettoService> threaded_service_;
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

  threaded_perfetto_service()->CreateProducer();
  auto writer = threaded_perfetto_service()->CreateTraceWriter(kDataSourceName);
  MockProducer::WritePackets(*writer, 10);

  base::RunLoop no_more_data;
  ExpectPackets(kPerfettoTestString, no_more_data.QuitClosure());

  threaded_perfetto_service()->DisableTracing();
  ReadBuffers();

  no_more_data.Run();

  EXPECT_EQ(10u, matching_packet_count());
}

TEST_F(TracingConsumerTest, DeleteConsumerWhenReceiving) {
  EnableTracingWithDataSourceName(kDataSourceName);

  threaded_perfetto_service()->CreateProducer();
  auto writer = threaded_perfetto_service()->CreateTraceWriter(kDataSourceName);
  MockProducer::WritePackets(*writer, 100u);

  base::RunLoop no_more_data;
  ExpectPackets(kPerfettoTestString, no_more_data.QuitClosure());

  threaded_perfetto_service()->DisableTracing();
  ReadBuffers();

  threaded_perfetto_service()->ClearConsumer();
  no_more_data.Run();
}

TEST_F(TracingConsumerTest, FlushProducers) {
  EnableTracingWithDataSourceName(kDataSourceName);

  threaded_perfetto_service()->CreateProducer();
  auto writer = threaded_perfetto_service()->CreateTraceWriter(kDataSourceName);

  base::RunLoop wait_for_packets;
  ExpectPackets(kPerfettoTestString, wait_for_packets.QuitClosure());

  MockProducer::WritePackets(*writer, 10);

  base::RunLoop wait_for_flush;
  threaded_perfetto_service()->Flush(*writer, wait_for_flush.QuitClosure());

  wait_for_flush.Run();
  ReadBuffers();
  wait_for_packets.Run();

  EXPECT_EQ(10u, matching_packet_count());
}

TEST_F(TracingConsumerTest, LargeDataSize) {
  EnableTracingWithDataSourceName(kDataSourceName);

  threaded_perfetto_service()->CreateProducer();
  auto writer = threaded_perfetto_service()->CreateTraceWriter(kDataSourceName);
  MockProducer::WritePacketBigly(*writer);

  base::RunLoop no_more_data;
  ExpectPackets(kPerfettoTestString, no_more_data.QuitClosure());

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

  threaded_perfetto_service()->CreateProducer();
  threaded_perfetto_service()->RegisterDataSource(kDataSourceName);

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

  threaded_perfetto_service()->CreateProducer();
  threaded_perfetto_service()->RegisterDataSource(kDataSourceName);

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

  threaded_perfetto_service()->CreateProducer();
  threaded_perfetto_service()->RegisterDataSource(kDataSourceName);

  EXPECT_FALSE(IsTracingEnabled());

  threaded_perfetto_service()->SetPidsInitialized();
  EXPECT_TRUE(IsTracingEnabled());
}

TEST_F(TracingConsumerTest, PrivacyFilterConfig) {
  EnableTracingWithDataSourceName(kDataSourceName,
                                  /* enable_privacy_filtering =*/true,
                                  /* convert_to_legacy_json =*/false);

  threaded_perfetto_service()->CreateProducer();

  auto config =
      threaded_perfetto_service()->GetDataSourceConfig(kDataSourceName);
  EXPECT_TRUE(config.chrome_config().privacy_filtering_enabled());
  base::trace_event::TraceConfig base_config(
      config.chrome_config().trace_config());
  EXPECT_FALSE(base_config.IsArgumentFilterEnabled());
}

TEST_F(TracingConsumerTest, NoPrivacyFilterWithJsonConversion) {
  EnableTracingWithDataSourceName(kDataSourceName,
                                  /* enable_privacy_filtering =*/false,
                                  /* convert_to_legacy_json =*/true);

  threaded_perfetto_service()->CreateProducer();

  auto config =
      threaded_perfetto_service()->GetDataSourceConfig(kDataSourceName);
  EXPECT_FALSE(config.chrome_config().privacy_filtering_enabled());
  base::trace_event::TraceConfig base_config(
      config.chrome_config().trace_config());
  EXPECT_FALSE(base_config.IsArgumentFilterEnabled());
}

TEST_F(TracingConsumerTest, PrivacyFilterConfigInJson) {
  EnableTracingWithDataSourceName(kDataSourceName,
                                  /* enable_privacy_filtering =*/true,
                                  /* convert_to_legacy_json =*/true);

  threaded_perfetto_service()->CreateProducer();
  auto config =
      threaded_perfetto_service()->GetDataSourceConfig(kDataSourceName);
  EXPECT_FALSE(config.chrome_config().privacy_filtering_enabled());
  base::trace_event::TraceConfig base_config(
      config.chrome_config().trace_config());
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

}  // namespace tracing
