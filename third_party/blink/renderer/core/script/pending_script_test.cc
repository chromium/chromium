// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/location.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {
// The script execution time in nested-script.js must greater than 200
// milliseconds.
constexpr base::TimeDelta kScriptRunTime = base::Milliseconds(200);
}  // namespace

class PendingScriptTest : public PageTestBase {
 public:
  PendingScriptTest() = default;
  ~PendingScriptTest() override = default;

  void SetUp() override { PageTestBase::SetUp(); }

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
    PageTestBase::TearDown();
  }

 protected:
  void CheckParseBlockedOnScriptExecutionDuration(
      const String& file_name,
      bool is_parser_blocked,
      base::Location location = FROM_HERE) {
    WebURL mocked_mainframe_url = RegisterMockedURLLoadFromBase(file_name);

    frame_test_helpers::WebViewHelper web_view_helper;
    web_view_helper.InitializeAndLoad(mocked_mainframe_url.GetString().Utf8());

    auto performance_metrics =
        web_view_helper.LocalMainFrame()->PerformanceMetricsForReporting();
    auto parse_blocked_on_script_execution_duration = base::Seconds(
        performance_metrics.ParseBlockedOnScriptExecutionDuration());
    if (is_parser_blocked) {
      EXPECT_GE(parse_blocked_on_script_execution_duration, kScriptRunTime)
          << location.ToString();
    } else {
      EXPECT_EQ(parse_blocked_on_script_execution_duration, base::TimeDelta())
          << location.ToString();
    }
    auto parse_duration = base::Seconds(performance_metrics.ParseStop() -
                                        performance_metrics.ParseStart());
    // `PageLoadTiming` wouldn't be recorded if
    // `ParseBlockedOnScriptExecutionDuration()` is longer than total parse
    // time, so we expect `parse_duration` to be greater than
    // `parse_blocked_on_script_execution_duration`.
    EXPECT_GE(parse_duration, parse_blocked_on_script_execution_duration)
        << location.ToString();
  }

  WebURL RegisterMockedURLLoadFromBase(const String& filename,
                                       const String& mime_type = "text/html") {
    return url_test_helpers::RegisterMockedURLLoadFromBase(
        base_url_, test::CoreTestDataPath(relative_path_), filename, mime_type);
  }

 private:
  const String base_url_ = "http://internal.test/";
  const String relative_path_ = "script";
};

TEST_F(PendingScriptTest,
       OnlyCountOutermostScriptExecutionTime_ParserBlockedScript) {
  RegisterMockedURLLoadFromBase("nested-script.js", "text/javascript");
  CheckParseBlockedOnScriptExecutionDuration(
      "parser-blocked-inline-nested-script.html", true);
  CheckParseBlockedOnScriptExecutionDuration(
      "parser-blocked-external-nested-script.html", true);
  CheckParseBlockedOnScriptExecutionDuration(
      "parser-blocked-module-nested-script.html", true);
  CheckParseBlockedOnScriptExecutionDuration(
      "parser-blocked-defer-nested-script.html", true);
}

TEST_F(PendingScriptTest,
       OnlyCountOutermostScriptExecutionTime_NotParserBlockedScript) {
  RegisterMockedURLLoadFromBase("nested-script.js", "text/javascript");
  CheckParseBlockedOnScriptExecutionDuration(
      "not-parser-blocked-async-nested-script.html", false);
}

}  // namespace blink
