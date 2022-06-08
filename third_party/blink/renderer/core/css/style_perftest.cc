// Copyright 2022 The Chromium Authors. All rights reserved.
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

#include "base/json/json_reader.h"
#include "testing/perf/perf_result_reporter.h"
#include "testing/perf/perf_test.h"
#include "third_party/blink/public/platform/web_back_forward_cache_loader_helper.h"
#include "third_party/blink/renderer/core/css/container_query_data.h"
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
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/process_heap.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

// A WebURLLoader simulating that requests time out forever due to no network.
// (We don't really want to benchmark URL loading.)
class NoNetworkWebURLLoader : public WebURLLoader {
 public:
  NoNetworkWebURLLoader() = default;
  NoNetworkWebURLLoader(const NoNetworkWebURLLoader&) = delete;
  NoNetworkWebURLLoader& operator=(const NoNetworkWebURLLoader&) = delete;

  // WebURLLoader member functions:
  void LoadSynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      bool pass_response_pipe_to_client,
      bool no_mime_sniffing,
      base::TimeDelta timeout_interval,
      WebURLLoaderClient* client,
      WebURLResponse& response,
      absl::optional<WebURLError>&,
      WebData&,
      int64_t& encoded_data_length,
      int64_t& encoded_body_length,
      blink::WebBlobInfo& downloaded_blob,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper) override {
    // Nothing should call this in our test.
    NOTREACHED();
  }
  void LoadAsynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      bool no_mime_sniffing,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper,
      WebURLLoaderClient* client) override {
    // We simply never call back, simulating load times that are larger
    // than the test runtime.
  }
  void Freeze(WebLoaderFreezeMode mode) override {
    // Ignore.
  }
  void DidChangePriority(WebURLRequest::Priority new_priority,
                         int intra_priority_value) override {
    // Ignore.
  }
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForBodyLoader()
      override {
    return base::MakeRefCounted<scheduler::FakeTaskRunner>();
  }
};

class NoNetworkWebURLLoaderFactory : public WebURLLoaderFactory {
 public:
  NoNetworkWebURLLoaderFactory() = default;

  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const WebURLRequest&,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>,
      CrossVariantMojoRemote<blink::mojom::KeepAliveHandleInterfaceBase>,
      WebBackForwardCacheLoaderHelper) override {
    return std::make_unique<NoNetworkWebURLLoader>();
  }
};

// A LocalFrameClient that uses NoNetworkWebURLLoader, so that nothing external
// is ever loaded.
class NoNetworkLocalFrameClient : public EmptyLocalFrameClient {
 public:
  NoNetworkLocalFrameClient() = default;

 private:
  std::unique_ptr<WebURLLoaderFactory> CreateURLLoaderFactory() override {
    return std::make_unique<NoNetworkWebURLLoaderFactory>();
  }
};

static std::unique_ptr<DummyPageHolder> LoadDumpedPage(
    const base::Value::Dict& dict,
    perf_test::PerfResultReporter& reporter) {
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

  base::ElapsedTimer parse_timer;
  for (const base::Value& sheet_json : *dict.FindList("stylesheets")) {
    const base::Value::Dict& sheet_dict = sheet_json.GetDict();
    auto* sheet = MakeGarbageCollected<StyleSheetContents>(
        MakeGarbageCollected<CSSParserContext>(document));
    sheet->ParseString(WTF::String(*sheet_dict.FindString("text")));
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

  reporter.RegisterImportantMetric("ParseTime", "us");
  reporter.AddResult("ParseTime", parse_time);

  return page;
}

static void MeasureStyleForDumpedPage(const char* filename, const char* label) {
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
    scoped_refptr<SharedBuffer> serialized = test::ReadFromFile(filename);
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
    page->GetDocument().UpdateStyleAndLayoutTreeForThisDocument();
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
