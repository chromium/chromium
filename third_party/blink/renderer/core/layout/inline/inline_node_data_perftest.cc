// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/inline_node_data.h"

#include "skia/ext/codec_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/testing/no_network_url_loader.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

TEST(InlineNodeDataPerfTest, CountOnHTML5Spec) {
  skia::EnsurePNGDecoderRegistered();
  String html5_path =
      test::BlinkRootDir() + "/perf_tests/parser/resources/html5.html";
  std::optional<Vector<char>> file_data = test::ReadFromFile(html5_path);
  ASSERT_TRUE(file_data.has_value());

  auto page = std::make_unique<DummyPageHolder>(
      gfx::Size(800, 600), nullptr,
      MakeGarbageCollected<NoNetworkLocalFrameClient>());
  page->GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);
  page->GetPage().SetDefaultPageScaleLimits(1, 4);

  Document& document = page->GetDocument();
  auto html_span = base::span<const char>(*file_data);
  String html_content(html_span);
  document.documentElement()->SetInnerHTMLWithoutTrustedTypes(
      html_content, ASSERT_NO_EXCEPTION);

  // Perform full style and layout.
  document.UpdateStyleAndLayoutTree();
  document.View()->UpdateAllLifecyclePhasesForTest();

  // Walk the layout tree and count InlineNodeData instances.
  unsigned inline_node_data_count = 0;
  const LayoutView* layout_view = document.GetLayoutView();
  for (const LayoutObject* object = layout_view; object;
       object = object->NextInPreOrder()) {
    if (object->IsLayoutBlockFlow()) {
      const auto* block_flow = To<LayoutBlockFlow>(object);
      if (block_flow->GetInlineNodeData()) {
        ++inline_node_data_count;
      }
    }
  }

  auto reporter = perf_test::PerfResultReporter("InlineNodeData", "HTML5Spec");
  reporter.RegisterImportantMetric("Count", "count");
  reporter.AddResult("Count", static_cast<double>(inline_node_data_count));

  reporter.RegisterImportantMetric("SizeofBytes", "bytes");
  reporter.AddResult("SizeofBytes",
                     static_cast<double>(sizeof(InlineNodeData)));

  reporter.RegisterImportantMetric("TotalBytes", "bytes");
  reporter.AddResult("TotalBytes", static_cast<double>(inline_node_data_count *
                                                       sizeof(InlineNodeData)));

  LOG(INFO) << "InlineNodeData count: " << inline_node_data_count;
  LOG(INFO) << "sizeof(InlineNodeData): " << sizeof(InlineNodeData);
  LOG(INFO) << "Total InlineNodeData bytes: "
            << inline_node_data_count * sizeof(InlineNodeData);
}

}  // namespace blink
