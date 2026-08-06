// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/container_timing.h"

#include "base/timer/lap_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/paint/timing/container_timing_test_utils.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

constexpr int kLaps = 1000;
constexpr int kWarmupLaps = 5;
constexpr char kMetricRunsPerSecond[] = "runs_per_second";

// Selects which paint path the benchmark exercises. The variant toggles the
// ContainerTimingPrepaintTraversal feature and whether the full lifecycle is
// run to populate the tracker; everything else is identical.
enum class Variant { kLegacy, kPrePaint };

const char* VariantSuffix(Variant v) {
  return v == Variant::kPrePaint ? "prepaint" : "legacy";
}

perf_test::PerfResultReporter SetUpReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter("ContainerTimingPerfTest.", story);
  reporter.RegisterImportantMetric(kMetricRunsPerSecond, "runs/s");
  return reporter;
}

// Generates a structure of nested DOM elements interleaving container timing
// roots and content nodes. The container_depth sets how many nested container
// timing roots are created. Then, non_container_depth is the depth of the
// content elements each root has nested inside.
//
// Each root will be attached to the deepest content element of the parent root.
//
// For container_depth=2, non_container_depth=3:
//   <ct_root_1 containertiming="ct_1">
//     <content_1_1>
//       <content_1_2>
//         <content_1_3>
//           <ct_root_2 containertiming="ct_2">
//             <content_2_1>
//               <content_2_2>
//                 <content_2_3/>  ...
String GenerateNestedHTML(size_t container_depth, size_t non_container_depth) {
  StringBuilder html;
  for (size_t ct_root_index = 1; ct_root_index <= container_depth;
       ++ct_root_index) {
    html.AppendFormat("<div id='ct_root_%zu' containertiming='ct_%zu'>",
                      ct_root_index, ct_root_index);
    for (size_t content_index = 1; content_index <= non_container_depth;
         ++content_index) {
      // Give the innermost leaf content element a CSS background image so that
      // IsImageType() returns true and marked_nodes_ is populated by the
      // pre-paint tracker. Without this, GetContainerRootFor() always
      // misses and the PrePaint benchmark never exercises the fast path.
      if (ct_root_index == container_depth &&
          content_index == non_container_depth) {
        html.AppendFormat(
            "<div id='content_%zu_%zu' "
            "style='background-image:linear-gradient(red,red)'>",
            ct_root_index, content_index);
      } else {
        html.AppendFormat("<div id='content_%zu_%zu'>", ct_root_index,
                          content_index);
      }
    }
  }
  for (size_t i = 0; i < container_depth; ++i) {
    for (size_t j = 0; j < non_container_depth; ++j) {
      html.Append("</div>");  // content node
    }
    html.Append("</div>");  // ct_root
  }
  return html.ToString();
}

// A container timing root holding `image_count` <img> elements. Real <img>s,
// unlike the nested generator's CSS background images, so the pre-paint walk
// records actual image-generating nodes.
String GenerateImagesHTML(size_t image_count) {
  StringBuilder html;
  html.Append("<div id='ct_root' containertiming='ct'>");
  for (size_t i = 1; i <= image_count; ++i) {
    html.AppendFormat("<img id='image_%zu' style='width:10px;height:10px'>", i);
  }
  html.Append("</div>");
  return html.ToString();
}

// Per-image attribution: each lap paints a different image, where the depth
// benchmarks repeat one element. No resources are loaded; the load
// notification is a single attribute check.
void RunImageAttributionBenchmark(Variant variant,
                                  const std::string& story_base,
                                  size_t image_count) {
  const bool use_prepaint = variant == Variant::kPrePaint;
  ScopedContainerTimingPrepaintTraversalForTest scoped_feature(use_prepaint);
  auto page = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = page->GetDocument();
  document.body()->SetInnerHTMLWithoutTrustedTypes(
      GenerateImagesHTML(image_count));
  if (use_prepaint) {
    // Populate the tracker, else both variants measure the legacy path.
    page->GetFrameView().UpdateAllLifecyclePhasesForTest();
  } else {
    document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  }

  ContainerTiming& container_timing =
      ContainerTiming::From(*document.domWindow());

  HeapVector<Member<Element>> images;
  images.ReserveInitialCapacity(static_cast<wtf_size_t>(image_count));
  for (size_t i = 1; i <= image_count; ++i) {
    Element* image =
        document.getElementById(AtomicString(String::Format("image_%zu", i)));
    ASSERT_TRUE(image);
    images.push_back(image);
  }

  // Fail rather than time empty calls if the images were not attributed.
  auto* performance = DOMWindowPerformance::performance(*document.domWindow());
  SimulateContainerTimingPaint(container_timing, images.front(),
                               gfx::RectF(0, 0, 10, 10));
  performance->PopulateContainerTimingEntries();
  ASSERT_FALSE(
      performance->getBufferedEntriesByType(AtomicString("container")).empty());

  base::LapTimer timer(kWarmupLaps, base::TimeDelta(), kLaps);
  for (int i = 0; i < kLaps + kWarmupLaps; ++i) {
    Element* image = images[static_cast<wtf_size_t>(i) % images.size()];
    SimulateContainerTimingPaint(
        container_timing, image,
        gfx::RectF((i % 1000) * 10, (i / 1000) * 10, 10, 10));
    timer.NextLap();
  }

  auto reporter = SetUpReporter(story_base + "_" + VariantSuffix(variant));
  reporter.AddResult(kMetricRunsPerSecond, timer.LapsPerSecond());
}

// Unified paint benchmark. The variant selects whether the legacy DOM-walk
// fallback or the pre-paint tracker fast path is exercised. The story name
// preserves the legacy/prepaint suffix so the perf history under the old
// names remains comparable.
//
// `same_rect`: when true, every lap paints the same rect; after the first
// paint, MaybeUpdateLastNewPaintedArea early-returns (Contains == true), so
// the loop measures traversal/lookup cost without cc::Region accumulation.
// When false, each lap paints a unique rect — the region grows and Region
// operations dominate the lap cost.
void RunPaintBenchmark(Variant variant,
                       const std::string& story_base,
                       size_t container_timing_depth,
                       size_t non_container_depth,
                       bool same_rect = false) {
  const bool use_prepaint = variant == Variant::kPrePaint;
  ScopedContainerTimingPrepaintTraversalForTest scoped_feature(use_prepaint);
  auto page = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = page->GetDocument();
  document.body()->SetInnerHTMLWithoutTrustedTypes(
      GenerateNestedHTML(container_timing_depth, non_container_depth));
  if (use_prepaint) {
    // Run the full lifecycle so the real pre-paint walk populates the tracker
    // and clears the ContainerTimingChanged staleness bits set during HTML
    // parsing. Without this, OnElementPainted() falls back to the legacy
    // ParentContainerRootFallback() walk every iteration and the benchmark
    // measures the legacy path under both flag states.
    page->GetFrameView().UpdateAllLifecyclePhasesForTest();
  } else {
    document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  }

  ContainerTiming& container_timing =
      ContainerTiming::From(*document.domWindow());

  AtomicString content_id(String::Format(
      "content_%zu_%zu", container_timing_depth, non_container_depth));
  Element* content = document.getElementById(content_id);
  ASSERT_TRUE(content);

  base::LapTimer timer(kWarmupLaps, base::TimeDelta(), kLaps);

  for (int i = 0; i < kLaps + kWarmupLaps; ++i) {
    gfx::RectF rect =
        same_rect ? gfx::RectF(0, 0, 10, 10)
                  : gfx::RectF((i % 1000) * 10, (i / 1000) * 10, 10, 10);
    SimulateContainerTimingPaint(container_timing, content, rect);
    timer.NextLap();
  }

  std::string story = story_base + "_" + VariantSuffix(variant);
  if (same_rect) {
    story += "_same_rect";
  }
  auto reporter = SetUpReporter(story);
  reporter.AddResult(kMetricRunsPerSecond, timer.LapsPerSecond());
}

// Cache-invalidation benchmark: every lap changes one root's identifier, which
// drops that root's Record so the next paint has to recreate it.
void RunInvalidationCycleBenchmark(Variant variant,
                                   const std::string& story_base,
                                   size_t container_timing_depth,
                                   size_t non_container_timing_depth,
                                   size_t changing_ct_root) {
  const bool use_prepaint = variant == Variant::kPrePaint;
  ScopedContainerTimingPrepaintTraversalForTest scoped_feature(use_prepaint);
  auto page = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = page->GetDocument();
  document.body()->SetInnerHTMLWithoutTrustedTypes(
      GenerateNestedHTML(container_timing_depth, non_container_timing_depth));
  if (use_prepaint) {
    page->GetFrameView().UpdateAllLifecyclePhasesForTest();
  } else {
    document.UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  }

  ContainerTiming& container_timing =
      ContainerTiming::From(*document.domWindow());
  AtomicString ct_root_id(String::Format("ct_root_%zu", changing_ct_root));
  Element* ct_root = document.getElementById(ct_root_id);
  AtomicString content_id(String::Format(
      "content_%zu_%zu", container_timing_depth, non_container_timing_depth));
  Element* content = document.getElementById(content_id);
  ASSERT_TRUE(ct_root);
  ASSERT_TRUE(content);

  // Precompute the two identifiers so the measured loop doesn't allocate and
  // atomize a new string every lap. Alternating between them guarantees the
  // value actually changes each lap.
  const AtomicString value_even("new_value0");
  const AtomicString value_odd("new_value1");

  base::LapTimer timer(kWarmupLaps, base::TimeDelta(), kLaps);

  for (int i = 0; i < kLaps + kWarmupLaps; ++i) {
    // Change the attribute value. Under prepaint the tracker entry still
    // points to the same element (which retains the containertiming
    // attribute with a new value); the FastHasAttribute guard in
    // OnElementPainted passes and the paint proceeds normally via the
    // tracker path.
    ct_root->setAttribute(html_names::kContainertimingAttr,
                          (i % 2) ? value_odd : value_even);

    gfx::RectF rect((i % 1000) * 10, (i / 1000) * 10, 10, 10);
    SimulateContainerTimingPaint(container_timing, content, rect);
    timer.NextLap();
  }

  std::string story =
      "invalidation_cycle_" + story_base + "_" + VariantSuffix(variant);
  auto reporter = SetUpReporter(story);
  reporter.AddResult(kMetricRunsPerSecond, timer.LapsPerSecond());
}

}  // namespace

class ContainerTimingPerfTest : public ::testing::TestWithParam<Variant> {};

INSTANTIATE_TEST_SUITE_P(All,
                         ContainerTimingPerfTest,
                         ::testing::Values(Variant::kLegacy,
                                           Variant::kPrePaint),
                         [](const ::testing::TestParamInfo<Variant>& info) {
                           return std::string(VariantSuffix(info.param));
                         });

TEST_P(ContainerTimingPerfTest, PaintPropagation_Depth10_1) {
  RunPaintBenchmark(GetParam(), "depth_10_1", 10, 1);
}

TEST_P(ContainerTimingPerfTest, PaintPropagation_Depth10_1_SameRect) {
  RunPaintBenchmark(GetParam(), "depth_10_1", 10, 1, /*same_rect=*/true);
}

TEST_P(ContainerTimingPerfTest, PaintPropagation_Depth10_100) {
  RunPaintBenchmark(GetParam(), "depth_10_100", 10, 100);
}

TEST_P(ContainerTimingPerfTest, PaintPropagation_Depth10_100_SameRect) {
  RunPaintBenchmark(GetParam(), "depth_10_100", 10, 100, /*same_rect=*/true);
}

TEST_P(ContainerTimingPerfTest, PaintPropagation_Depth50_1) {
  RunPaintBenchmark(GetParam(), "depth_50_1", 50, 1);
}

TEST_P(ContainerTimingPerfTest, PaintPropagation_Depth50_1_SameRect) {
  RunPaintBenchmark(GetParam(), "depth_50_1", 50, 1, /*same_rect=*/true);
}

TEST_P(ContainerTimingPerfTest, PaintPropagation_Depth50_100) {
  RunPaintBenchmark(GetParam(), "depth_50_100", 50, 100);
}

TEST_P(ContainerTimingPerfTest, PaintPropagation_Depth50_100_SameRect) {
  RunPaintBenchmark(GetParam(), "depth_50_100", 50, 100, /*same_rect=*/true);
}

TEST_P(ContainerTimingPerfTest, CacheInvalidationCycle_Depth10_1_5) {
  RunInvalidationCycleBenchmark(GetParam(), "depth10_1_5", 10, 1, 5);
}

TEST_P(ContainerTimingPerfTest, CacheInvalidationCycle_Depth10_1_1) {
  RunInvalidationCycleBenchmark(GetParam(), "depth10_1_1", 10, 1, 1);
}

TEST_P(ContainerTimingPerfTest, CacheInvalidationCycle_Depth10_100_5) {
  RunInvalidationCycleBenchmark(GetParam(), "depth10_100_5", 10, 100, 5);
}

TEST_P(ContainerTimingPerfTest, CacheInvalidationCycle_Depth10_100_1) {
  RunInvalidationCycleBenchmark(GetParam(), "depth10_100_1", 10, 100, 1);
}

TEST_P(ContainerTimingPerfTest, ImageAttribution_100) {
  RunImageAttributionBenchmark(GetParam(), "images_100", 100);
}

TEST_P(ContainerTimingPerfTest, ImageAttribution_1000) {
  RunImageAttributionBenchmark(GetParam(), "images_1000", 1000);
}

}  // namespace blink
