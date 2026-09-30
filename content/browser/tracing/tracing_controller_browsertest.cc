// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/tracing_controller.h"

#include <stdint.h>

#include <optional>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/task/task_traits.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_config.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_content_browser_client.h"
#include "content/shell/browser/shell.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pbzero.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "content/browser/tracing/cros_tracing_agent.h"
#endif

#if BUILDFLAG(IS_CASTOS)
#include "content/browser/tracing/cast_tracing_agent.h"
#endif

using base::trace_event::TraceConfig;

namespace content {

namespace {

// A legacy JSON trace decodes to no packets, so a non-zero count means the
// trace is a protobuf.
size_t CountTracePackets(const std::string& serialized_trace) {
  perfetto::protos::pbzero::Trace::Decoder trace(serialized_trace);
  size_t packets = 0;
  for (auto it = trace.packet(); !!it; ++it) {
    ++packets;
  }
  return packets;
}

// The default config, asking for the protobuf trace instead of JSON.
perfetto::TraceConfig ProtobufTraceConfig() {
  return tracing::GetDefaultPerfettoConfig(TraceConfig(),
                                           /*privacy_filtering_enabled=*/false,
                                           /*convert_to_legacy_json=*/false);
}

// Stops tracing and waits for the session to finish, discarding the trace.
void StopTracingAndWait() {
  base::test::TestFuture<std::unique_ptr<std::string>> trace;
  ASSERT_TRUE(TracingController::GetInstance()->StopTracing(
      TracingControllerImpl::CreateCallbackEndpoint(trace.GetCallback())));
  ASSERT_TRUE(trace.Wait());
}

}  // namespace

class TracingControllerTestEndpoint
    : public TracingController::TraceDataEndpoint {
 public:
  TracingControllerTestEndpoint(
      TracingController::CompletionCallback done_callback)
      : done_callback_(std::move(done_callback)) {}

  void ReceiveTraceChunk(std::unique_ptr<std::string> chunk) override {
    EXPECT_FALSE(chunk->empty());
    trace_ += *chunk;
  }

  void ReceivedTraceFinalContents() override {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(done_callback_),
                       std::make_unique<std::string>(std::move(trace_))));
  }

 protected:
  ~TracingControllerTestEndpoint() override = default;

  std::string trace_;
  TracingController::CompletionCallback done_callback_;
};

class TracingControllerTest : public ContentBrowserTest {
 public:
  TracingControllerTest() = default;

  void SetUp() override {
    get_categories_done_callback_count_ = 0;
    enable_recording_done_callback_count_ = 0;
    disable_recording_done_callback_count_ = 0;

#if BUILDFLAG(IS_CHROMEOS)
    ash::DebugDaemonClient::InitializeFake();
    // Set statistic provider for hardware class tests.
    ash::system::StatisticsProvider::SetTestProvider(
        &fake_statistics_provider_);
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kHardwareClassKey, "test-hardware-class");
#endif
    ContentBrowserTest::SetUp();
  }

  void TearDown() override {
    ContentBrowserTest::TearDown();
#if BUILDFLAG(IS_CHROMEOS)
    ash::DebugDaemonClient::Shutdown();
#endif
  }

  void Navigate(Shell* shell) {
    EXPECT_TRUE(NavigateToURL(shell, GetTestUrl("", "title1.html")));
  }

  void GetCategoriesDoneCallbackTest(base::OnceClosure quit_callback,
                                     const std::set<std::string>& categories) {
    get_categories_done_callback_count_++;
    EXPECT_FALSE(categories.empty());
    std::move(quit_callback).Run();
  }

  void StartTracingDoneCallbackTest(base::OnceClosure quit_callback) {
    enable_recording_done_callback_count_++;
    std::move(quit_callback).Run();
  }

  void StopTracingStringDoneCallbackTest(base::OnceClosure quit_callback,
                                         std::unique_ptr<std::string> data) {
    disable_recording_done_callback_count_++;
    last_data_ = std::move(data);
    EXPECT_FALSE(last_data_->empty());
    std::move(quit_callback).Run();
  }

  void StopTracingFileDoneCallbackTest(base::OnceClosure quit_callback,
                                       const base::FilePath& file_path) {
    disable_recording_done_callback_count_++;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(PathExists(file_path));
      std::optional<int64_t> file_size = base::GetFileSize(file_path);
      ASSERT_TRUE(file_size.has_value());
      EXPECT_GT(file_size.value(), 0);
    }
    std::move(quit_callback).Run();
    last_actual_recording_file_path_ = file_path;
  }

  int get_categories_done_callback_count() const {
    return get_categories_done_callback_count_;
  }

  int enable_recording_done_callback_count() const {
    return enable_recording_done_callback_count_;
  }

  int disable_recording_done_callback_count() const {
    return disable_recording_done_callback_count_;
  }

  base::FilePath last_actual_recording_file_path() const {
    return last_actual_recording_file_path_;
  }

  const std::string& last_data() const { return *last_data_; }

  void TestStartAndStopTracingString(bool enable_systrace = false) {
    Navigate(shell());

    TracingController* controller = TracingController::GetInstance();

    {
      base::RunLoop run_loop;
      TracingController::StartTracingDoneCallback callback =
          base::BindOnce(&TracingControllerTest::StartTracingDoneCallbackTest,
                         base::Unretained(this), run_loop.QuitClosure());
      TraceConfig config;
      if (enable_systrace)
        config.EnableSystrace();
      bool result = controller->StartTracing(config, std::move(callback));
      ASSERT_TRUE(result);
      run_loop.Run();
      EXPECT_EQ(enable_recording_done_callback_count(), 1);
    }

    {
      base::RunLoop run_loop;
      TracingController::CompletionCallback callback = base::BindOnce(
          &TracingControllerTest::StopTracingStringDoneCallbackTest,
          base::Unretained(this), run_loop.QuitClosure());
      bool result = controller->StopTracing(
          TracingController::CreateStringEndpoint(std::move(callback)));
      ASSERT_TRUE(result);
      run_loop.Run();
      EXPECT_EQ(disable_recording_done_callback_count(), 1);
    }
  }

  void TestStartAndStopTracingStringWithFilter() {

    Navigate(shell());

    TracingControllerImpl* controller = TracingControllerImpl::GetInstance();

    {
      base::RunLoop run_loop;
      TracingController::StartTracingDoneCallback callback =
          base::BindOnce(&TracingControllerTest::StartTracingDoneCallbackTest,
                         base::Unretained(this), run_loop.QuitClosure());

      bool result =
          controller->StartTracing(TraceConfig(), std::move(callback),
                                   /*privacy_filtering_enabled=*/true);
      ASSERT_TRUE(result);
      run_loop.Run();
      EXPECT_EQ(enable_recording_done_callback_count(), 1);
    }

    {
      base::RunLoop run_loop;
      TracingController::CompletionCallback callback = base::BindOnce(
          &TracingControllerTest::StopTracingStringDoneCallbackTest,
          base::Unretained(this), run_loop.QuitClosure());

      scoped_refptr<TracingController::TraceDataEndpoint> trace_data_endpoint =
          TracingController::CreateStringEndpoint(std::move(callback));

      bool result =
          controller->StopTracing(trace_data_endpoint, /*agent_label=*/"");
      ASSERT_TRUE(result);
      run_loop.Run();
      EXPECT_EQ(disable_recording_done_callback_count(), 1);
    }
  }

  void TestStartAndStopTracingCompressed() {
    Navigate(shell());

    TracingController* controller = TracingController::GetInstance();

    {
      base::RunLoop run_loop;
      TracingController::StartTracingDoneCallback callback =
          base::BindOnce(&TracingControllerTest::StartTracingDoneCallbackTest,
                         base::Unretained(this), run_loop.QuitClosure());
      bool result =
          controller->StartTracing(TraceConfig(), std::move(callback));
      ASSERT_TRUE(result);
      run_loop.Run();
      EXPECT_EQ(enable_recording_done_callback_count(), 1);
    }

    {
      base::RunLoop run_loop;
      TracingController::CompletionCallback callback = base::BindOnce(
          &TracingControllerTest::StopTracingStringDoneCallbackTest,
          base::Unretained(this), run_loop.QuitClosure());
      bool result = controller->StopTracing(
          TracingControllerImpl::CreateCompressedStringEndpoint(
              new TracingControllerTestEndpoint(std::move(callback)),
              true /* compress_with_background_priority */));
      ASSERT_TRUE(result);
      run_loop.Run();
      EXPECT_EQ(disable_recording_done_callback_count(), 1);
    }
  }

  void TestStartAndStopTracingFile(
      const base::FilePath& result_file_path) {
    Navigate(shell());

    TracingController* controller = TracingController::GetInstance();

    {
      base::RunLoop run_loop;
      TracingController::StartTracingDoneCallback callback =
          base::BindOnce(&TracingControllerTest::StartTracingDoneCallbackTest,
                         base::Unretained(this), run_loop.QuitClosure());
      bool result =
          controller->StartTracing(TraceConfig(), std::move(callback));
      ASSERT_TRUE(result);
      run_loop.Run();
      EXPECT_EQ(enable_recording_done_callback_count(), 1);
    }

    {
      base::RunLoop run_loop;
      base::RepeatingClosure callback = base::BindRepeating(
          &TracingControllerTest::StopTracingFileDoneCallbackTest,
          base::Unretained(this), run_loop.QuitClosure(), result_file_path);
      bool result =
          controller->StopTracing(TracingController::CreateFileEndpoint(
              result_file_path, std::move(callback),
              base::TaskPriority::USER_BLOCKING));
      ASSERT_TRUE(result);
      run_loop.Run();
      EXPECT_EQ(disable_recording_done_callback_count(), 1);
    }
  }

#if BUILDFLAG(IS_CHROMEOS)
 protected:
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
#endif

 private:
  int get_categories_done_callback_count_;
  int enable_recording_done_callback_count_;
  int disable_recording_done_callback_count_;
  base::FilePath last_actual_recording_file_path_;
  std::unique_ptr<std::string> last_data_;
};

// Consistent failures on Android Asan https://crbug.com/1045519
#if BUILDFLAG(IS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_EnableAndStopTracing DISABLED_EnableAndStopTracing
#define MAYBE_EnableAndStopTracingWithFilePath \
  DISABLED_EnableAndStopTracingWithFilePath
#define MAYBE_EnableAndStopTracingWithCompression \
  DISABLED_EnableAndStopTracingWithCompression
#define MAYBE_EnableAndStopTracingWithEmptyFile \
  DISABLED_EnableAndStopTracingWithEmptyFile
#define MAYBE_DoubleStopTracing DISABLED_DoubleStopTracing
#define MAYBE_ProcessesPresentInTrace DISABLED_ProcessesPresentInTrace
#define MAYBE_EnableAndStopTracingWithProtobufOutput \
  DISABLED_EnableAndStopTracingWithProtobufOutput
#define MAYBE_ProtobufOutputToFileIsNotTranslated \
  DISABLED_ProtobufOutputToFileIsNotTranslated
#define MAYBE_StartTracingWithAddedDataSource \
  DISABLED_StartTracingWithAddedDataSource
#define MAYBE_StartTracingWithMixedOutputFormatsFails \
  DISABLED_StartTracingWithMixedOutputFormatsFails
#define MAYBE_StartTracingWithPerfettoConfigFailsWhileTracing \
  DISABLED_StartTracingWithPerfettoConfigFailsWhileTracing
#define MAYBE_StopTracingWithAgentLabelFailsForProtobuf \
  DISABLED_StopTracingWithAgentLabelFailsForProtobuf
#else
#define MAYBE_EnableAndStopTracing EnableAndStopTracing
#define MAYBE_EnableAndStopTracingWithFilePath EnableAndStopTracingWithFilePath
#define MAYBE_EnableAndStopTracingWithCompression \
  EnableAndStopTracingWithCompression
#define MAYBE_EnableAndStopTracingWithEmptyFile \
  EnableAndStopTracingWithEmptyFile
#define MAYBE_DoubleStopTracing DoubleStopTracing
#define MAYBE_ProcessesPresentInTrace ProcessesPresentInTrace
#define MAYBE_EnableAndStopTracingWithProtobufOutput \
  EnableAndStopTracingWithProtobufOutput
#define MAYBE_ProtobufOutputToFileIsNotTranslated \
  ProtobufOutputToFileIsNotTranslated
#define MAYBE_StartTracingWithAddedDataSource StartTracingWithAddedDataSource
#define MAYBE_StartTracingWithMixedOutputFormatsFails \
  StartTracingWithMixedOutputFormatsFails
#define MAYBE_StartTracingWithPerfettoConfigFailsWhileTracing \
  StartTracingWithPerfettoConfigFailsWhileTracing
#define MAYBE_StopTracingWithAgentLabelFailsForProtobuf \
  StopTracingWithAgentLabelFailsForProtobuf
#endif

IN_PROC_BROWSER_TEST_F(TracingControllerTest, GetCategories) {
  TestStartAndStopTracingString();

  TracingController* controller = TracingController::GetInstance();

  base::RunLoop run_loop;
  TracingController::GetCategoriesDoneCallback callback =
      base::BindOnce(&TracingControllerTest::GetCategoriesDoneCallbackTest,
                     base::Unretained(this), run_loop.QuitClosure());
  ASSERT_TRUE(controller->GetCategories(std::move(callback)));
  run_loop.Run();
  EXPECT_EQ(get_categories_done_callback_count(), 1);
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest, MAYBE_EnableAndStopTracing) {
  TestStartAndStopTracingString();
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       MAYBE_EnableAndStopTracingWithFilePath) {
  base::FilePath file_path;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::CreateTemporaryFile(&file_path);
  }
  TestStartAndStopTracingFile(file_path);
  EXPECT_EQ(file_path.value(), last_actual_recording_file_path().value());
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       MAYBE_EnableAndStopTracingWithCompression) {
  TestStartAndStopTracingCompressed();
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       MAYBE_EnableAndStopTracingWithEmptyFile) {
  Navigate(shell());

  base::RunLoop run_loop;
  TracingController* controller = TracingController::GetInstance();
  EXPECT_TRUE(controller->StartTracing(
      TraceConfig(),
      TracingController::StartTracingDoneCallback()));
  EXPECT_TRUE(controller->StopTracing(
      TracingControllerImpl::CreateCallbackEndpoint(base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<std::string> trace_str) {
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()))));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest, MAYBE_DoubleStopTracing) {
  Navigate(shell());

  base::RunLoop run_loop;
  TracingController* controller = TracingController::GetInstance();
  EXPECT_TRUE(controller->StartTracing(
      TraceConfig(), TracingController::StartTracingDoneCallback()));
  EXPECT_TRUE(controller->StopTracing(
      TracingControllerImpl::CreateCallbackEndpoint(base::BindOnce(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<std::string> trace_str) {
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()))));
  EXPECT_FALSE(controller->StopTracing(nullptr));
  run_loop.Run();
}

// Only CrOS and Cast support system tracing.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_CASTOS)
#define MAYBE_SystemTraceEvents SystemTraceEvents
#else
#define MAYBE_SystemTraceEvents DISABLED_SystemTraceEvents
#endif
class SystemTraceTracingControllerTest : public TracingControllerTest {
 public:
  SystemTraceTracingControllerTest() {
#if BUILDFLAG(IS_CHROMEOS)
    feature_list_.InitAndEnableFeature(kCrOSTracingDataSource);
#elif BUILDFLAG(IS_CASTOS)
    feature_list_.InitAndEnableFeature(kCastTracingDataSource);
#endif
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SystemTraceTracingControllerTest,
                       MAYBE_SystemTraceEvents) {
  TestStartAndStopTracingString(true /* enable_systrace */);
  EXPECT_TRUE(last_data().find("systemTraceEvents") != std::string::npos);
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest, MAYBE_ProcessesPresentInTrace) {
  TestStartAndStopTracingString();
  EXPECT_TRUE(last_data().find("CrBrowserMain") != std::string::npos);
  EXPECT_TRUE(last_data().find("CrRendererMain") != std::string::npos);
}

// A config that does not set convert_to_legacy_json yields a protobuf trace.
IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       MAYBE_EnableAndStopTracingWithProtobufOutput) {
  Navigate(shell());

  TracingControllerImpl* controller = TracingControllerImpl::GetInstance();

  base::test::TestFuture<void> started;
  ASSERT_TRUE(controller->StartTracingWithPerfettoConfig(
      ProtobufTraceConfig(), started.GetCallback()));
  ASSERT_TRUE(started.Wait());

  base::test::TestFuture<std::unique_ptr<std::string>> trace;
  ASSERT_TRUE(controller->StopTracing(
      TracingController::CreateStringEndpoint(trace.GetCallback())));
  std::unique_ptr<std::string> contents = trace.Take();

  ASSERT_FALSE(contents->empty());
  // A legacy JSON trace starts with '{'; a protobuf trace does not.
  EXPECT_NE('{', (*contents)[0]);
  EXPECT_GT(CountTracePackets(*contents), 0u);
}

// A protobuf trace must reach disk byte for byte; Windows text mode would
// expand its '\n' bytes and it would no longer decode.
IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       MAYBE_ProtobufOutputToFileIsNotTranslated) {
  Navigate(shell());

  base::FilePath file_path;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::CreateTemporaryFile(&file_path));
  }

  TracingControllerImpl* controller = TracingControllerImpl::GetInstance();

  base::test::TestFuture<void> started;
  ASSERT_TRUE(controller->StartTracingWithPerfettoConfig(
      ProtobufTraceConfig(), started.GetCallback()));
  ASSERT_TRUE(started.Wait());

  base::test::TestFuture<void> written;
  ASSERT_TRUE(controller->StopTracing(TracingController::CreateFileEndpoint(
      file_path, written.GetCallback(), base::TaskPriority::USER_BLOCKING)));
  ASSERT_TRUE(written.Wait());

  std::string trace;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(file_path, &trace));
  }

  ASSERT_FALSE(trace.empty());
  EXPECT_NE('{', trace[0]);
  EXPECT_GT(CountTracePackets(trace), 0u);
}

// A caller may add data sources GetDefaultPerfettoConfig() does not produce.
IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       MAYBE_StartTracingWithAddedDataSource) {
  Navigate(shell());

  perfetto::TraceConfig config = ProtobufTraceConfig();
  ASSERT_FALSE(config.data_sources().empty());
  config.add_data_sources()->mutable_config()->set_name(
      "org.chromium.tracing_controller_browsertest");

  TracingControllerImpl* controller = TracingControllerImpl::GetInstance();

  base::test::TestFuture<void> started;
  ASSERT_TRUE(controller->StartTracingWithPerfettoConfig(
      config, started.GetCallback()));
  ASSERT_TRUE(started.Wait());

  StopTracingAndWait();
}

// convert_to_legacy_json applies to the whole session, so a config whose data
// sources disagree is rejected.
IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       MAYBE_StartTracingWithMixedOutputFormatsFails) {
  Navigate(shell());

  TracingControllerImpl* controller = TracingControllerImpl::GetInstance();

  perfetto::TraceConfig config = ProtobufTraceConfig();
  ASSERT_GT(config.data_sources().size(), 1u);
  (*config.mutable_data_sources())[0]
      .mutable_config()
      ->mutable_chrome_config()
      ->set_convert_to_legacy_json(true);

  EXPECT_FALSE(controller->StartTracingWithPerfettoConfig(
      config, TracingController::StartTracingDoneCallback()));
  EXPECT_FALSE(controller->IsTracing());
}

// The config belongs to the session and cannot be swapped while tracing.
IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       MAYBE_StartTracingWithPerfettoConfigFailsWhileTracing) {
  Navigate(shell());

  TracingControllerImpl* controller = TracingControllerImpl::GetInstance();

  base::test::TestFuture<void> started;
  ASSERT_TRUE(controller->StartTracing(TraceConfig(), started.GetCallback()));
  ASSERT_TRUE(started.Wait());

  EXPECT_FALSE(controller->StartTracingWithPerfettoConfig(
      ProtobufTraceConfig(), TracingController::StartTracingDoneCallback()));

  StopTracingAndWait();
}

// An agent label filters the converted JSON, so it needs a JSON trace.
IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       MAYBE_StopTracingWithAgentLabelFailsForProtobuf) {
  Navigate(shell());

  TracingControllerImpl* controller = TracingControllerImpl::GetInstance();

  base::test::TestFuture<void> started;
  ASSERT_TRUE(controller->StartTracingWithPerfettoConfig(
      ProtobufTraceConfig(), started.GetCallback()));
  ASSERT_TRUE(started.Wait());

  base::test::TestFuture<std::unique_ptr<std::string>> rejected;
  EXPECT_FALSE(controller->StopTracing(
      TracingController::CreateStringEndpoint(rejected.GetCallback()),
      "traceEvents"));

  StopTracingAndWait();
  EXPECT_FALSE(rejected.IsReady());
}

}  // namespace content
