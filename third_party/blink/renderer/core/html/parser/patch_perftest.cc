// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Benchmarks <template for> declarative partial update lookup across document
// streaming (in-order and out-of-order), unmatched markers in large DOMs, and
// innerHTML fragment parsing.
//
// Pass --disable-patch-marker-map to measure the legacy O(N * DOM) tree walk.

#include "base/command_line.h"
#include "base/timer/elapsed_timer.h"
#include "testing/perf/perf_result_reporter.h"
#include "testing/perf/perf_test.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/testing/no_network_url_loader.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

String BuildPatchBenchmarkHtml(int num_patches,
                               int num_padding_elements,
                               bool matched,
                               bool reverse_order) {
  StringBuilder html;
  // Use display:none on <main> so Document::close() does not dominate the
  // measurement with text shaping/layout of tens of thousands of nodes.
  html.Append("<!doctype html><html><body><main style=\"display:none\">");
  for (int i = 0; i < num_patches; ++i) {
    if (matched) {
      html.Append("<section><?start name=\"t");
      html.AppendNumber(i);
      html.Append("\"><i>wait ");
      html.AppendNumber(i);
      html.Append("</i><?end></section>");
    } else {
      html.Append("<section><i>wait ");
      html.AppendNumber(i);
      html.Append("</i></section>");
    }
  }
  for (int i = 0; i < num_padding_elements; ++i) {
    html.Append("<span>.</span>");
  }
  html.Append("</main>");
  for (int i = 0; i < num_patches; ++i) {
    int target = reverse_order ? (num_patches - 1 - i) : i;
    html.Append("<template id=\"t");
    html.AppendNumber(target);
    html.Append("\" for=\"t");
    html.AppendNumber(target);
    html.Append("\"><b>done ");
    html.AppendNumber(target);
    html.Append("</b></template>");
  }
  html.Append("</body></html>");
  return html.ReleaseString();
}

void RunDocumentPatchBenchmark(const char* story, const String& html) {
  const bool use_marker_map =
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          "disable-patch-marker-map");
  ScopedHTMLPatchingMarkerMapForTest marker_map_scope(use_marker_map);

  auto page = std::make_unique<DummyPageHolder>(
      gfx::Size(800, 600), nullptr,
      MakeGarbageCollected<NoNetworkLocalFrameClient>());
  page->GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);

  base::ElapsedTimer timer;
  page->GetDocument().SetContent(html);
  base::TimeDelta parse_time = timer.Elapsed();

  perf_test::PerfResultReporter reporter("BlinkHTMLPatch", story);
  reporter.RegisterImportantMetric("ParseTime", "us");
  reporter.AddResult("ParseTime", parse_time);
}

}  // namespace

TEST(PatchPerfTest, MatchedInOrder4000) {
  String html = BuildPatchBenchmarkHtml(
      /*num_patches=*/4000, /*num_padding_elements=*/0, /*matched=*/true,
      /*reverse_order=*/false);
  RunDocumentPatchBenchmark("MatchedInOrder4000", html);
}

TEST(PatchPerfTest, MatchedOutOfOrder4000) {
  String html = BuildPatchBenchmarkHtml(
      /*num_patches=*/4000, /*num_padding_elements=*/0, /*matched=*/true,
      /*reverse_order=*/true);
  RunDocumentPatchBenchmark("MatchedOutOfOrder4000", html);
}

TEST(PatchPerfTest, UnmatchedLargeDOM1000x20000) {
  String html = BuildPatchBenchmarkHtml(
      /*num_patches=*/1000, /*num_padding_elements=*/20000, /*matched=*/false,
      /*reverse_order=*/false);
  RunDocumentPatchBenchmark("UnmatchedLargeDOM1000x20000", html);
}

TEST(PatchPerfTest, InnerHTMLMatched2000) {
  const bool use_marker_map =
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          "disable-patch-marker-map");
  ScopedHTMLPatchingMarkerMapForTest marker_map_scope(use_marker_map);

  String html = BuildPatchBenchmarkHtml(
      /*num_patches=*/2000, /*num_padding_elements=*/5000, /*matched=*/true,
      /*reverse_order=*/true);

  auto page = std::make_unique<DummyPageHolder>(
      gfx::Size(800, 600), nullptr,
      MakeGarbageCollected<NoNetworkLocalFrameClient>());
  page->GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);

  base::ElapsedTimer timer;
  page->GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(html);
  base::TimeDelta parse_time = timer.Elapsed();

  perf_test::PerfResultReporter reporter("BlinkHTMLPatch",
                                         "InnerHTMLMatched2000");
  reporter.RegisterImportantMetric("ParseTime", "us");
  reporter.AddResult("ParseTime", parse_time);
}

}  // namespace blink
