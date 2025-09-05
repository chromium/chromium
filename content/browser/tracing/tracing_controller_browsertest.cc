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
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/tracing/public/cpp/tracing_features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#endif

using base::trace_event::TraceConfig;

namespace content {

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

      scoped_refptr<TracingController::TraceDataEndpoint> trace_data_endpoint =
          TracingController::CreateStringEndpoint(std::move(callback));

      bool result =
          controller->StopTracing(trace_data_endpoint, /*agent_label=*/"",
                                  /*privacy_filtering_enabled=*/true);
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
#else
#define MAYBE_EnableAndStopTracing EnableAndStopTracing
#define MAYBE_EnableAndStopTracingWithFilePath EnableAndStopTracingWithFilePath
#define MAYBE_EnableAndStopTracingWithCompression \
  EnableAndStopTracingWithCompression
#define MAYBE_EnableAndStopTracingWithEmptyFile \
  EnableAndStopTracingWithEmptyFile
#define MAYBE_DoubleStopTracing DoubleStopTracing
#define MAYBE_ProcessesPresentInTrace ProcessesPresentInTrace
#endif

IN_PROC_BROWSER_TEST_F(TracingControllerTest, GetCategories) {
  Navigate(shell());

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
IN_PROC_BROWSER_TEST_F(TracingControllerTest, MAYBE_SystemTraceEvents) {
  TestStartAndStopTracingString(true /* enable_systrace */);
  EXPECT_TRUE(last_data().find("systemTraceEvents") != std::string::npos);
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest, MAYBE_ProcessesPresentInTrace) {
  TestStartAndStopTracingString();
  EXPECT_TRUE(last_data().find("CrBrowserMain") != std::string::npos);
  EXPECT_TRUE(last_data().find("CrRendererMain") != std::string::npos);
}

}  // namespace content
