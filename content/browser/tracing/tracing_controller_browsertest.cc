// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/tracing_controller.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/trace_uploader.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/tracing/public/cpp/trace_event_agent.h"
#include "services/tracing/public/cpp/tracing_features.h"

#if defined(OS_CHROMEOS)
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#endif

using base::trace_event::RECORD_CONTINUOUSLY;
using base::trace_event::RECORD_UNTIL_FULL;
using base::trace_event::TraceConfig;

namespace content {

namespace {

bool KeyEquals(const base::Value* value,
               const char* key_name,
               const char* expected) {
  const base::Value* content =
      value->FindKeyOfType(key_name, base::Value::Type::STRING);
  if (!content)
    return false;
  return content->GetString() == expected;
}

bool KeyNotEquals(const base::Value* value,
                  const char* key_name,
                  const char* expected) {
  const base::Value* content =
      value->FindKeyOfType(key_name, base::Value::Type::STRING);
  if (!content)
    return false;
  return content->GetString() != expected;
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
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(std::move(done_callback_),
                       std::make_unique<std::string>(std::move(trace_))));
  }

 protected:
  ~TracingControllerTestEndpoint() override = default;

  std::string trace_;
  TracingController::CompletionCallback done_callback_;
};

class TestTracingDelegate : public TracingDelegate {
 public:
  std::unique_ptr<TraceUploader> GetTraceUploader(
      scoped_refptr<network::SharedURLLoaderFactory>) override {
    return nullptr;
  }
};

class TracingControllerTest : public ContentBrowserTest {
 public:
  TracingControllerTest() {}

  void SetUp() override {
    get_categories_done_callback_count_ = 0;
    enable_recording_done_callback_count_ = 0;
    disable_recording_done_callback_count_ = 0;

#if defined(OS_CHROMEOS)
    // Set statistic provider for hardware class tests.
    chromeos::system::StatisticsProvider::SetTestProvider(
        &fake_statistics_provider_);
    fake_statistics_provider_.SetMachineStatistic(
        chromeos::system::kHardwareClassKey, "test-hardware-class");
#endif
    ContentBrowserTest::SetUp();
  }

  void Navigate(Shell* shell) {
    EXPECT_TRUE(NavigateToURL(shell, GetTestUrl("", "title1.html")));
  }

  std::unique_ptr<base::DictionaryValue> GenerateMetadataDict() {
    return std::move(metadata_);
  }

  void GetCategoriesDoneCallbackTest(base::Closure quit_callback,
                                     const std::set<std::string>& categories) {
    get_categories_done_callback_count_++;
    EXPECT_FALSE(categories.empty());
    std::move(quit_callback).Run();
  }

  void StartTracingDoneCallbackTest(base::Closure quit_callback) {
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

  void StopTracingFileDoneCallbackTest(base::Closure quit_callback,
                                       const base::FilePath& file_path) {
    disable_recording_done_callback_count_++;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(PathExists(file_path));
      int64_t file_size;
      base::GetFileSize(file_path, &file_size);
      EXPECT_GT(file_size, 0);
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
      TracingController::CompletionCallback callback = base::BindRepeating(
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
    TracingControllerImpl::GetInstance()->SetTracingDelegateForTesting(
        std::make_unique<TestTracingDelegate>());

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
      TracingController::CompletionCallback callback = base::BindRepeating(
          &TracingControllerTest::StopTracingStringDoneCallbackTest,
          base::Unretained(this), run_loop.QuitClosure());

      scoped_refptr<TracingController::TraceDataEndpoint> trace_data_endpoint =
          TracingController::CreateStringEndpoint(std::move(callback));

      metadata_ = std::make_unique<base::DictionaryValue>();
      metadata_->SetString("not-whitelisted", "this_not_found");
      tracing::TraceEventAgent::GetInstance()->AddMetadataGeneratorFunction(
          base::Bind(&TracingControllerTest::GenerateMetadataDict,
                     base::Unretained(this)));

      bool result =
          controller->StopTracing(trace_data_endpoint, /*agent_label=*/"",
                                  /*privacy_filtering_enabled=*/true);
      ASSERT_TRUE(result);
      run_loop.Run();
      EXPECT_EQ(disable_recording_done_callback_count(), 1);
    }

    TracingControllerImpl::GetInstance()->SetTracingDelegateForTesting(nullptr);
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
              result_file_path, std::move(callback)));
      ASSERT_TRUE(result);
      run_loop.Run();
      EXPECT_EQ(disable_recording_done_callback_count(), 1);
    }
  }

#if defined(OS_CHROMEOS)
 protected:
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
#endif

 private:
  int get_categories_done_callback_count_;
  int enable_recording_done_callback_count_;
  int disable_recording_done_callback_count_;
  base::FilePath last_actual_recording_file_path_;
  std::unique_ptr<base::DictionaryValue> metadata_;
  std::unique_ptr<std::string> last_data_;
};

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

IN_PROC_BROWSER_TEST_F(TracingControllerTest, EnableAndStopTracing) {
  TestStartAndStopTracingString();
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       DisableRecordingStoresMetadata) {
  TestStartAndStopTracingString();
  // Check that a number of important keys exist in the metadata dictionary. The
  // values are not checked to ensure the test is robust.
  base::Optional<base::Value> trace_json = base::JSONReader::Read(last_data());
  ASSERT_TRUE(trace_json);
  auto* metadata_json = static_cast<base::DictionaryValue*>(
      trace_json->FindKeyOfType("metadata", base::Value::Type::DICTIONARY));
  ASSERT_TRUE(metadata_json);

  std::string network_type;
  metadata_json->GetString("network-type", &network_type);
  EXPECT_FALSE(network_type.empty());
  std::string user_agent;
  metadata_json->GetString("user-agent", &user_agent);
  EXPECT_FALSE(user_agent.empty());
  std::string os_name;
  metadata_json->GetString("os-name", &os_name);
  EXPECT_FALSE(os_name.empty());
  std::string command_line;
  metadata_json->GetString("command_line", &command_line);
  EXPECT_FALSE(command_line.empty());
  std::string trace_config;
  metadata_json->GetString("trace-config", &trace_config);
  EXPECT_EQ(TraceConfig().ToString(), trace_config);
#if defined(OS_CHROMEOS)
  std::string hardware_class;
  metadata_json->GetString("hardware-class", &hardware_class);
  EXPECT_EQ(hardware_class, "test-hardware-class");
#endif
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest, NotWhitelistedMetadataStripped) {
  TestStartAndStopTracingStringWithFilter();
  // Check that a number of important keys exist in the metadata dictionary.
  base::Optional<base::Value> trace_json = base::JSONReader::Read(last_data());
  ASSERT_TRUE(trace_json);
  const base::Value* metadata_json =
      trace_json->FindKeyOfType("metadata", base::Value::Type::DICTIONARY);
  ASSERT_TRUE(metadata_json);

  EXPECT_TRUE(KeyNotEquals(metadata_json, "cpu-brand", "__stripped__"));
  EXPECT_TRUE(KeyNotEquals(metadata_json, "network-type", "__stripped__"));
  EXPECT_TRUE(KeyNotEquals(metadata_json, "os-name", "__stripped__"));
  EXPECT_TRUE(KeyNotEquals(metadata_json, "user-agent", "__stripped__"));
#if defined(OS_CHROMEOS)
  EXPECT_TRUE(KeyNotEquals(metadata_json, "hardware-class", "__stripped__"));
#endif

  // The following field is not whitelisted and is supposed to be stripped.
  EXPECT_TRUE(KeyEquals(metadata_json, "v8-version", "__stripped__"));
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       EnableAndStopTracingWithFilePath) {
  base::FilePath file_path;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::CreateTemporaryFile(&file_path);
  }
  TestStartAndStopTracingFile(file_path);
  EXPECT_EQ(file_path.value(), last_actual_recording_file_path().value());
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       EnableAndStopTracingWithCompression) {
  TestStartAndStopTracingCompressed();
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest,
                       EnableAndStopTracingWithEmptyFile) {
  Navigate(shell());

  base::RunLoop run_loop;
  TracingController* controller = TracingController::GetInstance();
  EXPECT_TRUE(controller->StartTracing(
      TraceConfig(),
      TracingController::StartTracingDoneCallback()));
  EXPECT_TRUE(controller->StopTracing(
      TracingControllerImpl::CreateCallbackEndpoint(base::BindRepeating(
          [](base::OnceClosure quit_closure,
             std::unique_ptr<std::string> trace_str) {
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()))));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest, DoubleStopTracing) {
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
#if defined(OS_CHROMEOS) || (defined(IS_CHROMECAST) && defined(OS_LINUX))
#define MAYBE_SystemTraceEvents SystemTraceEvents
#else
#define MAYBE_SystemTraceEvents DISABLED_SystemTraceEvents
#endif
IN_PROC_BROWSER_TEST_F(TracingControllerTest, MAYBE_SystemTraceEvents) {
  TestStartAndStopTracingString(true /* enable_systrace */);
  EXPECT_TRUE(last_data().find("systemTraceEvents") != std::string::npos);
}

IN_PROC_BROWSER_TEST_F(TracingControllerTest, ProcessesPresentInTrace) {
  TestStartAndStopTracingString();
  EXPECT_TRUE(last_data().find("CrBrowserMain") != std::string::npos);
  EXPECT_TRUE(last_data().find("CrRendererMain") != std::string::npos);
}

}  // namespace content
