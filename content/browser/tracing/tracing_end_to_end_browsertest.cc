// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <thread>

#include "base/metrics/histogram_macros.h"
#include "base/test/test_trace_processor.h"
#include "base/test/trace_test_utils.h"
#include "build/build_config.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/chrome_config.gen.h"

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
namespace content {

namespace {

perfetto::protos::gen::TraceConfig TraceConfigWithHistograms(
    const std::string& category_filter_string,
    const std::vector<std::string>& histograms) {
  base::trace_event::TraceConfig trace_event_config(category_filter_string, "");
  for (const auto& histogram : histograms) {
    trace_event_config.EnableHistogram(histogram);
  }

  auto perfetto_config =
      base::test::DefaultTraceConfig(category_filter_string, false);
  for (auto& ds : *perfetto_config.mutable_data_sources()) {
    if (ds.config().name() == "track_event") {
      ds.mutable_config()->mutable_chrome_config()->set_trace_config(
          trace_event_config.ToString());
    }
  }
  return perfetto_config;
}

}  // namespace

class TracingEndToEndBrowserTest : public ContentBrowserTest {};

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest, SimpleTraceEvent) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace(base::test::DefaultTraceConfig("foo", false),
                 perfetto::kCustomBackend);

  {
    // A simple trace event
    TRACE_EVENT("foo", "test_event");
  }

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  std::string query = "SELECT name from slice";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"name"},
                                     std::vector<std::string>{"test_event"}));
}

#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest,
                       PackageNameRecordedTraceLogSet) {
  base::trace_event::TraceLog::GetInstance()->SetRecordHostAppPackageName(true);
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace(base::test::DefaultTraceConfig("foo", false),
                 perfetto::kCustomBackend);

  {
    // A simple trace event
    TRACE_EVENT("foo", "test_event");
  }

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  std::string query =
      "SELECT "
      "DISTINCT(EXTRACT_ARG(arg_set_id, \"chrome.host_app_package_name\")) "
      "AS name "
      "FROM process "
      "WHERE "
      "EXTRACT_ARG(arg_set_id, \"chrome.host_app_package_name\") IS NOT NULL";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_THAT(
      result.value(),
      ::testing::ElementsAre(
          std::vector<std::string>{"name"},
          std::vector<std::string>{"org.chromium.content_browsertests_apk"}));
}

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest,
                       PackageNameNotRecordedTraceLogNotSet) {
  base::trace_event::TraceLog::GetInstance()->SetRecordHostAppPackageName(
      false);
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace(base::test::DefaultTraceConfig("foo", false),
                 perfetto::kCustomBackend);

  {
    // A simple trace event
    TRACE_EVENT("foo", "test_event");
  }

  absl::Status status = ttp.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  std::string query =
      "SELECT "
      "DISTINCT(EXTRACT_ARG(arg_set_id, \"chrome.host_app_package_name\")) "
      "AS name "
      "FROM process "
      "WHERE "
      "EXTRACT_ARG(arg_set_id, \"chrome.host_app_package_name\") IS NOT NULL";
  auto result = ttp.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();

  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"name"}));
}
#endif  // BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest, TwoSessionsSimple) {
  base::test::TestTraceProcessor ttp1, ttp2;
  ttp1.StartTrace("foo,cat");
  ttp2.StartTrace("test,cat");

  {
    TRACE_EVENT("foo", "foo_event");
    TRACE_EVENT("test", "test_event");
    TRACE_EVENT("cat", "cat_event");
  }

  absl::Status status = ttp1.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  std::string query = "SELECT name from slice";
  auto result1 = ttp1.RunQuery(query);
  ASSERT_TRUE(result1.has_value()) << result1.error();
  EXPECT_THAT(result1.value(),
              ::testing::ElementsAre(std::vector<std::string>{"name"},
                                     std::vector<std::string>{"foo_event"},
                                     std::vector<std::string>{"cat_event"}));

  status = ttp2.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto result2 = ttp2.RunQuery(query);
  ASSERT_TRUE(result2.has_value()) << result2.error();
  EXPECT_THAT(result2.value(),
              ::testing::ElementsAre(std::vector<std::string>{"name"},
                                     std::vector<std::string>{"test_event"},
                                     std::vector<std::string>{"cat_event"}));
}

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest, TwoSessionsHistograms) {
  // Start the first session that records "Test.Foo" histograms
  auto config = TraceConfigWithHistograms(
      "disabled-by-default-histogram_samples", {"Test.Foo"});
  base::test::TestTraceProcessor ttp1;
  ttp1.StartTrace(config);

  UMA_HISTOGRAM_BOOLEAN("Test.Foo", true);
  UMA_HISTOGRAM_BOOLEAN("Test.Bar", true);

  // Start the second session that records "Test.Bar" histograms
  config = TraceConfigWithHistograms("disabled-by-default-histogram_samples",
                                     {"Test.Bar"});
  base::test::TestTraceProcessor ttp2;
  ttp2.StartTrace(config);

  UMA_HISTOGRAM_BOOLEAN("Test.Foo", true);
  UMA_HISTOGRAM_BOOLEAN("Test.Bar", true);

  // Stop the second session. Its trace should contain only one "Test.Bar"
  // sample that was recorded while the session was active.
  // It will also contain a "Test.Foo" sample because right now the list of
  // monitored histograms is shared between sessions.
  // TODO(khokhlov): Fix the test expectations when CustomEventRecorder
  // correctly tracks which samples go to which sessions.
  absl::Status status = ttp2.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  std::string query =
      "SELECT "
      "EXTRACT_ARG(arg_set_id, \"chrome_histogram_sample.name\") AS name "
      "FROM slice "
      "WHERE cat = \"disabled-by-default-histogram_samples\"";
  auto result = ttp2.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"name"},
                                     std::vector<std::string>{"Test.Foo"},
                                     std::vector<std::string>{"Test.Bar"}));

  UMA_HISTOGRAM_BOOLEAN("Test.Foo", true);
  UMA_HISTOGRAM_BOOLEAN("Test.Bar", true);

  // Stop the first session. Its trace should contain all three "Test.Foo"
  // samples that were recorded while the session was active.
  // Some "Test.Bar" samples will also be there (see the previous comment).
  status = ttp1.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  result = ttp1.RunQuery(query);
  ASSERT_TRUE(result.has_value()) << result.error();
  EXPECT_THAT(result.value(),
              ::testing::ElementsAre(std::vector<std::string>{"name"},
                                     std::vector<std::string>{"Test.Foo"},
                                     std::vector<std::string>{"Test.Foo"},
                                     std::vector<std::string>{"Test.Bar"},
                                     std::vector<std::string>{"Test.Foo"},
                                     std::vector<std::string>{"Test.Bar"}));
}

IN_PROC_BROWSER_TEST_F(TracingEndToEndBrowserTest, TwoSessionsProcessNames) {
  base::test::TestTraceProcessor ttp1, ttp2;
  ttp1.StartTrace("foo");
  ttp2.StartTrace("test");

  {
    TRACE_EVENT("foo", "foo_event");
    TRACE_EVENT("test", "test_event");
  }

  absl::Status status = ttp1.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  std::string query =
      "SELECT COUNT(*) AS cnt FROM process WHERE name = \"Browser\"";
  auto result1 = ttp1.RunQuery(query);
  ASSERT_TRUE(result1.has_value()) << result1.error();
  EXPECT_THAT(result1.value(),
              ::testing::ElementsAre(std::vector<std::string>{"cnt"},
                                     std::vector<std::string>{"1"}));

  status = ttp2.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto result2 = ttp2.RunQuery(query);
  ASSERT_TRUE(result2.has_value()) << result2.error();
  EXPECT_THAT(result2.value(),
              ::testing::ElementsAre(std::vector<std::string>{"cnt"},
                                     std::vector<std::string>{"1"}));
}

}  // namespace content
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
