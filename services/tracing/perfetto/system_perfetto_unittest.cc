// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cerrno>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/process/launch.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/trace_event/trace_config.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/perfetto/producer_host.h"
#include "services/tracing/perfetto/system_test_utils.h"
#include "services/tracing/perfetto/test_utils.h"
#include "services/tracing/public/cpp/perfetto/dummy_producer.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "services/tracing/public/cpp/system_tracing_service.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "services/tracing/public/cpp/traced_process_impl.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/tracing/default_socket.h"
#include "third_party/perfetto/protos/perfetto/config/trace_config.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pb.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"  // nogncheck
#endif

namespace tracing {

namespace {

const char kPerfettoTestDataSourceName[] =
    "org.chromium.chrome_integration_unittest";

std::string GetPerfettoProducerName() {
  return base::StrCat({mojom::kPerfettoProducerNamePrefix, "123"});
}

std::string RandomASCII(size_t length) {
  std::string tmp;
  for (size_t i = 0; i < length; ++i) {
    tmp += base::RandInt('a', 'z');
  }
  return tmp;
}

class ClearAndRestoreSystemProducerScope {
 public:
  ClearAndRestoreSystemProducerScope() {
    base::RunLoop setup_loop;
    PerfettoTracedProcess::GetTaskRunner()->GetOrCreateTaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([this, &setup_loop] {
          saved_producer_ =
              PerfettoTracedProcess::Get()->SetSystemProducerForTesting(
                  nullptr);
          setup_loop.Quit();
        }));
    setup_loop.Run();
  }

  ~ClearAndRestoreSystemProducerScope() {
    base::RunLoop destroy_loop;
    PerfettoTracedProcess::GetTaskRunner()->GetOrCreateTaskRunner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([this, &destroy_loop]() {
          PerfettoTracedProcess::Get()
              ->SetSystemProducerForTesting(std::move(saved_producer_))
              .reset();
          destroy_loop.Quit();
        }));
    destroy_loop.Run();
  }

 private:
  std::unique_ptr<SystemProducer> saved_producer_;
};

class SystemPerfettoTest : public TracingUnitTest {
 public:
  void SetUp() override {
    TracingUnitTest::SetUp();

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

  void TearDown() override {
    data_sources_.clear();
    perfetto_service_.reset();

    if (old_tmp_dir_) {
      // Restore the old value back to its initial value.
      setenv("TMPDIR", old_tmp_dir_, true);
    } else {
      // TMPDIR wasn't set originally so unset it.
      unsetenv("TMPDIR");
    }

    TracingUnitTest::TearDown();
  }

  std::unique_ptr<MockPosixSystemProducer> CreateMockPosixSystemProducer(
      MockSystemService* service,
      int num_data_sources_expected = 0,
      base::RunLoop* system_data_source_enabled_runloop = nullptr,
      base::RunLoop* system_data_source_disabled_runloop = nullptr,
      bool check_sdk_level = false,
      bool sandbox_forbids_socket_connection = false) {
    std::unique_ptr<MockPosixSystemProducer> result;
    base::RunLoop loop_finished;
    // When we construct a MockPosixSystemProducer it needs to be on the
    // correct sequence. Construct it on the task runner and wait on the
    // |loop_finished| to ensure it is completely set up.
    PerfettoTracedProcess::GetTaskRunner()
        ->GetOrCreateTaskRunner()
        ->PostTaskAndReply(
            FROM_HERE, base::BindLambdaForTesting([&]() {
              result = std::make_unique<MockPosixSystemProducer>(
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
                      : base::OnceClosure(),
                  sandbox_forbids_socket_connection);
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

  PerfettoService* local_service() const { return perfetto_service_.get(); }

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
    size_t written =
        UNSAFE_TODO(config_file.Write(0, config.data(), config.size()));
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
  std::string stderr_;
  const char* old_tmp_dir_ = nullptr;
};

#if BUILDFLAG(IS_ANDROID)
// Flaky on Android: crbug.com/1262132#c17
#define MAYBE_SystemTraceEndToEnd DISABLED_SystemTraceEndToEnd
#else
#define MAYBE_SystemTraceEndToEnd SystemTraceEndToEnd
#endif
TEST_F(SystemPerfettoTest, MAYBE_SystemTraceEndToEnd) {
  auto system_service = CreateMockSystemService();

  // Set up the producer to talk to the system.
  base::RunLoop system_data_source_enabled_runloop;
  base::RunLoop system_data_source_disabled_runloop;
  auto system_producer = CreateMockPosixSystemProducer(
      system_service.get(),
      /* num_data_sources_expected = */ 1, &system_data_source_enabled_runloop,
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

#if BUILDFLAG(IS_ANDROID)
// Flaky on Android: crbug.com/1262132#c17
#define MAYBE_OneSystemSourceWithMultipleLocalSources \
  DISABLED_OneSystemSourceWithMultipleLocalSources
#else
#define MAYBE_OneSystemSourceWithMultipleLocalSources \
  OneSystemSourceWithMultipleLocalSources
#endif
TEST_F(SystemPerfettoTest, MAYBE_OneSystemSourceWithMultipleLocalSources) {
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
  auto system_producer = CreateMockPosixSystemProducer(
      system_service.get(),
      /* num_data_sources_expected = */ 1, &system_data_source_enabled_runloop,
      &system_data_source_disabled_runloop);

  system_data_source_enabled_runloop.Run();
  system_consumer.WaitForAllDataSourcesStarted();

  // Now start the local trace and wait for the system trace to stop first.
  base::RunLoop local_data_source_enabled_runloop;
  base::RunLoop local_data_source_disabled_runloop;
  base::RunLoop local_no_more_packets_runloop;
  std::unique_ptr<MockProducerClient::Handle> local_producer_client =
      MockProducerClient::Create(
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
      GetPerfettoProducerName(), kPerfettoTestDataSourceName, local_service(),
      **local_producer_client);

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

  PerfettoProducer::DeleteSoonForTesting(std::move(system_producer));
}

#if BUILDFLAG(IS_ANDROID)
// Flaky on Android: crbug.com/1262132#c17
#define MAYBE_MultipleSystemSourceWithOneLocalSourcesLocalFirst \
  DISABLED_MultipleSystemSourceWithOneLocalSourcesLocalFirst
#else
#define MAYBE_MultipleSystemSourceWithOneLocalSourcesLocalFirst \
  MultipleSystemSourceWithOneLocalSourcesLocalFirst
#endif
TEST_F(SystemPerfettoTest,
       MAYBE_MultipleSystemSourceWithOneLocalSourcesLocalFirst) {
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
  std::unique_ptr<MockProducerClient::Handle> local_producer_client =
      MockProducerClient::Create(
          /* num_data_sources = */ 1,
          local_data_source_enabled_runloop.QuitClosure(),
          local_data_source_disabled_runloop.QuitClosure());
  auto local_producer_host = std::make_unique<MockProducerHost>(
      GetPerfettoProducerName(), kPerfettoTestDataSourceName, local_service(),
      **local_producer_client);

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
  auto system_producer = CreateMockPosixSystemProducer(
      system_service.get(),
      /* num_data_sources_expected = */ 3, &system_data_source_enabled_runloop,
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
  (*local_producer_client)
      ->SetAgentEnabledCallback(
          local_data_source_reenabled_runloop.QuitClosure());
  (*local_producer_client)
      ->SetAgentDisabledCallback(
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

  PerfettoProducer::DeleteSoonForTesting(std::move(system_producer));
}

#if BUILDFLAG(IS_ANDROID)
// Flaky on Android: crbug.com/1262132#c17
#define MAYBE_MultipleSystemAndLocalSources \
  DISABLED_MultipleSystemAndLocalSources
#else
#define MAYBE_MultipleSystemAndLocalSources MultipleSystemAndLocalSources
#endif
TEST_F(SystemPerfettoTest, MAYBE_MultipleSystemAndLocalSources) {
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
  auto system_producer = CreateMockPosixSystemProducer(
      system_service.get(),
      /* num_data_sources_expected = */ 3, &system_data_source_enabled_runloop,
      &system_data_source_disabled_runloop);

  system_data_source_enabled_runloop.Run();
  system_consumer.WaitForAllDataSourcesStarted();

  // Now start the local trace and wait for the system trace to stop first.
  base::RunLoop local_data_source_enabled_runloop;
  base::RunLoop local_data_source_disabled_runloop;
  base::RunLoop local_no_more_packets_runloop;
  std::unique_ptr<MockProducerClient::Handle> local_producer_client =
      MockProducerClient::Create(
          /* num_data_sources = */ 3,
          local_data_source_enabled_runloop.QuitClosure(),
          local_data_source_disabled_runloop.QuitClosure());
  auto local_producer_host = std::make_unique<MockProducerHost>(
      GetPerfettoProducerName(), kPerfettoTestDataSourceName, local_service(),
      **local_producer_client);
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
  // |system_data_source_enabled| is called by the MockPosixSystemProducer after
  // calling PosixSystemProducer::StartDataSource, BUT
  // PosixSystemProducer::StartDataSource could not actual start the DataSource
  // if local tracing hasn't yet finished. So waiting for data to be written
  // (and thus implying the data source has started) is a better option.
  std::vector<base::RunLoop> data_sources_wrote_data{data_sources_.size()};
  for (size_t i = 0; i < data_sources_.size(); ++i) {
    data_sources_[i]->set_start_tracing_callback(
        data_sources_wrote_data[i].QuitClosure());
  }

  local_consumer.StopTracing();
  local_data_source_disabled_runloop.Run();
  local_consumer.WaitForAllDataSourcesStopped();
  local_no_more_packets_runloop.Run();

  // Wait for system tracing to return before stopping.
  system_data_source_reenabled_runloop.Run();
  for (auto& loop : data_sources_wrote_data) {
    loop.Run();
  }
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

  PerfettoProducer::DeleteSoonForTesting(std::move(system_producer));
}

#if BUILDFLAG(IS_ANDROID)
// Flaky on Android: crbug.com/1262132#c17
#define MAYBE_MultipleSystemAndLocalSourcesLocalFirst \
  DISABLED_MultipleSystemAndLocalSourcesLocalFirst
#else
#define MAYBE_MultipleSystemAndLocalSourcesLocalFirst \
  MultipleSystemAndLocalSourcesLocalFirst
#endif
TEST_F(SystemPerfettoTest, MAYBE_MultipleSystemAndLocalSourcesLocalFirst) {
  auto system_service = CreateMockSystemService();

  // We construct it up front so it connects to the service before the local
  // trace starts.
  base::RunLoop system_data_source_enabled_runloop;
  base::RunLoop system_data_source_disabled_runloop;
  auto system_producer = CreateMockPosixSystemProducer(
      system_service.get(),
      /* num_data_sources_expected = */ 3, &system_data_source_enabled_runloop,
      &system_data_source_disabled_runloop);

  // Now start the local trace and wait for the system trace to stop first.
  base::RunLoop local_data_source_enabled_runloop;
  base::RunLoop local_data_source_disabled_runloop;
  base::RunLoop local_no_more_packets_runloop;
  std::unique_ptr<MockProducerClient::Handle> local_producer_client =
      MockProducerClient::Create(
          /* num_data_sources = */ 3,
          local_data_source_enabled_runloop.QuitClosure(),
          local_data_source_disabled_runloop.QuitClosure());
  auto local_producer_host = std::make_unique<MockProducerHost>(
      GetPerfettoProducerName(), kPerfettoTestDataSourceName, local_service(),
      **local_producer_client);
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

  PerfettoProducer::DeleteSoonForTesting(std::move(system_producer));
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
// Flaky on all CrOS platforms: crbug.com/1262132#c18
// Flaky on Android: crbug.com/1262132
#define MAYBE_SystemTraceWhileLocalStartupTracing \
  DISABLED_SystemTraceWhileLocalStartupTracing
#else
#define MAYBE_SystemTraceWhileLocalStartupTracing \
  SystemTraceWhileLocalStartupTracing
#endif
// Attempts to start a system trace while a local startup trace is active. The
// system trace should only be started after the local trace is completed.
TEST_F(SystemPerfettoTest, MAYBE_SystemTraceWhileLocalStartupTracing) {
  // We're using mojom::kTraceEventDataSourceName for the local producer to
  // emulate starting the real TraceEventDataSource which owns startup tracing.
  auto mock_trace_event_ds = TestDataSource::CreateAndRegisterDataSource(
      mojom::kTraceEventDataSourceName, 2);

  // Wait for data source to register.
  RunUntilIdle();

  auto system_service = CreateMockSystemService();

  // Create local producer.
  base::RunLoop local_data_source_enabled_runloop;
  base::RunLoop local_data_source_disabled_runloop;
  auto local_producer = MockProducerClient::Create(
      /* num_data_sources = */ 1,
      local_data_source_enabled_runloop.QuitClosure(),
      local_data_source_disabled_runloop.QuitClosure());

  // Setup startup tracing for local producer.
  CHECK((*local_producer)
            ->SetupStartupTracing(base::trace_event::TraceConfig(),
                                  /*privacy_filtering_enabled=*/false));

  // Attempt to start a system tracing session. Because startup tracing is
  // already active, the system producer shouldn't activate yet.
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
  auto system_producer = CreateMockPosixSystemProducer(
      system_service.get(),
      /* num_data_sources_expected = */ 1, &system_data_source_enabled_runloop,
      &system_data_source_disabled_runloop);

  RunUntilIdle();

  // Now connect the local ProducerHost & consumer, taking over startup tracing.
  base::RunLoop local_no_more_packets_runloop;
  std::unique_ptr<MockConsumer> local_consumer(new MockConsumer(
      {mojom::kTraceEventDataSourceName}, local_service()->GetService(),
      [&local_no_more_packets_runloop](bool has_more) {
        if (!has_more) {
          local_no_more_packets_runloop.Quit();
        }
      }));
  auto local_producer_host = std::make_unique<MockProducerHost>(
      GetPerfettoProducerName(), mojom::kTraceEventDataSourceName,
      local_service(), **local_producer);
  local_data_source_enabled_runloop.Run();
  local_consumer->WaitForAllDataSourcesStarted();

  // Ensures that the Trace data gets written and committed.
  RunUntilIdle();

  // Stop local session. This should reconnect the system producer & start the
  // system session.
  local_consumer->StopTracing();
  local_data_source_disabled_runloop.Run();
  local_consumer->WaitForAllDataSourcesStopped();
  local_no_more_packets_runloop.Run();
  // Local consumer should have received 2 packets from |mock_trace_event_ds|.
  EXPECT_EQ(2u, local_consumer->received_test_packets());

  // Wait for system producer to get enabled. Wait have to wait for the
  // individual data source to write data, because it might be queued until the
  // local trace has fully finished.
  base::RunLoop system_data_source_wrote_data_runloop;
  data_sources_[0]->set_start_tracing_callback(
      system_data_source_wrote_data_runloop.QuitClosure());

  system_data_source_enabled_runloop.Run();
  system_data_source_wrote_data_runloop.Run();
  system_consumer.WaitForAllDataSourcesStarted();

  // Stop the trace on the correct sequence to ensure everything is committed.
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

  // Local consumer should have received 1 packet from the |data_sources_[0]|.
  EXPECT_EQ(1u, system_consumer.received_test_packets());

  PerfettoProducer::DeleteSoonForTesting(std::move(system_producer));
}

#if BUILDFLAG(IS_ANDROID)
// Failing on android-pie-arm64-dbg, see crbug.com/1262132.
TEST_F(SystemPerfettoTest, DISABLED_SystemToLowAPILevel) {
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
    auto system_producer = CreateMockPosixSystemProducer(
        system_service.get(),
        /* num_data_sources_expected = */ 1,
        &system_data_source_enabled_runloop,
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

// Flaky on Android: crbug.com/1262132#c17
TEST_F(SystemPerfettoTest, DISABLED_EnabledOnDebugBuilds) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kEnablePerfettoSystemTracing);
  if (base::android::BuildInfo::GetInstance()->is_debug_android()) {
    EXPECT_TRUE(ShouldSetupSystemTracing());
  } else {
    EXPECT_FALSE(ShouldSetupSystemTracing());
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
// Flaky on Android: crbug.com/1262132#c17
#define MAYBE_RespectsFeatureList DISABLED_RespectsFeatureList
#else
#define MAYBE_RespectsFeatureList RespectsFeatureList
#endif
TEST_F(SystemPerfettoTest, MAYBE_RespectsFeatureList) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_debug_android()) {
    // The feature list is ignored on debug android builds so we should have a
    // real system producer so just bail out of this test.
    EXPECT_TRUE(ShouldSetupSystemTracing());
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(features::kEnablePerfettoSystemTracing);
    EXPECT_TRUE(ShouldSetupSystemTracing());
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kEnablePerfettoSystemTracing);
    EXPECT_FALSE(ShouldSetupSystemTracing());
  }
}

#if BUILDFLAG(IS_ANDROID)
// Flaky on Android: crbug.com/1262132#c17
TEST_F(SystemPerfettoTest, DISABLED_RespectsFeaturePreAndroidPie) {
  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_P) {
    return;
  }

  auto run_test = [this](bool enable_feature) {
    PerfettoTracedProcess::Get()->ClearDataSourcesForTesting();

    base::test::ScopedFeatureList feature_list;
    if (enable_feature) {
      feature_list.InitAndEnableFeature(features::kEnablePerfettoSystemTracing);
    } else {
      feature_list.InitAndDisableFeature(
          features::kEnablePerfettoSystemTracing);
    }

    std::string data_source_name = "temp_name";

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
    auto system_producer = CreateMockPosixSystemProducer(
        system_service.get(),
        /* num_data_sources_expected = */ 1,
        &system_data_source_enabled_runloop,
        &system_data_source_disabled_runloop, /* check_sdk_level = */ true);
    PerfettoTracedProcess::GetTaskRunner()->PostTask(
        [&system_producer]() { system_producer->ConnectToSystemService(); });

    if (enable_feature) {
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

    if (enable_feature) {
      system_data_source_disabled_runloop.Run();
      system_consumer.WaitForAllDataSourcesStopped();
    }
    system_no_more_packets_runloop.Run();

    PerfettoProducer::DeleteSoonForTesting(std::move(system_producer));
    return system_consumer.received_test_packets();
  };

  EXPECT_EQ(0u, run_test(/* enable_feature = */ false));
  EXPECT_EQ(1u, run_test(/* enable_feature = */ true));
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(SystemPerfettoTest, DISABLED_EnablePerfettoSystemTracingDefaultState) {
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(features::kEnablePerfettoSystemTracing.default_state,
            base::FEATURE_ENABLED_BY_DEFAULT);
#else
  EXPECT_EQ(features::kEnablePerfettoSystemTracing.default_state,
            base::FEATURE_DISABLED_BY_DEFAULT);
#endif
}

#if defined(ANDROID)
// Flaky on Android: crbug.com/1262132#c17
#define MAYBE_SetupSystemTracing DISABLED_SetupSystemTracing
#else
#define MAYBE_SetupSystemTracing SetupSystemTracing
#endif
TEST_F(SystemPerfettoTest, MAYBE_SetupSystemTracing) {
  ClearAndRestoreSystemProducerScope saved_system_producer;
  EXPECT_FALSE(PerfettoTracedProcess::Get()->system_producer());
  PerfettoTracedProcess::Get()->SetupSystemTracing();
  EXPECT_TRUE(PerfettoTracedProcess::Get()->system_producer());
#if BUILDFLAG(IS_POSIX)
  EXPECT_FALSE(PerfettoTracedProcess::Get()
                   ->system_producer()
                   ->IsDummySystemProducerForTesting());
#else   // BUILDFLAG(IS_POSIX)
  EXPECT_TRUE(PerfettoTracedProcess::Get()
                  ->system_producer()
                  ->IsDummySystemProducerForTesting());
#endif  // BUILDFLAG(IS_POSIX)
}

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
TEST_F(SystemPerfettoTest, SandboxedOpenProducerSocket) {
  const char* kProducerSockEnvName = "PERFETTO_PRODUCER_SOCK_NAME";
  auto system_service = CreateMockSystemService();

  // Create the Mojo receiver.
  auto sts = std::make_unique<SystemTracingService>();

  // Override default socket name to make |sts| connect the |system_service|
  // correctly.
  const char* saved_producer_sock_name = getenv(kProducerSockEnvName);
  ASSERT_EQ(
      0, setenv(kProducerSockEnvName, system_service->producer().c_str(), 1));

  // Bind the remote and receiver.
  PerfettoTracedProcess::GetTaskRunner()->PostTask([&sts]() {
    auto remote = sts->BindAndPassPendingRemote();
    TracedProcessImpl::GetInstance()->EnableSystemTracingService(
        std::move(remote));
  });

  // Set up the producer to talk to the system.
  base::RunLoop system_data_source_enabled_runloop;
  base::RunLoop system_data_source_disabled_runloop;
  // Create a MockPosixSystemProducer that doesn't make direct socket connection
  // but through Mojo.
  auto system_producer = CreateMockPosixSystemProducer(
      system_service.get(),
      /* num_data_sources_expected = */ 1, &system_data_source_enabled_runloop,
      &system_data_source_disabled_runloop, false,
      /* sandbox_forbids_socket_connection= */ true);

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
      [&system_consumer, &stop_tracing, &sts]() {
        system_consumer.StopTracing();
        // Mojo receiver is bound on the Perfetto task runner.
        sts.reset();
        stop_tracing.Quit();
      });
  stop_tracing.Run();

  system_data_source_disabled_runloop.Run();
  system_no_more_packets_runloop.Run();
  system_consumer.WaitForAllDataSourcesStopped();

  if (saved_producer_sock_name) {
    ASSERT_EQ(0, setenv(kProducerSockEnvName, saved_producer_sock_name, true));
  } else {
    ASSERT_EQ(0, unsetenv(kProducerSockEnvName));
  }

  EXPECT_EQ(1u, system_consumer.received_test_packets());
  PerfettoProducer::DeleteSoonForTesting(std::move(system_producer));
}
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace tracing
