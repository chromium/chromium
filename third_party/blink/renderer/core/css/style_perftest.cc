// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A benchmark to verify style performance (and also hooks into layout,
// but not generally layout itself). This isolates style from paint etc.,
// for more stable benchmarking and profiling. Note that this test
// depends on external JSON files with stored web pages, which are
// not yet checked in. The tests will be skipped if you don't have the
// files available.

#include <algorithm>
#include <functional>
#include <string_view>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "testing/perf/perf_result_reporter.h"
#include "testing/perf/perf_test.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/blink/renderer/core/css/container_query_data.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_recalc_change.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/no_network_url_loader.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/process_heap.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

// The HTML left by the dumper script will contain any <style> tags that were
// in the DOM, which will be interpreted by SetInnerHTMLWithoutTrustedTypes()
// and converted to style sheets. However, we already have our own canonical
// list of sheets (from the JSON) that we want to use. Keeping both will make
// for duplicated rules, enabling rules and sheets that have since been deleted
// (occasionally even things like “display: none !important”) and so on.
// Thus, as a kludge, we strip all <style> tags from the HTML here before
// parsing.
static String StripStyleTags(const String& html) {
  StringBuilder stripped_html;
  wtf_size_t pos = 0;
  for (;;) {
    // Allow <style id=" etc.
    wtf_size_t style_start = html.DeprecatedFindIgnoringCase("<style", pos);
    if (style_start == kNotFound) {
      // No more <style> tags, so append the rest of the string.
      stripped_html.Append(html.subview(pos, html.length() - pos));
      break;
    }
    // Bail out if it's not “<style>” or “<style ”; it's probably
    // a false positive then.
    if (style_start + 6 >= html.length() ||
        (html[style_start + 6] != ' ' && html[style_start + 6] != '>')) {
      stripped_html.Append(html.subview(pos, style_start - pos));
      pos = style_start + 6;
      continue;
    }
    wtf_size_t style_end =
        html.DeprecatedFindIgnoringCase("</style>", style_start);
    if (style_end == kNotFound) {
      LOG(FATAL) << "Mismatched <style> tag";
    }
    stripped_html.Append(html.subview(pos, style_start - pos));
    pos = style_end + 8;
  }
  return stripped_html.ToString();
}

static std::unique_ptr<DummyPageHolder> LoadDumpedPage(
    const base::DictValue& dict,
    base::TimeDelta& parse_time,
    perf_test::PerfResultReporter* reporter) {
  const std::string parse_iterations_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "style-parse-iterations");
  int parse_iterations = 1;
  if (!parse_iterations_str.empty()) {
    CHECK(base::StringToInt(parse_iterations_str, &parse_iterations))
        << "Invalid value for --style-parse-iterations";
  }

  const CSSDeferPropertyParsing defer_property_parsing =
      base::CommandLine::ForCurrentProcess()->HasSwitch("style-lazy-parsing")
          ? CSSDeferPropertyParsing::kYes
          : CSSDeferPropertyParsing::kNo;

  auto page = std::make_unique<DummyPageHolder>(
      gfx::Size(800, 600), nullptr,
      MakeGarbageCollected<NoNetworkLocalFrameClient>());
  page->GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);
  page->GetPage().SetDefaultPageScaleLimits(1, 4);

  Document& document = page->GetDocument();
  StyleEngine& engine = document.GetStyleEngine();
  document.documentElement()->SetInnerHTMLWithoutTrustedTypes(
      StripStyleTags(String(*dict.FindString("html"))), ASSERT_NO_EXCEPTION);

  int num_sheets = 0;
  int num_bytes = 0;

  base::ElapsedTimer parse_timer;
  for (const base::Value& sheet_json : *dict.FindList("stylesheets")) {
    const base::DictValue& sheet_dict = sheet_json.GetDict();
    auto* sheet = MakeGarbageCollected<StyleSheetContents>(
        MakeGarbageCollected<CSSParserContext>(document));

    for (int i = 0; i < parse_iterations; ++i) {
      sheet->ParseString(String(*sheet_dict.FindString("text")),
                         /*allow_import_rules=*/true, defer_property_parsing);
    }
    if (*sheet_dict.FindString("type") == "user") {
      engine.InjectSheet(g_empty_atom, sheet, WebCssOrigin::kUser);
    } else {
      engine.InjectSheet(g_empty_atom, sheet, WebCssOrigin::kAuthor);
    }
    ++num_sheets;
    num_bytes += sheet_dict.FindString("text")->size();
  }
  parse_time = parse_timer.Elapsed();

  if (reporter) {
    reporter->RegisterFyiMetric("NumSheets", "");
    reporter->AddResult("NumSheets", static_cast<double>(num_sheets));

    reporter->RegisterFyiMetric("SheetSize", "kB");
    reporter->AddResult("SheetSize", static_cast<double>(num_bytes / 1024));

    reporter->RegisterImportantMetric("ParseTime", "us");
    reporter->AddResult("ParseTime", parse_time);
  }

  return page;
}

struct StylePerfResult {
  bool skipped = false;
  base::TimeDelta parse_time;
  base::TimeDelta initial_style_time;
  base::TimeDelta recalc_style_time;
  int64_t gc_allocated_bytes;
  int64_t partition_allocated_bytes;  // May be negative due to bugs.

  // Part of gc_allocated_bytes, but much more precise. Only enabled if
  // --measure-computed-style-memory is set -- and if so, gc_allocated_bytes
  // is going to be much higher due to the extra allocated objects used for
  // diffing.
  int64_t computed_style_used_bytes;
};

static StylePerfResult MeasureStyleForDumpedPage(
    std::string_view filename,
    bool parse_only,
    perf_test::PerfResultReporter* reporter) {
  StylePerfResult result;

  // Running more than once is useful for profiling. (If this flag does not
  // exist, it will return the empty string.)
  const std::string recalc_iterations_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "style-recalc-iterations");
  int recalc_iterations = 1;
  if (!recalc_iterations_str.empty()) {
    CHECK(base::StringToInt(recalc_iterations_str, &recalc_iterations))
        << "Invalid value for --style-recalc-iterations";
  }

  const bool measure_computed_style_memory =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          "measure-computed-style-memory");

  // Do a forced GC run before we start loading anything, so that we have
  // a more stable baseline. Note that even with this, the GC deltas tend to
  // be different depending on what other tests that run before, so if you want
  // the more consistent memory numbers, you'll need to run only a single test
  // only (e.g. --gtest_filter=StyleCalcPerfTest.Video).
  ThreadState::Current()->CollectAllGarbageForTesting();

  size_t orig_gc_allocated_bytes =
      blink::ProcessHeap::TotalAllocatedObjectSize();
  size_t orig_partition_allocated_bytes =
      Partitions::TotalSizeOfCommittedPages();

  std::unique_ptr<DummyPageHolder> page;

  {
    std::optional<Vector<char>> serialized =
        test::ReadFromFile(test::StylePerfTestDataPath(String(filename)));
    if (!serialized) {
      // Some test data is very large and needs to be downloaded separately,
      // so it may not always be present. Do not fail, but report the test as
      // skipped.
      result.skipped = true;
      return result;
    }
    std::optional<base::DictValue> json =
        base::JSONReader::ReadDict(base::as_string_view(*serialized),
                                   base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    CHECK(json.has_value());
    page = LoadDumpedPage(*json, result.parse_time, reporter);
  }

  page->GetDocument()
      .GetStyleEngine()
      .GetStyleResolver()
      .SetCountComputedStyleBytes(measure_computed_style_memory);

  if (!parse_only) {
    {
      base::ElapsedTimer style_timer;
      for (int i = 0; i < recalc_iterations; ++i) {
        page->GetDocument().UpdateStyleAndLayoutTreeForThisDocument();
        if (i != recalc_iterations - 1) {
          page->GetDocument().GetStyleEngine().MarkAllElementsForStyleRecalc(
              StyleChangeReasonForTracing::Create("test"));
        }
      }
      result.initial_style_time = style_timer.Elapsed();
    }

    page->GetDocument().GetStyleEngine().MarkAllElementsForStyleRecalc(
        StyleChangeReasonForTracing::Create("test"));

    {
      base::ElapsedTimer style_timer;
      page->GetDocument().UpdateStyleAndLayoutTreeForThisDocument();
      result.recalc_style_time = style_timer.Elapsed();
    }
  }

  // Loading the document may have posted tasks, which can hold on to memory.
  // Run them now, to make sure they don't leak or otherwise skew the
  // statistics.
  test::RunPendingTasks();

  size_t gc_allocated_bytes = blink::ProcessHeap::TotalAllocatedObjectSize();
  size_t partition_allocated_bytes = Partitions::TotalSizeOfCommittedPages();

  result.gc_allocated_bytes = gc_allocated_bytes - orig_gc_allocated_bytes;
  result.partition_allocated_bytes =
      partition_allocated_bytes - orig_partition_allocated_bytes;
  if (measure_computed_style_memory) {
    result.computed_style_used_bytes = page->GetDocument()
                                           .GetStyleEngine()
                                           .GetStyleResolver()
                                           .GetComputedStyleBytesUsed();
  }

  return result;
}

static void MeasureAndPrintStyleForDumpedPage(std::string_view filename,
                                              std::string_view label) {
  auto reporter = perf_test::PerfResultReporter("BlinkStyle", label);
  const bool parse_only =
      base::CommandLine::ForCurrentProcess()->HasSwitch("parse-style-only");

  StylePerfResult result =
      MeasureStyleForDumpedPage(filename, parse_only, &reporter);
  if (result.skipped) {
    GTEST_SKIP() << "Skipping " << label << " test because " << filename
                 << " could not be read";
  }

  if (!parse_only) {
    reporter.RegisterImportantMetric("InitialCalcTime", "us");
    reporter.AddResult("InitialCalcTime", result.initial_style_time);

    reporter.RegisterImportantMetric("RecalcTime", "us");
    reporter.AddResult("RecalcTime", result.recalc_style_time);
  }

  if (result.computed_style_used_bytes > 0) {
    reporter.RegisterImportantMetric("ComputedStyleUsed", "kB");
    reporter.AddResult(
        "ComputedStyleUsed",
        static_cast<size_t>(result.computed_style_used_bytes) / 1024);

    // Don't print GCAllocated if we measured ComputedStyle; it causes
    // much more GC churn, which will skew the metrics.
  } else {
    reporter.RegisterImportantMetric("GCAllocated", "kB");
    reporter.AddResult("GCAllocated",
                       static_cast<size_t>(result.gc_allocated_bytes) / 1024);
  }

  reporter.RegisterImportantMetric("PartitionAllocated", "kB");
  reporter.AddResult(
      "PartitionAllocated",
      static_cast<size_t>(result.partition_allocated_bytes) / 1024);
}

TEST(StyleCalcPerfTest, Video) {
  MeasureAndPrintStyleForDumpedPage("video.json", "Video");
}

TEST(StyleCalcPerfTest, Extension) {
  MeasureAndPrintStyleForDumpedPage("extension.json", "Extension");
}

TEST(StyleCalcPerfTest, News) {
  MeasureAndPrintStyleForDumpedPage("news.json", "News");
}

TEST(StyleCalcPerfTest, ECommerce) {
  MeasureAndPrintStyleForDumpedPage("ecommerce.json", "ECommerce");
}

TEST(StyleCalcPerfTest, Social1) {
  MeasureAndPrintStyleForDumpedPage("social1.json", "Social1");
}

TEST(StyleCalcPerfTest, Social2) {
  MeasureAndPrintStyleForDumpedPage("social2.json", "Social2");
}

TEST(StyleCalcPerfTest, Encyclopedia) {
  MeasureAndPrintStyleForDumpedPage("encyclopedia.json", "Encyclopedia");
}

TEST(StyleCalcPerfTest, Sports) {
  MeasureAndPrintStyleForDumpedPage("sports.json", "Sports");
}

TEST(StyleCalcPerfTest, Search) {
  MeasureAndPrintStyleForDumpedPage("search.json", "Search");
}

// The data set for this test is not checked in, so if you want to measure it,
// you will need to recreate it yourself. You can do so using the script in
//
//   third_party/blink/renderer/core/css/scripts/style_perftest_snap_page
//
// And the URL set to use is the top 1k URLs from
//
//   tools/perf/page_sets/alexa1-10000-urls.json
TEST(StyleCalcPerfTest, Alexa1000) {
  std::vector<StylePerfResult> results;
  const bool parse_only =
      base::CommandLine::ForCurrentProcess()->HasSwitch("parse-style-only");

  for (int i = 1; i <= 1000; ++i) {
    StylePerfResult result = MeasureStyleForDumpedPage(
        absl::StrFormat("alexa%04d.json", i), parse_only, /*reporter=*/nullptr);
    if (!result.skipped) {
      results.push_back(result);
    }
    if (i % 100 == 0) {
      LOG(INFO) << "Benchmarked " << results.size() << " pages, skipped "
                << (i - results.size()) << "...";
    }
    if (i == 10 && results.empty()) {
      LOG(INFO) << "The Alexa 1k test set has not been dumped "
                << "(tried the first 10), skipping it.";
      return;
    }
  }

  if (results.empty()) {
    return;
  }

  auto reporter = perf_test::PerfResultReporter("BlinkStyle", "Alexa1000");
  for (double percentile : {0.5, 0.9, 0.99}) {
    size_t pos = std::min<size_t>(lrint(results.size() * percentile),
                                  results.size() - 1);

    auto add_metric = [&](std::string_view name, std::string_view unit,
                          auto transform, auto projection) {
      std::ranges::nth_element(results, results.begin() + pos, {}, projection);
      std::string label =
          absl::StrFormat("%s%.0fthPercentile", name, percentile * 100.0);
      reporter.RegisterImportantMetric(label, unit);
      reporter.AddResult(label,
                         transform(std::invoke(projection, results[pos])));
    };
    auto to_kb = [](int64_t v) { return static_cast<size_t>(v) / 1024; };
    auto to_us = [](base::TimeDelta t) { return t; };

    add_metric("ParseTime", "us", to_us, &StylePerfResult::parse_time);

    if (!parse_only) {
      add_metric("InitialCalcTime", "us", to_us,
                 &StylePerfResult::initial_style_time);
      add_metric("RecalcTime", "us", to_us,
                 &StylePerfResult::recalc_style_time);
    }

    add_metric("GCAllocated", "kB", to_kb,
               &StylePerfResult::gc_allocated_bytes);
    add_metric("PartitionAllocated", "kB", to_kb,
               &StylePerfResult::partition_allocated_bytes);
  }
}

}  // namespace blink
