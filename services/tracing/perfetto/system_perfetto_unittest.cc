// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cerrno>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/perfetto/producer_host.h"
#include "services/tracing/perfetto/system_test_utils.h"
#include "services/tracing/perfetto/test_utils.h"
#include "services/tracing/public/cpp/perfetto/dummy_producer.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/ipc/default_socket.h"
#include "third_party/perfetto/protos/perfetto/config/trace_config.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pb.h"

namespace tracing {

namespace {

const char kPerfettoTestDataSourceName[] =
    "org.chromium.chrome_integration_unittest";
const char kPerfettoProducerName[] = "org.chromium.perfetto_producer.123";

std::string RandomASCII(size_t length) {
  std::string tmp;
  for (size_t i = 0; i < length; ++i) {
    tmp += base::RandInt('a', 'z');
  }
  return tmp;
}

class SaveSystemProducerAndScopedRestore {
 public:
  SaveSystemProducerAndScopedRestore()
      : saved_producer_(
            PerfettoTracedProcess::Get()->SetSystemProducerForTesting(
                std::make_unique<DummyProducer>(
                    PerfettoTracedProcess::GetTaskRunner()))) {}

  ~SaveSystemProducerAndScopedRestore() {
    base::RunLoop destroy;
    PerfettoTracedProcess::GetTaskRunner()
        ->GetOrCreateTaskRunner()
        ->PostTaskAndReply(
            FROM_HERE, base::BindLambdaForTesting([this]() {
              PerfettoTracedProcess::Get()
                  ->SetSystemProducerForTesting(std::move(saved_producer_))
                  .reset();
            }),
            destroy.QuitClosure());
    destroy.Run();
  }

 private:
  std::unique_ptr<SystemProducer> saved_producer_;
};

class SystemPerfettoTest : public testing::Test {
 public:
  SystemPerfettoTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
    PerfettoTracedProcess::ResetTaskRunnerForTesting();
    PerfettoTracedProcess::Get()->ClearDataSourcesForTesting();

    EXPECT_TRUE(tmp_dir_.CreateUniqueTempDir());
    // We need to set TMPDIR environment variable because when a new producer
    // connects to the perfetto service it needs to create a memmap'd file for
    // the shared memory buffer. Setting TMPDIR allows the service to know
    // where this should be.
    //
    // Finally since environment variables are leaked into other tests if
    // multiple tests run we need to restore the value so each test is
    // hermetic.
    old_tmp_dir_ = getenv("TMPDIR");
    setenv("TMPDIR", tmp_dir_.GetPath().value().c_str(), true);
    // Set up the system socket locations in a valid tmp directory.
    producer_socket_ =
        tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("producer")).value();
    consumer_socket_ =
        tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("consumer")).value();

    // Construct three default TestDataSources which write out different amount
    // of packets to make it easy to verify which data source has written
    // packets.
    data_sources_.push_back(TestDataSource::CreateAndRegisterDataSource(
        kPerfettoTestDataSourceName, 1));
    data_sources_.push_back(TestDataSource::CreateAndRegisterDataSource(
        base::StrCat({kPerfettoTestDataSourceName, "1"}), 3));
    data_sources_.push_back(TestDataSource::CreateAndRegisterDataSource(
        base::StrCat({kPerfettoTestDataSourceName, "2"}), 7));

    // Construct the service and wait for it to completely set up.
    perfetto_service_ = std::make_unique<PerfettoService>();
    RunUntilIdle();
  }

  std::unique_ptr<MockAndroidSystemProducer> CreateMockAndroidSystemProducer(
      MockSystemService* service,
      int num_data_sources_expected = 0,
      base::RunLoop* system_data_source_enabled_runloop = nullptr,
      base::RunLoop* system_data_source_disabled_runloop = nullptr,
      bool check_sdk_level = false) {
    std::unique_ptr<MockAndroidSystemProducer> result;
    base::RunLoop loop_finished;
    // When we construct a MockAndroidSystemProducer it needs to be on the
    // correct sequence. Construct it on the task runner and wait on the
    // |loop_finished| to ensure it is completely set up.
    PerfettoTracedProcess::GetTaskRunner()
        ->GetOrCreateTaskRunner()
        ->PostTaskAndReply(
            FROM_HERE, base::BindLambdaForTesting([&]() {
              result.reset(new MockAndroidSystemProducer(
                  service->producer(), check_sdk_level,
                  num_data_sources_expected,
                  system_data_source_enabled_runloop
                      ? base::BindOnce(
                            [](base::RunLoop* loop) { loop->Quit(); },
                            system_data_source_enabled_runloop)
                      : base::OnceClosure(),
                  system_data_source_disabled_runloop
                      ? base::BindOnce(
                            [](base::RunLoop* loop) { loop->Quit(); },
                            system_data_source_disabled_runloop)
                      : base::OnceClosure()));
            }),
            loop_finished.QuitClosure());
    loop_finished.Run();
    DCHECK(result);
    return result;
  }

  std::unique_ptr<MockSystemService> CreateMockSystemService() {
    return std::make_unique<MockSystemService>(consumer_socket_,
                                               producer_socket_);
  }

  ~SystemPerfettoTest() override {
    RunUntilIdle();
    // The producer client will be destroyed in the next iteration of the test,
    // but the sequence it was used on disappears with the
    // |task_environment_|. So we reset the sequence so it can be freely
    // destroyed.
    PerfettoTracedProcess::Get()->producer_client()->ResetSequenceForTesting();
    if (old_tmp_dir_) {
      // Restore the old value back to its initial value.
      setenv("TMPDIR", old_tmp_dir_, true);
    } else {
      // TMPDIR wasn't set originally so unset it.
      unsetenv("TMPDIR");
    }
  }

  PerfettoService* local_service() const { return perfetto_service_.get(); }
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  // Fork() + executes the perfetto cmdline client with the given args and
  // returns true if we exited with a success otherwise |stderr_| is populated
  // with the reason for the failure.
  bool ExecPerfetto(std::initializer_list<std::string> args,
                    std::string config) {
    stderr_.clear();
    base::CommandLine cmd(base::FilePath("/system/bin/perfetto"));
    for (auto& arg : args) {
      cmd.AppendArg(std::move(arg));
    }
    std::string config_path =
        tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("trace_config")).value();
    config_path += RandomASCII(16);
    cmd.AppendArgPath(base::FilePath(config_path));
    base::File config_file(
        base::FilePath(config_path),
        base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    if (!config_file.IsValid()) {
      stderr_ = "Tried to create ";
      stderr_ += config_path;
      stderr_ += " but failed with error ";
      stderr_ += base::File::ErrorToString(config_file.error_details());
      return false;
    }
    size_t written = config_file.Write(0, config.data(), config.size());
    if (written != config.size()) {
      stderr_ = base::StrCat({"Expected ", base::NumberToString(config.size()),
                              " bytes written but actually wrote ",
                              base::NumberToString(written)});
      return false;
    }
    config_file.Close();

    bool succeeded = base::GetAppOutputAndError(cmd, &stderr_);
    if (!succeeded) {
      stderr_ +=
          " !!! end of |stderr_| this was generated by the commandline: ";
      stderr_ += cmd.GetCommandLineString();
    }
    // This just cleans up the config file we generated above.
    EXPECT_EQ(0, remove(config_path.c_str()));
    return succeeded;
  }

 protected:
  // |tmp_dir_| must be destroyed last. So must be declared first.
  base::ScopedTempDir tmp_dir_;
  std::string producer_socket_;
  std::string consumer_socket_;
  std::unique_ptr<PerfettoService> perfetto_service_;
  std::vector<std::unique_ptr<TestDataSource>> data_sources_;
  base::test::TaskEnvironment task_environment_;
  std::string stderr_;
  const char* old_tmp_dir_ = nullptr;
};

TEST_F(SystemPerfettoTest, SystemTraceEndToEnd) {
  auto system_service = CreateMockSystemService();

  // Set up the producer to talk to the system.
  base::RunLoop system_data_source_enabled_runloop;
  base::RunLoop system_data_source_disabled_runloop;
  auto system_producer = CreateMockAndroidSystemProducer(
      system_service.get(),
      /* num_data_sources = */ 1, &system_data_source_enabled_runloop,
      &system_data_source_disabled_runloop);

  // Start a system trace, and wait on the Data Source being started.
  base::RunLoop system_no_more_packets_runloop;
  MockConsumer system_consumer(
      {kPerfettoTestDataSourceName}, system_service->GetService(),
      [&system_no_more_packets_runloop](bool has_more) {
        if (!has_more) {
          system_no_more_packets_runloop.Quit();
        }
      });
  system_data_source_enabled_runloop.Run();
  system_consumer.WaitForAllDataSourcesStarted();

  // Post a task to ensure we stop the trace after the data is written.
  base::RunLoop stop_tracing;
  PerfettoTracedProcess::GetTaskRunner()->PostTask(
      [&system_consumer, &stop_tracing]() {
        system_consumer.StopTracing();
        stop_tracing.Quit();
      });
  stop_tracing.Run();

  system_data_source_disabled_runloop.Run();
  system_no_more_packets_runloop.Run();
  system_consumer.WaitForAllDataSourcesStopped();

  EXPECT_EQ(1u, system_consumer.received_test_packets());
  PerfettoProducer::DeleteSoonForTesting(std::move(system_producer));
}

// TODO(crbug/964324): We need to run this test in permissive mode, but
// currently the bots don't do that. We should switch this to a telemetry
// test to ensure our integration works on P+ Android devices.
TEST_F(SystemPerfettoTest, DISABLED_SystemTraceEndToEndRealService) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_P) {
    LOG(INFO)
        << "Skipping SystemTraceEndToEndRealService test, this phone "
        << "is pre P SDK, which means there is no 'real' service running.";
    return;
  }

  perfetto::protos::TraceConfig trace_config;
  trace_config.add_buffers()->set_size_kb(1024);
  trace_config.add_data_sources()->mutable_config()->set_name(
      data_sources_[0]->name());
  trace_config.add_data_sources()->mutable_config()->set_name(
      data_sources_[2]->name());
  trace_config.set_duration_ms(100);

  std::string path = "/data/misc/perfetto-traces/trace";
  path += RandomASCII(16);
  EXPECT_TRUE(
      ExecPerfetto({"-o", path, "-c"}, trace_config.SerializeAsString()))
      << "failed with stderr: \"" << stderr_ << "\"";

  char* data = new char[1024 * 1024];
  ASSERT_TRUE(data);
  int num_bytes = base::ReadFile(base::FilePath(path), data, (1024 * 1024) - 1);
  EXPECT_NE(num_bytes, -1);
  std::string output(data, num_bytes);
  delete[] data;

  perfetto::protos::Trace trace;
  EXPECT_TRUE(trace.ParseFromString(output));

  int count = 0;
  for (const auto& packet : trace.packet()) {
    if (packet.has_for_testing()) {
      ++count;
    }
  }
  EXPECT_EQ(1 + 7, count);
  EXPECT_EQ(0, remove(path.c_str()));
}

TEST_F(SystemPerfettoTest, OneSystemSourceWithMultipleLocalSources) {
  auto system_service = CreateMockSystemService();

  // Start a trace using the system Perfetto service.
  base::RunLoop system_no_more_packets_runloop;
  MockConsumer system_consumer(
      {kPerfettoTestDataSourceName}, system_service->GetService(),
      [&system_no_more_packets_runloop](bool has_more) {
        if (!has_more) {
          system_no_more_packets_runloop.Quit();
        }
      });

  base::RunLoop system_data_source_enabled_runloop;
  base::RunLoop system_data_source_disabled_runloop;
  auto system_producer = CreateMockAndroidSystemProducer(
      system_service.get(),
      /* num_data_sources = */ 1, &system_data_source_enabled_runloop,
      &system_data_source_disabled_runloop);

  system_data_source_enabled_runloop.Run();
  system_consumer.WaitForAllDataSourcesStarted();

  // Now start the local trace and wait for the system trace to stop first.
  base::RunLoop local_data_source_enabled_runloop;
  base::RunLoop local_data_source_disabled_runloop;
  base::RunLoop local_no_more_packets_runloop;
  auto local_producer_client = std::make_unique<MockProducerClient>(
      /* num_data_sources = */ 3,
      local_data_source_enabled_runloop.QuitClosure(),
      local_data_source_disabled_runloop.QuitClosure());
  MockConsumer local_consumer(
      {kPerfettoTestDataSourceName,
       base::StrCat({kPerfettoTestDataSourceName, "1"}),
       base::StrCat({kPerfettoTestDataSourceName, "2"})},
      local_service()->GetService(),
      [&local_no_more_packets_runloop](bool has_more) {
        if (!has_more) {
          local_no_more_packets_runloop.Quit();
        }
      });
  auto local_producer_host = std::make_unique<MockProducerHost>(
      kPerfettoProducerName, kPerfettoTestDataSourceName,
      local_service()->GetService(), local_producer_client.get());

  system_consumer.WaitForAllDataSourcesStopped();
  system_data_source_disabled_runloop.Run();
  local_consumer.WaitForAllDataSourcesStarted();
  local_data_source_enabled_runloop.Run();

  // Ensures that the Trace data gets written and committed.
  RunUntilIdle();

  // Once we StopTracing() on the local trace the system tracing system will
  // come back. So set new enabled and disabled RunLoops for the system
  // producer.
  base::RunLoop system_data_source_reenabled_runloop;
  base::RunLoop system_data_source_redisabled_runloop;
  system_producer->SetDataSourceEnabledCallback(
      system_data_source_reenabled_runloop.QuitClosure());
  system_producer->SetDataSourceDisabledCallback(
      system_data_source_redisabled_runloop.QuitClosure());
  base::RunLoop system_data_source_wrote_data_runloop;
  data_sources_[0]->set_start_tracing_callback(
      system_data_source_wrote_data_runloop.QuitClosure());

  local_consumer.StopTracing();
  local_data_source_disabled_runloop.Run();
  local_consumer.WaitForAllDataSourcesStopped();
  local_no_more_packets_runloop.Run();

  // Wait for system tracing to return before stopping the trace on the correct
  // sequence to ensure everything is committed.
  system_data_source_reenabled_runloop.Run();
  system_consumer.WaitForAllDataSourcesStarted();
  base::RunLoop stop_tracing;
  PerfettoTracedProcess::GetTaskRunner()->PostTask(
      [&system_consumer, &stop_tracing]() {
        system_consumer.StopTracing();
        stop_tracing.Quit();
      });
  stop_tracing.Run();

  system_data_source_redisabled_runloop.Run();
  system_consumer.WaitForAllDataSourcesStopped();
  system_no_more_packets_runloop.Run();

  // |local_consumer| should have seen one |send_packet_count_| from each data
  // source, whereas |system_consumer| should see 2 packets from the first data
  // source having been started twice.
  EXPECT_EQ(1u + 3u + 7u, local_consumer.received_test_packets());
  EXPECT_EQ(2u, system_consumer.received_test_packets());

  PerfettoProducer::DeleteSoonForTesting(std::move(local_producer_client));
  PerfettoProducer::DeleteSoonForTesting(std::move(system_producer));
}

TEST_F(SystemPerfettoTest, MultipleSystemSourceWithOneLocalSourcesLocalFirst) {
  auto system_service = CreateMockSystemService();

  base::RunLoop local_no_more_packets_runloop;
  std::unique_ptr<MockConsumer> local_consumer(
      new MockConsumer({base::StrCat({kPerfettoTestDataSourceName, "2"})},
                       local_service()->GetService(),
                       [&local_no_more_packets_runloop](bool has_more) {
                         if (!has_more) {
                           local_no_more_packets_runloop.Quit();
                         }
                       }));
  // Now start the local trace and wait for the system trace to stop first.
  base::RunLoop local_data_source_enabled_runloop;
  base::RunLoop local_data_source_disabled_runloop;
  auto local_producer_client = std::make_unique<MockProducerClient>(
      /* num_data_sources = */ 1,
      local_data_source_enabled_runloop.QuitClosure(),
      local_data_source_disabled_runloop.QuitClosure());
  auto local_producer_host = std::make_unique<MockProducerHost>(
      kPerfettoProducerName, kPerfettoTestDataSourceName,
      local_service()->GetService(), local_producer_client.get());

  local_data_source_enabled_runloop.Run();
  local_consumer->WaitForAllDataSourcesStarted();

  // Ensures that the Trace data gets written and committed.
  RunUntilIdle();

  local_consumer->StopTracing();
  local_data_source_disabled_runloop.Run();
  local_consumer->WaitForAllDataSourcesStopped();
  local_no_more_packets_runloop.Run();
  EXPECT_EQ(7u, local_consumer->received_test_packets());

  // Because we can't just use |system_data_source_enabled| because they might
  // attempt to enable but be queued until the local trace has fully finished.
  // We therefore need to set callbacks explicitly after the data has been
  // written.
  std::vector<base::RunLoop> data_sources_wrote_data{data_sources_.size()};
  for (size_t i = 0; i < data_sources_.size(); ++i) {
    data_sources_[i]->set_start_tracing_callback(
        data_sources_wrote_data[i].QuitClosure());
  }

  // Start a trace using the system Perfetto service.
  base::RunLoop system_no_more_packets_runloop;
  MockConsumer system_consumer(
      {kPerfettoTestDataSourceName,
       base::StrCat({kPerfettoTestDataSourceName, "1"}),
       base::StrCat({kPerfettoTestDataSourceName, "2"})},
      system_service->GetService(),
      [&system_no_more_packets_runloop](bool has_more) {
        if (!has_more) {
          system_no_more_packets_runloop.Quit();
        }
      });

  base::RunLoop system_data_source_enabled_runloop;
  base::RunLoop system_data_source_disabled_runloop;
  auto system_producer = CreateMockAndroidSystemProducer(
      system_service.get(),
      /* num_data_sources = */ 3, &system_data_source_enabled_runloop,
      &system_data_source_disabled_runloop);

  system_data_source_enabled_runloop.Run();
  for (auto& loop : data_sources_wrote_data) {
    loop.Run();
  }
  system_consumer.WaitForAllDataSourcesStarted();

  // Wait for system tracing to return before stopping the trace on the correct
  // sequence to ensure everything is committed.
  base::RunLoop stop_tracing;
  PerfettoTracedProcess::GetTaskRunner()->PostTask(
      [&system_consumer, &stop_tracing]() {
        system_consumer.StopTracing();
        stop_tracing.Quit();
      });
  stop_tracing.Run();

  system_data_source_disabled_runloop.Run();
  system_consumer.WaitForAllDataSourcesStopped();
  system_no_more_packets_runloop.Run();

  // Once we StopTracing() on the system trace the we want to make sure a new
  // local trace can start smoothly. So set new enabled and disabled RunLoops
  // for the system producer.
  base::RunLoop local_data_source_reenabled_runloop;
  base::RunLoop local_data_source_redisabled_runloop;
  local_producer_client->SetAgentEnabledCallback(
      local_data_source_reenabled_runloop.QuitClosure());
  local_producer_client->SetAgentDisabledCallback(
      local_data_source_redisabled_runloop.QuitClosure());

  local_consumer->FreeBuffers();
  local_consumer->StartTracing();

  local_data_source_reenabled_runloop.Run();
  local_consumer->WaitForAllDataSourcesStarted();
  local_consumer->StopTracing();
  local_consumer->WaitForAllDataSourcesStopped();
  local_data_source_redisabled_runloop.Run();

  // |local_consumer| should have seen one |send_packet_count_| from each data
  // source, whereas |system_consumer| should see 2 packets from the first data
  // source having been started twice.
  EXPECT_EQ(14u, local_consumer->received_test_packets());
  EXPECT_EQ(1u + 3u + 7u, system_consumer.received_test_packets());

  PerfettoProducer::DeleteSoonForTesting(std::move(local_producer_client));
  PerfettoProducer::DeleteSoonForTesting(std::move(system_producer));
}

TEST_F(SystemPerfettoTest, MultipleSystemAndLocalSources) {
  auto system_service = CreateMockSystemService();

  // Start a trace using the system Perfetto service.
  base::RunLoop system_no_more_packets_runloop;
  MockConsumer system_consumer(
      {kPerfettoTestDataSourceName,
       base::StrCat({kPerfettoTestDataSourceName, "1"}),
       base::StrCat({kPerfettoTestDataSourceName, "2"})},
      system_service->GetService(),
      [&system_no_more_packets_runloop](bool has_more) {
        if (!has_more) {
          system_no_more_packets_runloop.Quit();
        }
      });

  base::RunLoop system_data_source_enabled_runloop;
  base::RunLoop system_data_source_disabled_runloop;
  auto system_producer = CreateMockAndroidSystemProducer(
      system_service.get(),
      /* num_data_sources = */ 3, &system_data_source_enabled_runloop,
      &system_data_source_disabled_runloop);

  system_data_source_enabled_runloop.Run();
  system_consumer.WaitForAllDataSourcesStarted();

  // Now start the local trace and wait for the system trace to stop first.
  base::RunLoop local_data_source_enabled_runloop;
  base::RunLoop local_data_source_disabled_runloop;
  base::RunLoop local_no_more_packets_runloop;
  auto local_producer_client = std::make_unique<MockProducerClient>(
      /* num_data_sources = */ 3,
      local_data_source_enabled_runloop.QuitClosure(),
      local_data_source_disabled_runloop.QuitClosure());
  auto local_producer_host = std::make_unique<MockProducerHost>(
      kPerfettoProducerName, kPerfettoTestDataSourceName,
      local_service()->GetService(), local_producer_client.get());
  MockConsumer local_consumer(
      {kPerfettoTestDataSourceName,
       base::StrCat({kPerfettoTestDataSourceName, "1"}),
       base::StrCat({kPerfettoTestDataSourceName, "2"})},
      local_service()->GetService(),
      [&local_no_more_packets_runloop](bool has_more) {
        if (!has_more) {
          local_no_more_packets_runloop.Quit();
        }
      });

  system_data_source_disabled_runloop.Run();
  system_consumer.WaitForAllDataSourcesStopped();
  local_data_source_enabled_runloop.Run();
  local_consumer.WaitForAllDataSourcesStarted();

  // Ensures that the Trace data gets written and committed.
  RunUntilIdle();

  // Once we StopTracing() on the local trace the system tracing system will
  // come back. So set new enabled and disabled RunLoops for the system
  // producer.
  base::RunLoop system_data_source_reenabled_runloop;
  base::RunLoop system_data_source_redisabled_runloop;
  system_producer->SetDataSourceEnabledCallback(
      system_data_source_reenabled_runloop.QuitClosure());
  system_producer->SetDataSourceDisabledCallback(
      system_data_source_redisabled_runloop.QuitClosure());

  local_consumer.StopTracing();
  local_data_source_disabled_runloop.Run();
  local_consumer.WaitForAllDataSourcesStopped();
  local_no_more_packets_runloop.Run();

  // Wait for system tracing to return before stopping.
  system_data_source_reenabled_runloop.Run();
  system_consumer.WaitForAllDataSourcesStarted();

  base::RunLoop stop_tracing;
  PerfettoTracedProcess::GetTaskRunner()->PostTask(
      [&system_consumer, &stop_tracing]() {
        system_consumer.StopTracing();
        stop_tracing.Quit();
      });
  stop_tracing.Run();

  system_data_source_redisabled_runloop.Run();
  system_no_more_packets_runloop.Run();

  // |local_consumer| should have seen one |send_packet_count_| from each data
  // source, whereas |system_consumer| should see 2 packets from each since it
  // got started twice.
  EXPECT_EQ(1u + 3u + 7u, local_consumer.received_test_packets());
  EXPECT_EQ((1u + 3u + 7u) * 2, system_consumer.received_test_packets());

  PerfettoProducer::DeleteSoonForTesting(std::move(local_producer_client));
  PerfettoProducer::DeleteSoonForTesting(std::move(system_producer));
}

TEST_F(SystemPerfettoTest, MultipleSystemAndLocalSourcesLocalFirst) {
  auto system_service = CreateMockSystemService();

  // We construct it up front so it connects to the service before the local
  // trace starts.
  base::RunLoop system_data_source_enabled_runloop;
  base::RunLoop system_data_source_disabled_runloop;
  auto system_producer = CreateMockAndroidSystemProducer(
      system_service.get(),
      /* num_data_sources = */ 3, &system_data_source_enabled_runloop,
      &system_data_source_disabled_runloop);

  // Now start the local trace and wait for the system trace to stop first.
  base::RunLoop local_data_source_enabled_runloop;
  base::RunLoop local_data_source_disabled_runloop;
  base::RunLoop local_no_more_packets_runloop;
  auto local_producer_client = std::make_unique<MockProducerClient>(
      /* num_data_sources = */ 3,
      local_data_source_enabled_runloop.QuitClosure(),
      local_data_source_disabled_runloop.QuitClosure());
  auto local_producer_host = std::make_unique<MockProducerHost>(
      kPerfettoProducerName, kPerfettoTestDataSourceName,
      local_service()->GetService(), local_producer_client.get());
  MockConsumer local_consumer(
      {kPerfettoTestDataSourceName,
       base::StrCat({kPerfettoTestDataSourceName, "1"}),
       base::StrCat({kPerfettoTestDataSourceName, "2"})},
      local_service()->GetService(),
      [&local_no_more_packets_runloop](bool has_more) {
        if (!has_more) {
          local_no_more_packets_runloop.Quit();
        }
      });

  local_data_source_enabled_runloop.Run();
  local_consumer.WaitForAllDataSourcesStarted();

  // Ensures that the Trace data gets written and committed.
  RunUntilIdle();

  // Because we can't just use |system_data_source_enabled| because they might
  // attempt to enable but be queued until the local trace has fully finished.
  // We therefore need to set callbacks explicitly after the data has been
  // written.
  std::vector<base::RunLoop> data_sources_wrote_data{data_sources_.size()};
  for (size_t i = 0; i < data_sources_.size(); ++i) {
    data_sources_[i]->set_start_tracing_callback(
        data_sources_wrote_data[i].QuitClosure());
  }

  // Start a trace using the system Perfetto service.
  base::RunLoop system_no_more_packets_runloop;
  MockConsumer system_consumer(
      {kPerfettoTestDataSourceName,
       base::StrCat({kPerfettoTestDataSourceName, "1"}),
       base::StrCat({kPerfettoTestDataSourceName, "2"})},
      system_service->GetService(),
      [&system_no_more_packets_runloop](bool has_more) {
        if (!has_more) {
          system_no_more_packets_runloop.Quit();
        }
      });

  // Post a task to ensure all the connection logic has been run.
  base::RunLoop local_stop_tracing;
  PerfettoTracedProcess::GetTaskRunner()->PostTask(
      [&local_consumer, &local_stop_tracing]() {
        local_consumer.StopTracing();
        local_stop_tracing.Quit();
      });
  local_stop_tracing.Run();

  local_data_source_disabled_runloop.Run();
  local_consumer.WaitForAllDataSourcesStopped();
  local_no_more_packets_runloop.Run();

  // Now the system trace will start.
  system_data_source_enabled_runloop.Run();
  for (auto& loop : data_sources_wrote_data) {
    loop.Run();
  }
  system_consumer.WaitForAllDataSourcesStarted();

  // Wait for system tracing to return before stopping.
  base::RunLoop system_stop_tracing;
  PerfettoTracedProcess::GetTaskRunner()->PostTask(
      [&system_consumer, &system_stop_tracing]() {
        system_consumer.StopTracing();
        system_stop_tracing.Quit();
      });
  system_stop_tracing.Run();

  system_data_source_disabled_runloop.Run();
  system_consumer.WaitForAllDataSourcesStopped();
  system_no_more_packets_runloop.Run();

  // |local_consumer| & |system_consumer| should have seen one
  // |send_packet_count_| from each data source.
  EXPECT_EQ(1u + 3u + 7u, local_consumer.received_test_packets());
  EXPECT_EQ(1u + 3u + 7u, system_consumer.received_test_packets());

  PerfettoProducer::DeleteSoonForTesting(std::move(local_producer_client));
  PerfettoProducer::DeleteSoonForTesting(std::move(system_producer));
}

TEST_F(SystemPerfettoTest, SystemToLowAPILevel) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_P) {
    LOG(INFO) << "Skipping SystemToLowAPILevel test, this phone supports the "
              << "P SDK (or above).";
    // This test will do exactly the same thing on versions beyond P so just
    // exit. Once we are no longer testing on O and below we can remove this
    // test.
    return;
  }

  auto run_test = [this](bool check_sdk_level) {
    PerfettoTracedProcess::Get()->ClearDataSourcesForTesting();

    std::string data_source_name = "temp_name";
    data_source_name += check_sdk_level ? "true" : "false";

    base::RunLoop data_source_started_runloop;
    std::unique_ptr<TestDataSource> data_source =
        TestDataSource::CreateAndRegisterDataSource(data_source_name, 1);
    data_source->set_start_tracing_callback(
        data_source_started_runloop.QuitClosure());

    auto system_service = CreateMockSystemService();

    base::RunLoop system_no_more_packets_runloop;
    MockConsumer system_consumer(
        {data_source_name}, system_service->GetService(),
        [&system_no_more_packets_runloop](bool has_more) {
          if (!has_more) {
            system_no_more_packets_runloop.Quit();
          }
        });

    base::RunLoop system_data_source_enabled_runloop;
    base::RunLoop system_data_source_disabled_runloop;
    auto system_producer = CreateMockAndroidSystemProducer(
        system_service.get(),
        /* num_data_sources = */ 1, &system_data_source_enabled_runloop,
        &system_data_source_disabled_runloop, check_sdk_level);

    if (!check_sdk_level) {
      system_data_source_enabled_runloop.Run();
      data_source_started_runloop.Run();
      system_consumer.WaitForAllDataSourcesStarted();
    }

    // Post the task to ensure that the data will have been written and
    // committed if any tracing is being done.
    base::RunLoop stop_tracing;
    PerfettoTracedProcess::GetTaskRunner()->PostTask(
        [&system_consumer, &stop_tracing]() {
          system_consumer.StopTracing();
          stop_tracing.Quit();
        });
    stop_tracing.Run();

    if (!check_sdk_level) {
      system_data_source_disabled_runloop.Run();
      system_consumer.WaitForAllDataSourcesStopped();
    }
    system_no_more_packets_runloop.Run();

    PerfettoProducer::DeleteSoonForTesting(std::move(system_producer));
    return system_consumer.received_test_packets();
  };

  // If |check_sdk_level| == true, the |system_producer| will not even attempt
  // to connect to the |system_service| and therefore we should see no packets.
  EXPECT_EQ(1u, run_test(/* check_sdk_level = */ false));
  EXPECT_EQ(0u, run_test(/* check_sdk_level = */ true));
}

TEST_F(SystemPerfettoTest, EnabledOnDebugBuilds) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kEnablePerfettoSystemTracing);
  // We have to prevent destroying the system producer because we might have
  // created it on a different task environment (wrong sequence).
  SaveSystemProducerAndScopedRestore saved_system_producer;
  PerfettoTracedProcess::ReconstructForTesting(producer_socket_.c_str());
  if (base::android::BuildInfo::GetInstance()->is_debug_android()) {
    EXPECT_FALSE(PerfettoTracedProcess::Get()
                     ->SystemProducerForTesting()
                     ->IsDummySystemProducerForTesting());
  } else {
    EXPECT_TRUE(PerfettoTracedProcess::Get()
                    ->SystemProducerForTesting()
                    ->IsDummySystemProducerForTesting());
  }
}

TEST_F(SystemPerfettoTest, RespectsFeatureList) {
  if (base::android::BuildInfo::GetInstance()->is_debug_android()) {
    // The feature list is ignored on debug android builds so we should have a
    // real system producer so just bail out of this test.
    EXPECT_FALSE(PerfettoTracedProcess::Get()
                     ->SystemProducerForTesting()
                     ->IsDummySystemProducerForTesting());
    return;
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kEnablePerfettoSystemTracing);
    PerfettoTracedProcess::ReconstructForTesting(producer_socket_.c_str());
    EXPECT_FALSE(PerfettoTracedProcess::Get()
                     ->SystemProducerForTesting()
                     ->IsDummySystemProducerForTesting());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kEnablePerfettoSystemTracing);
    PerfettoTracedProcess::ReconstructForTesting(producer_socket_.c_str());
    EXPECT_TRUE(PerfettoTracedProcess::Get()
                    ->SystemProducerForTesting()
                    ->IsDummySystemProducerForTesting());
  }
}
}  // namespace
}  // namespace tracing
