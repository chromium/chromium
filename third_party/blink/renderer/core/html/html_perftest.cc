// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A benchmark to isolate the HTML parsing done in the Speedometer test,
// for more stable benchmarking and profiling.

#include <string_view>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "testing/perf/perf_result_reporter.h"
#include "testing/perf/perf_test.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/testing/no_network_url_loader.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

// This is a dump of all setInnerHTML() calls from the VanillaJS-TodoMVC
// Speedometer test.
TEST(HTMLParsePerfTest, Speedometer) {
  const char* filename = "speedometer_saved_output.json";
  const char* label = "Speedometer";

  // Running more than once is useful for profiling. (If this flag does not
  // exist, it will return the empty string.)
  const std::string html_parse_iterations_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "html-parse-iterations");
  int html_parse_iterations =
      html_parse_iterations_str.empty() ? 1 : stoi(html_parse_iterations_str);

  auto reporter = perf_test::PerfResultReporter("BlinkHTML", label);

  std::optional<Vector<char>> serialized =
      test::ReadFromFile(test::CoreTestDataPath(filename));
  CHECK(serialized);
  std::optional<base::Value> json =
      base::JSONReader::Read(base::as_string_view(*serialized));
  if (!json.has_value()) {
    char msg[256];
    snprintf(msg, sizeof(msg), "Skipping %s test because %s could not be read",
             label, filename);
    GTEST_SKIP_(msg);
  }

  auto page = std::make_unique<DummyPageHolder>(
      gfx::Size(800, 600), nullptr,
      MakeGarbageCollected<NoNetworkLocalFrameClient>());
  page->GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);
  page->GetPage().SetDefaultPageScaleLimits(1, 4);

  Document& document = page->GetDocument();

  {
    base::ElapsedTimer html_timer;
    for (int i = 0; i < html_parse_iterations; ++i) {
      for (const base::Value& html : json->GetList()) {
        WTF::String html_wtf(html.GetString());
        document.body()->setInnerHTML(html_wtf);
      }
    }
    base::TimeDelta html_time = html_timer.Elapsed();
    reporter.RegisterImportantMetric("ParseTime", "us");
    reporter.AddResult("ParseTime", html_time);
  }
}

}  // namespace blink
