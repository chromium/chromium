// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A benchmark to verify style performance (and also hooks into layout,
// but not generally layout itself). This isolates style from paint etc.,
// for more stable benchmarking and profiling. Note that this test
// depends on external JSON files with stored web pages, which are
// not yet checked in. The tests will be skipped if you don't have the
// files available.

#include "third_party/blink/renderer/core/css/style_recalc_change.h"

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "testing/perf/perf_result_reporter.h"
#include "testing/perf/perf_test.h"
#include "third_party/blink/public/platform/web_back_forward_cache_loader_helper.h"
#include "third_party/blink/renderer/core/css/container_query_data.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/no_network_web_url_loader.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/process_heap.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

static std::unique_ptr<DummyPageHolder> LoadDumpedPage(
    const base::Value::Dict& dict,
    perf_test::PerfResultReporter& reporter) {
  const std::string parse_iterations_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "style-parse-iterations");
  int parse_iterations =
      parse_iterations_str.empty() ? 1 : stoi(parse_iterations_str);

  auto page = std::make_unique<DummyPageHolder>(
      gfx::Size(800, 600), nullptr,
      MakeGarbageCollected<NoNetworkLocalFrameClient>());
  page->GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);
  page->GetPage().SetDefaultPageScaleLimits(1, 4);

  Document& document = page->GetDocument();
  StyleEngine& engine = document.GetStyleEngine();
  document.body()->setInnerHTML(WTF::String(*dict.FindString("html")),
                                ASSERT_NO_EXCEPTION);

  int num_sheets = 0;
  int num_bytes = 0;

  // If --pre-tokenize is given, we do all the tokenization outside of
  // the timer (simulating the case where it's already tokenized for us
  // on a different thread).
  const bool pre_tokenize =
      base::CommandLine::ForCurrentProcess()->HasSwitch("pre-tokenize");
  std::vector<std::unique_ptr<CachedCSSTokenizer>> tokenizers;
  base::ElapsedTimer tokenize_timer;
  for (const base::Value& sheet_json : *dict.FindList("stylesheets")) {
    const base::Value::Dict& sheet_dict = sheet_json.GetDict();
    if (pre_tokenize) {
      tokenizers.push_back(CSSTokenizer::CreateCachedTokenizer(
          WTF::String(*sheet_dict.FindString("text"))));
      // Extra tokenizers are just a copy of the first.
      for (int i = 1; i < parse_iterations; ++i) {
        tokenizers.push_back(tokenizers.back()->DuplicateForTesting());
      }
    } else {
      for (int i = 0; i < parse_iterations; ++i) {
        tokenizers.push_back(nullptr);
      }
    }
  }
  base::TimeDelta tokenize_time = tokenize_timer.Elapsed();

  base::ElapsedTimer parse_timer;
  int tokenizer_idx = 0;
  for (const base::Value& sheet_json : *dict.FindList("stylesheets")) {
    const base::Value::Dict& sheet_dict = sheet_json.GetDict();
    auto* sheet = MakeGarbageCollected<StyleSheetContents>(
        MakeGarbageCollected<CSSParserContext>(document));

    for (int i = 0; i < parse_iterations; ++i) {
      sheet->ParseString(WTF::String(*sheet_dict.FindString("text")),
                         /*allow_import_rules=*/true,
                         std::move(tokenizers[tokenizer_idx++]));
    }
    if (*sheet_dict.FindString("type") == "user") {
      engine.InjectSheet("", sheet, WebCssOrigin::kUser);
    } else {
      engine.InjectSheet("", sheet, WebCssOrigin::kAuthor);
    }
    ++num_sheets;
    num_bytes += sheet_dict.FindString("text")->size();
  }
  base::TimeDelta parse_time = parse_timer.Elapsed();

  reporter.RegisterFyiMetric("NumSheets", "");
  reporter.AddResult("NumSheets", static_cast<double>(num_sheets));

  reporter.RegisterFyiMetric("SheetSize", "kB");
  reporter.AddResult("SheetSize", static_cast<double>(num_bytes / 1024));

  if (pre_tokenize) {
    reporter.RegisterImportantMetric("TokenizeTime", "us");
    reporter.AddResult("TokenizeTime", tokenize_time);
  }

  reporter.RegisterImportantMetric("ParseTime", "us");
  reporter.AddResult("ParseTime", parse_time);

  return page;
}

static void MeasureStyleForDumpedPage(const char* filename, const char* label) {
  // Running more than once is useful for profiling. (If this flag does not
  // exist, it will return the empty string.)
  const std::string recalc_iterations_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "style-recalc-iterations");
  int recalc_iterations =
      recalc_iterations_str.empty() ? 1 : stoi(recalc_iterations_str);

  auto reporter = perf_test::PerfResultReporter("BlinkStyle", label);

  // Do a forced GC run before we start loading anything, so that we have
  // a more stable baseline. Note that even with this, the GC deltas tend to
  // be different depending on what other tests that run before, so if you want
  // the more consistent memory numbers, you'll need to run only a single test
  // only (e.g. --gtest_filter=StyleCalcPerfTest.Video).
  ThreadState::Current()->CollectAllGarbageForTesting();

  size_t orig_gc_allocated_bytes =
      blink::ProcessHeap::TotalAllocatedObjectSize();
  size_t orig_partition_allocated_bytes =
      WTF::Partitions::TotalSizeOfCommittedPages();

  std::unique_ptr<DummyPageHolder> page;

  {
    scoped_refptr<SharedBuffer> serialized =
        test::ReadFromFile(test::StylePerfTestDataPath(filename));
    absl::optional<base::Value> json = base::JSONReader::Read(
        base::StringPiece(serialized->Data(), serialized->size()));
    if (!json.has_value()) {
      char msg[256];
      snprintf(msg, sizeof(msg),
               "Skipping %s test because %s could not be read", label,
               filename);
      GTEST_SKIP_(msg);
    }
    page = LoadDumpedPage(json->GetDict(), reporter);
  }

  {
    base::ElapsedTimer style_timer;
    for (int i = 0; i < recalc_iterations; ++i) {
      page->GetDocument().UpdateStyleAndLayoutTreeForThisDocument();
      if (i != recalc_iterations - 1) {
        page->GetDocument().GetStyleEngine().MarkAllElementsForStyleRecalc(
            StyleChangeReasonForTracing::Create("test"));
      }
    }
    base::TimeDelta style_time = style_timer.Elapsed();
    reporter.RegisterImportantMetric("InitialCalcTime", "us");
    reporter.AddResult("InitialCalcTime", style_time);
  }

  page->GetDocument().GetStyleEngine().MarkAllElementsForStyleRecalc(
      StyleChangeReasonForTracing::Create("test"));

  {
    base::ElapsedTimer style_timer;
    page->GetDocument().UpdateStyleAndLayoutTreeForThisDocument();
    base::TimeDelta style_time = style_timer.Elapsed();
    reporter.RegisterImportantMetric("RecalcTime", "us");
    reporter.AddResult("RecalcTime", style_time);
  }

  // Loading the document may have posted tasks, which can hold on to memory.
  // Run them now, to make sure they don't leak or otherwise skew the
  // statistics.
  test::RunPendingTasks();

  size_t gc_allocated_bytes = blink::ProcessHeap::TotalAllocatedObjectSize();
  size_t partition_allocated_bytes =
      WTF::Partitions::TotalSizeOfCommittedPages();

  reporter.RegisterImportantMetric("GCAllocated", "kB");
  reporter.AddResult("GCAllocated",
                     (gc_allocated_bytes - orig_gc_allocated_bytes) / 1024);
  reporter.RegisterImportantMetric("PartitionAllocated", "kB");
  reporter.AddResult(
      "PartitionAllocated",
      (partition_allocated_bytes - orig_partition_allocated_bytes) / 1024);
}

TEST(StyleCalcPerfTest, Video) {
  MeasureStyleForDumpedPage("video.json", "Video");
}

TEST(StyleCalcPerfTest, Extension) {
  MeasureStyleForDumpedPage("extension.json", "Extension");
}

TEST(StyleCalcPerfTest, News) {
  MeasureStyleForDumpedPage("news.json", "News");
}

TEST(StyleCalcPerfTest, ECommerce) {
  MeasureStyleForDumpedPage("ecommerce.json", "ECommerce");
}

TEST(StyleCalcPerfTest, Social1) {
  MeasureStyleForDumpedPage("social1.json", "Social1");
}

TEST(StyleCalcPerfTest, Social2) {
  MeasureStyleForDumpedPage("social2.json", "Social2");
}

TEST(StyleCalcPerfTest, Encyclopedia) {
  MeasureStyleForDumpedPage("encyclopedia.json", "Encyclopedia");
}

TEST(StyleCalcPerfTest, Sports) {
  MeasureStyleForDumpedPage("sports.json", "Sports");
}

TEST(StyleCalcPerfTest, Search) {
  MeasureStyleForDumpedPage("search.json", "Search");
}

}  // namespace blink
