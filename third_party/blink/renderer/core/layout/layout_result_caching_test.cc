// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/geometry/fragment_geometry.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/layout_utils.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {
namespace {

// These tests exercise the caching logic of |LayoutResult|s. They are
// rendering tests which contain two children: "test" and "src".
//
// Both have layout initially performed on them, however the "src" will have a
// different |ConstraintSpace| which is then used to test either a cache hit
// or miss.
class LayoutResultCachingTest : public RenderingTest {
 protected:
  LayoutResultCachingTest() {}

  const LayoutResult* TestCachedLayoutResultWithBreakToken(
      LayoutBox* box,
      const ConstraintSpace& constraint_space,
      const BlockBreakToken* break_token) {
    std::optional<FragmentGeometry> fragment_geometry;
    LayoutCacheStatus cache_status;
    return box->CachedLayoutResult(constraint_space, break_token, nullptr,
                                   nullptr, &fragment_geometry, &cache_status);
  }

  const LayoutResult* TestCachedLayoutResult(
      LayoutBox* box,
      const ConstraintSpace& constraint_space,
      LayoutCacheStatus* out_cache_status = nullptr) {
    std::optional<FragmentGeometry> fragment_geometry;
    LayoutCacheStatus cache_status;
    const LayoutResult* result =
        box->CachedLayoutResult(constraint_space, nullptr, nullptr, nullptr,
                                &fragment_geometry, &cache_status);
    if (out_cache_status) {
      *out_cache_status = cache_status;
    }
    return result;
  }
};

TEST_F(LayoutResultCachingTest, HitDifferentExclusionSpace) {
  // Same BFC offset, different exclusion space.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 20px;"></div>
      </div>
      <div id="test" style="height: 20px;"></div>
    </div>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 30px;"></div>
      </div>
      <div id="src" style="height: 20px;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result->BfcBlockOffset().value(), LayoutUnit(50));
  EXPECT_EQ(result->BfcLineOffset(), LayoutUnit());
}

TEST_F(LayoutResultCachingTest, HitDifferentBFCOffset) {
  // Different BFC offset, same exclusion space.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 20px;"></div>
      </div>
      <div id="test" style="height: 20px; padding-top: 5px;">
        <div class="float" style="height: 20px;"></div>
      </div>
    </div>
    <div class="bfc">
      <div style="height: 40px;">
        <div class="float" style="height: 20px;"></div>
      </div>
      <div id="src" style="height: 20px; padding-top: 5px;">
        <div class="float" style="height: 20px;"></div>
      </div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result->BfcBlockOffset().value(), LayoutUnit(40));
  EXPECT_EQ(result->BfcLineOffset(), LayoutUnit());

  // Also check that the exclusion(s) got moved correctly.
  LayoutOpportunityVector opportunities =
      result->GetExclusionSpace().AllLayoutOpportunities(
          /* offset */ {LayoutUnit(), LayoutUnit()},
          /* available_inline_size */ LayoutUnit(100));

  EXPECT_EQ(opportunities.size(), 3u);

  EXPECT_EQ(opportunities[0].rect.start_offset,
            BfcOffset(LayoutUnit(50), LayoutUnit()));
  EXPECT_EQ(opportunities[0].rect.end_offset,
            BfcOffset(LayoutUnit(100), LayoutUnit::Max()));

  EXPECT_EQ(opportunities[1].rect.start_offset,
            BfcOffset(LayoutUnit(), LayoutUnit(20)));
  EXPECT_EQ(opportunities[1].rect.end_offset,
            BfcOffset(LayoutUnit(100), LayoutUnit(45)));

  EXPECT_EQ(opportunities[2].rect.start_offset,
            BfcOffset(LayoutUnit(), LayoutUnit(65)));
  EXPECT_EQ(opportunities[2].rect.end_offset,
            BfcOffset(LayoutUnit(100), LayoutUnit::Max()));
}

TEST_F(LayoutResultCachingTest, HitDifferentBFCOffsetSameMarginStrut) {
  // Different BFC offset, same margin-strut.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
    </style>
    <div class="bfc">
      <div style="height: 50px; margin-bottom: 20px;"></div>
      <div id="test" style="height: 20px;"></div>
    </div>
    <div class="bfc">
      <div style="height: 40px; margin-bottom: 20px;"></div>
      <div id="src" style="height: 20px;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, MissDescendantAboveBlockStart1) {
  // Same BFC offset, different exclusion space, descendant above
  // block start.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 20px;"></div>
      </div>
      <div id="test" style="height: 20px; padding-top: 5px;">
        <div style="height: 10px; margin-top: -10px;"></div>
      </div>
    </div>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 30px;"></div>
      </div>
      <div id="src" style="height: 20px;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LayoutResultCachingTest, MissDescendantAboveBlockStart2) {
  // Different BFC offset, same exclusion space, descendant above
  // block start.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 20px;"></div>
      </div>
      <div id="test" style="height: 20px; padding-top: 5px;">
        <div style="height: 10px; margin-top: -10px;"></div>
      </div>
    </div>
    <div class="bfc">
      <div style="height: 40px;">
        <div class="float" style="height: 20px;"></div>
      </div>
      <div id="src" style="height: 20px;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitOOFDescendantAboveBlockStart) {
  // Different BFC offset, same exclusion space, OOF-descendant above
  // block start.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 20px;"></div>
      </div>
      <div id="test" style="position: relative; height: 20px; padding-top: 5px;">
        <div style="position: absolute; height: 10px; top: -10px;"></div>
      </div>
    </div>
    <div class="bfc">
      <div style="height: 40px;">
        <div class="float" style="height: 20px;"></div>
      </div>
      <div id="src" style="height: 20px;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitLineBoxDescendantAboveBlockStart) {
  // Different BFC offset, same exclusion space, line-box descendant above
  // block start.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 20px;"></div>
      </div>
      <div id="test" style="font-size: 12px;">
        text
        <span style="margin: 0 1px;">
          <span style="display: inline-block; vertical-align: text-bottom; width: 16px; height: 16px;"></span>
        </span>
      </div>
    </div>
    <div class="bfc">
      <div style="height: 40px;">
        <div class="float" style="height: 20px;"></div>
      </div>
      <div id="src" style="font-size: 12px;">
        text
      </div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, MissFloatInitiallyIntruding1) {
  // Same BFC offset, different exclusion space, float initially
  // intruding.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 60px;"></div>
      </div>
      <div id="test" style="height: 20px;"></div>
    </div>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 30px;"></div>
      </div>
      <div id="src" style="height: 20px;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LayoutResultCachingTest, MissFloatInitiallyIntruding2) {
  // Different BFC offset, same exclusion space, float initially
  // intruding.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 60px;"></div>
      </div>
      <div id="test" style="height: 60px;"></div>
    </div>
    <div class="bfc">
      <div style="height: 70px;">
        <div class="float" style="height: 60px;"></div>
      </div>
      <div id="src" style="height: 20px;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LayoutResultCachingTest, MissFloatWillIntrude1) {
  // Same BFC offset, different exclusion space, float will intrude.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 40px;"></div>
      </div>
      <div id="test" style="height: 20px;"></div>
    </div>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 60px;"></div>
      </div>
      <div id="src" style="height: 20px;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LayoutResultCachingTest, MissFloatWillIntrude2) {
  // Different BFC offset, same exclusion space, float will intrude.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 40px;"></div>
      </div>
      <div id="test" style="height: 60px;"></div>
    </div>
    <div class="bfc">
      <div style="height: 30px;">
        <div class="float" style="height: 40px;"></div>
      </div>
      <div id="src" style="height: 20px;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitPushedByFloats1) {
  // Same BFC offset, different exclusion space, pushed by floats.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 60px;"></div>
      </div>
      <div id="test" style="height: 20px; clear: left;"></div>
    </div>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 70px;"></div>
      </div>
      <div id="src" style="height: 20px; clear: left;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitPushedByFloats2) {
  // Different BFC offset, same exclusion space, pushed by floats.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 60px;"></div>
      </div>
      <div id="test" style="height: 20px; clear: left;"></div>
    </div>
    <div class="bfc">
      <div style="height: 30px;">
        <div class="float" style="height: 60px;"></div>
      </div>
      <div id="src" style="height: 20px; clear: left;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, MissPushedByFloats1) {
  // Same BFC offset, different exclusion space, pushed by floats.
  // Miss due to shrinking offset.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 70px;"></div>
      </div>
      <div id="test" style="height: 20px; clear: left;"></div>
    </div>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 60px;"></div>
      </div>
      <div id="src" style="height: 20px; clear: left;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LayoutResultCachingTest, MissPushedByFloats2) {
  // Different BFC offset, same exclusion space, pushed by floats.
  // Miss due to shrinking offset.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 50px; }
    </style>
    <div class="bfc">
      <div style="height: 30px;">
        <div class="float" style="height: 60px;"></div>
      </div>
      <div id="test" style="height: 20px; clear: left;"></div>
    </div>
    <div class="bfc">
      <div style="height: 50px;">
        <div class="float" style="height: 60px;"></div>
      </div>
      <div id="src" style="height: 20px; clear: left;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitDifferentRareData) {
  // Same absolute fixed constraints.
  SetBodyInnerHTML(R"HTML(
    <style>
      .container { position: relative; width: 100px; height: 100px; }
      .abs { position: absolute; width: 100px; height: 100px; top: 0; left: 0; }
    </style>
    <div class="container">
      <div id="test" class="abs"></div>
    </div>
    <div class="container" style="width: 200px; height: 200px;">
      <div id="src" class="abs"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitPercentageMinWidth) {
  // min-width calculates to different values, but doesn't change size.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .inflow { width: 100px; min-width: 25%; }
    </style>
    <div class="bfc">
      <div id="test" class="inflow"></div>
    </div>
    <div class="bfc" style="width: 200px; height: 200px;">
      <div id="src" class="inflow"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitFixedMinWidth) {
  // min-width is always larger than the available size.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .inflow { min-width: 300px; }
    </style>
    <div class="bfc">
      <div id="test" class="inflow"></div>
    </div>
    <div class="bfc" style="width: 200px; height: 200px;">
      <div id="src" class="inflow"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitShrinkToFit) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flow-root; width: 300px; height: 100px;">
      <div id="test1" style="float: left;">
        <div style="display: inline-block; width: 150px;"></div>
        <div style="display: inline-block; width: 50px;"></div>
      </div>
      <div id="test2" style="float: left;">
        <div style="display: inline-block; width: 350px;"></div>
        <div style="display: inline-block; width: 250px;"></div>
      </div>
    </div>
    <div style="display: flow-root; width: 400px; height: 100px;">
      <div id="src1" style="float: left;">
        <div style="display: inline-block; width: 150px;"></div>
        <div style="display: inline-block; width: 50px;"></div>
      </div>
    </div>
    <div style="display: flow-root; width: 200px; height: 100px;">
      <div id="src2" style="float: left;">
        <div style="display: inline-block; width: 350px;"></div>
        <div style="display: inline-block; width: 250px;"></div>
      </div>
    </div>
  )HTML");

  auto* test1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test1"));
  auto* test2 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test2"));
  auto* src1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src1"));
  auto* src2 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src2"));

  LayoutCacheStatus cache_status;
  ConstraintSpace space =
      src1->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test1, space, &cache_status);
  // test1 was sized to its max-content size, passing an available size larger
  // than the fragment should hit the cache.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);

  space = src2->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  result = TestCachedLayoutResult(test2, space, &cache_status);
  // test2 was sized to its min-content size in, passing an available size
  // smaller than the fragment should hit the cache.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, MissShrinkToFit) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flow-root; width: 300px; height: 100px;">
      <div id="test1" style="float: left;">
        <div style="display: inline-block; width: 150px;"></div>
        <div style="display: inline-block; width: 50px;"></div>
      </div>
      <div id="test2" style="float: left;">
        <div style="display: inline-block; width: 350px;"></div>
        <div style="display: inline-block; width: 250px;"></div>
      </div>
      <div id="test3" style="float: left; min-width: 80%;">
        <div style="display: inline-block; width: 150px;"></div>
        <div style="display: inline-block; width: 250px;"></div>
      </div>
      <div id="test4" style="float: left; margin-left: 75px;">
        <div style="display: inline-block; width: 150px;"></div>
        <div style="display: inline-block; width: 50px;"></div>
      </div>
    </div>
    <div style="display: flow-root; width: 100px; height: 100px;">
      <div id="src1" style="float: left;">
        <div style="display: inline-block; width: 150px;"></div>
        <div style="display: inline-block; width: 50px;"></div>
      </div>
    </div>
    <div style="display: flow-root; width: 400px; height: 100px;">
      <div id="src2" style="float: left;">
        <div style="display: inline-block; width: 350px;"></div>
        <div style="display: inline-block; width: 250px;"></div>
      </div>
      <div id="src3" style="float: left; min-width: 80%;">
        <div style="display: inline-block; width: 150px;"></div>
        <div style="display: inline-block; width: 250px;"></div>
      </div>
    </div>
    <div style="display: flow-root; width: 250px; height: 100px;">
      <div id="src4" style="float: left; margin-left: 75px;">
        <div style="display: inline-block; width: 150px;"></div>
        <div style="display: inline-block; width: 50px;"></div>
      </div>
    </div>
  )HTML");

  auto* test1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test1"));
  auto* test2 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test2"));
  auto* test3 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test3"));
  auto* test4 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test4"));
  auto* src1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src1"));
  auto* src2 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src2"));
  auto* src3 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src3"));
  auto* src4 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src4"));

  LayoutCacheStatus cache_status;
  ConstraintSpace space =
      src1->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test1, space, &cache_status);
  // test1 was sized to its max-content size, passing an available size smaller
  // than the fragment should miss the cache.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);

  space = src2->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  result = TestCachedLayoutResult(test2, space, &cache_status);
  // test2 was sized to its min-content size, passing an available size
  // larger than the fragment should miss the cache.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);

  space = src3->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  result = TestCachedLayoutResult(test3, space, &cache_status);
  // test3 was sized to its min-content size, however it should miss the cache
  // as it has a %-min-size.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);

  space = src4->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  result = TestCachedLayoutResult(test4, space, &cache_status);
  // test4 was sized to its max-content size, however it should miss the cache
  // due to its margin.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitShrinkToFitSameIntrinsicSizes) {
  // We have a shrink-to-fit node, with the min, and max intrinsic sizes being
  // equal (the available size doesn't affect the final size).
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .shrink { width: fit-content; }
      .child { width: 250px; }
    </style>
    <div class="bfc">
      <div id="test" class="shrink">
        <div class="child"></div>
      </div>
    </div>
    <div class="bfc" style="width: 200px; height: 200px;">
      <div id="src" class="shrink">
        <div class="child"></div>
      </div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitShrinkToFitDifferentParent) {
  // The parent "bfc" node changes from shrink-to-fit, to a fixed width. But
  // these calculate as the same available space to the "test" element.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; }
      .child { width: 250px; }
    </style>
    <div class="bfc" style="width: fit-content; height: 100px;">
      <div id="test">
        <div class="child"></div>
      </div>
    </div>
    <div class="bfc" style="width: 250px; height: 100px;">
      <div id="src">
        <div class="child"></div>
      </div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, MissQuirksModePercentageBasedChild) {
  // Quirks-mode %-block-size child.
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .child { height: 50%; }
    </style>
    <div class="bfc">
      <div id="test">
        <div class="child"></div>
      </div>
    </div>
    <div class="bfc" style="height: 200px;">
      <div id="src">
        <div class="child"></div>
      </div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitQuirksModePercentageBasedParentAndChild) {
  // Quirks-mode %-block-size parent *and* child. Here we mark the parent as
  // depending on %-block-size changes, however itself doesn't change in
  // height.
  // We are able to hit the cache as we detect that the height for the child
  // *isn't* indefinite, and results in the same height as before.
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .parent { height: 50%; min-height: 200px; }
      .child { height: 50%; }
    </style>
    <div class="bfc">
      <div id="test" class="parent">
        <div class="child"></div>
      </div>
    </div>
    <div class="bfc" style="height: 200px;">
      <div id="src" class="parent">
        <div class="child"></div>
      </div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitStandardsModePercentageBasedChild) {
  // Standards-mode %-block-size child.
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .child { height: 50%; }
    </style>
    <div class="bfc">
      <div id="test">
        <div class="child"></div>
      </div>
    </div>
    <div class="bfc" style="height: 200px;">
      <div id="src">
        <div class="child"></div>
      </div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, ChangeTableCellBlockSizeConstrainedness) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .table { display: table; width: 300px; }
      .cell { display: table-cell; }
      .child1 { height: 100px; }
      .child2, .child3 { overflow:auto; height:10%; }
    </style>
    <div class="table">
      <div class="cell">
        <div class="child1" id="test1"></div>
        <div class="child2" id="test2">
          <div style="height:30px;"></div>
        </div>
        <div class="child3" id="test3"></div>
      </div>
    </div>
    <div class="table" style="height:300px;">
      <div class="cell">
        <div class="child1" id="src1"></div>
        <div class="child2" id="src2">
          <div style="height:30px;"></div>
        </div>
        <div class="child3" id="src3"></div>
      </div>
    </div>
  )HTML");

  auto* test1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test1"));
  auto* test2 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test2"));
  auto* test3 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test3"));
  auto* src1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src1"));
  auto* src2 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src2"));
  auto* src3 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src3"));

  LayoutCacheStatus cache_status;
  ConstraintSpace space =
      src1->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test1, space, &cache_status);
  // The first child has a fixed height, and shouldn't be affected by the cell
  // height.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);

  space = src2->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  result = TestCachedLayoutResult(test2, space, &cache_status);
  // The second child has overflow:auto and a percentage height, but its
  // intrinsic height is identical to its extrinsic height (when the cell has a
  // height). So it won't need layout, either.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);

  space = src3->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  result = TestCachedLayoutResult(test3, space, &cache_status);
  // The third child has overflow:auto and a percentage height, and its
  // intrinsic height is 0 (no children), so it matters whether the cell has a
  // height or not. We're only going to need simplified layout, though, since no
  // children will be affected by its height change.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsSimplifiedLayout);
}

TEST_F(LayoutResultCachingTest, OptimisticFloatPlacementNoRelayout) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .root { display: flow-root; width: 300px; }
      .float { float: left; width: 10px; height: 10px; }
    </style>
    <div class="root">
      <div id="empty">
        <div class="float"></div>
      </div>
    </div>
  )HTML");

  auto* empty = To<LayoutBlockFlow>(GetLayoutObjectByElementId("empty"));

  ConstraintSpace space =
      empty->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();

  // We shouldn't have a "forced" BFC block-offset, as the "empty"
  // self-collapsing block should have its "expected" BFC block-offset at the
  // correct place.
  EXPECT_EQ(space.ForcedBfcBlockOffset(), std::nullopt);
}

TEST_F(LayoutResultCachingTest, SelfCollapsingShifting) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float { float: left; width: 10px; height: 10px; }
      .adjoining-oof { position: absolute; display: inline; }
    </style>
    <div class="bfc">
      <div class="float"></div>
      <div id="test1"></div>
    </div>
    <div class="bfc">
      <div class="float" style="height; 20px;"></div>
      <div id="src1"></div>
    </div>
    <div class="bfc">
      <div class="float"></div>
      <div id="test2">
        <div class="adjoining-oof"></div>
      </div>
    </div>
    <div class="bfc">
      <div class="float" style="height; 20px;"></div>
      <div id="src2">
        <div class="adjoining-oof"></div>
      </div>
    </div>
    <div class="bfc">
      <div class="float"></div>
      <div style="height: 30px;"></div>
      <div id="test3">
        <div class="adjoining-oof"></div>
      </div>
    </div>
    <div class="bfc">
      <div class="float" style="height; 20px;"></div>
      <div style="height: 30px;"></div>
      <div id="src3">
        <div class="adjoining-oof"></div>
      </div>
    </div>
  )HTML");

  auto* test1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test1"));
  auto* test2 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test2"));
  auto* test3 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test3"));
  auto* src1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src1"));
  auto* src2 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src2"));
  auto* src3 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src3"));

  LayoutCacheStatus cache_status;

  ConstraintSpace space =
      src1->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test1, space, &cache_status);

  // Case 1: We have a different set of constraints, but as the child has no
  // adjoining descendants it can be shifted anywhere.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);

  space = src2->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  result = TestCachedLayoutResult(test2, space, &cache_status);

  // Case 2: We have a different set of constraints, but the child has an
  // adjoining object and isn't "past" the floats - it can't be reused.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);

  space = src3->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  result = TestCachedLayoutResult(test3, space, &cache_status);

  // Case 3: We have a different set of constraints, and adjoining descendants,
  // but have a position past where they might affect us.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, ClearancePastAdjoiningFloatsMovement) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
      .float-left { float: left; width: 10px; height: 10px; }
      .float-right { float: right; width: 10px; height: 20px; }
    </style>
    <div class="bfc">
      <div>
        <div class="float-left"></div>
        <div class="float-right"></div>
        <div id="test1" style="clear: both;">text</div>
      </div>
    </div>
    <div class="bfc">
      <div>
        <div class="float-left" style="height; 20px;"></div>
        <div class="float-right"></div>
        <div id="src1" style="clear: both;">text</div>
      </div>
    </div>
    <div class="bfc">
      <div>
        <div class="float-left"></div>
        <div class="float-right"></div>
        <div id="test2" style="clear: left;">text</div>
      </div>
    </div>
    <div class="bfc">
      <div>
        <div class="float-left" style="height; 20px;"></div>
        <div class="float-right"></div>
        <div id="src2" style="clear: left;">text</div>
      </div>
    </div>
  )HTML");

  auto* test1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test1"));
  auto* test2 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test2"));
  auto* src1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src1"));
  auto* src2 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src2"));

  LayoutCacheStatus cache_status;

  ConstraintSpace space =
      src1->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test1, space, &cache_status);

  // Case 1: We have forced clearance, but floats won't impact our children.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);

  space = src2->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  result = TestCachedLayoutResult(test2, space, &cache_status);

  // Case 2: We have forced clearance, and floats will impact our children.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LayoutResultCachingTest, MarginStrutMovementSelfCollapsing) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
    </style>
    <div class="bfc">
      <div style="margin-top: 10px;">
        <div id="test1">
          <div></div>
        </div>
      </div>
    </div>
    <div class="bfc">
      <div style="margin-top: 5px;">
        <div id="src1">
          <div></div>
        </div>
      </div>
    </div>
    <div class="bfc">
      <div style="margin-top: 10px;">
        <div id="test2">
          <div style="margin-bottom: 8px;"></div>
        </div>
      </div>
    </div>
    <div class="bfc">
      <div style="margin-top: 5px;">
        <div id="src2">
          <div style="margin-bottom: 8px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  auto* test1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test1"));
  auto* test2 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test2"));
  auto* src1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src1"));
  auto* src2 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src2"));

  LayoutCacheStatus cache_status;

  ConstraintSpace space =
      src1->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test1, space, &cache_status);

  // Case 1: We can safely re-use this fragment as it doesn't append anything
  // to the margin-strut within the sub-tree.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);

  // The "end" margin-strut should be updated.
  MarginStrut expected_margin_strut;
  expected_margin_strut.Append(LayoutUnit(5), false /* is_quirky */);
  EXPECT_EQ(expected_margin_strut, result->EndMarginStrut());

  space = src2->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  result = TestCachedLayoutResult(test2, space, &cache_status);

  // Case 2: We can't re-use this fragment as it appended a non-zero value to
  // the margin-strut within the sub-tree.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LayoutResultCachingTest, MarginStrutMovementInFlow) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
    </style>
    <div class="bfc">
      <div style="margin-top: 10px;">
        <div id="test1">
          <div>text</div>
        </div>
      </div>
    </div>
    <div class="bfc">
      <div style="margin-top: 5px;">
        <div id="src1">
          <div>text</div>
        </div>
      </div>
    </div>
    <div class="bfc">
      <div style="margin-top: 10px;">
        <div id="test2">
          <div style="margin-top: 8px;">text</div>
        </div>
      </div>
    </div>
    <div class="bfc">
      <div style="margin-top: 5px;">
        <div id="src2">
          <div style="margin-top: 8px;">text</div>
        </div>
      </div>
    </div>
    <div class="bfc">
      <div style="margin-top: 10px;">
        <div id="test3">
          <div>
            <div style="margin-top: 8px;"></div>
          </div>
          <div>text</div>
        </div>
      </div>
    </div>
    <div class="bfc">
      <div style="margin-top: 5px;">
        <div id="src3">
          <div>
            <div style="margin-top: 8px;"></div>
          </div>
          <div>text</div>
        </div>
      </div>
    </div>
  )HTML");

  auto* test1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test1"));
  auto* test2 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test2"));
  auto* test3 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test3"));
  auto* src1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src1"));
  auto* src2 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src2"));
  auto* src3 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src3"));

  LayoutCacheStatus cache_status;

  ConstraintSpace space =
      src1->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test1, space, &cache_status);

  // Case 1: We can safely re-use this fragment as it doesn't append anything
  // to the margin-strut within the sub-tree.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);

  space = src2->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  result = TestCachedLayoutResult(test2, space, &cache_status);

  // Case 2: We can't re-use this fragment as it appended a non-zero value to
  // the margin-strut within the sub-tree.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);

  space = src3->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  result = TestCachedLayoutResult(test3, space, &cache_status);

  // Case 3: We can't re-use this fragment as a (inner) self-collapsing block
  // appended a non-zero value to the margin-strut within the sub-tree.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LayoutResultCachingTest, MarginStrutMovementPercentage) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 300px; height: 300px; }
    </style>
    <div class="bfc">
      <div style="margin-top: 10px;">
        <div id="test1" style="width: 0px;">
          <div style="margin-top: 50%;">text</div>
        </div>
      </div>
    </div>
    <div class="bfc">
      <div style="margin-top: 5px;">
        <div id="src1" style="width: 0px;">
          <div style="margin-top: 50%;">text</div>
        </div>
      </div>
    </div>
  )HTML");

  auto* test1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test1"));
  auto* src1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src1"));

  LayoutCacheStatus cache_status;

  ConstraintSpace space =
      src1->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test1, space, &cache_status);

  // We can't re-use this fragment as it appended a non-zero value (50%) to the
  // margin-strut within the sub-tree.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitIsFixedBlockSizeIndefinite) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; width: 100px; height: 100px;">
      <div id="test1" style="flex-grow: 1; min-height: 100px;">
        <div style="height: 50px;">text</div>
      </div>
    </div>
    <div style="display: flex; width: 100px; height: 100px; align-items: stretch;">
      <div id="src1" style="flex-grow: 1; min-height: 100px;">
        <div style="height: 50px;">text</div>
      </div>
    </div>
  )HTML");

  auto* test1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test1"));
  auto* src1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src1"));

  LayoutCacheStatus cache_status;

  ConstraintSpace space =
      src1->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test1, space, &cache_status);

  // Even though the "align-items: stretch" will make the final fixed
  // block-size indefinite, we don't have any %-block-size children, so we can
  // hit the cache.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, MissIsFixedBlockSizeIndefinite) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div style="display: flex; width: 100px; height: 100px; align-items: start;">
      <div id="src1" style="flex-grow: 1; min-height: 100px;">
        <div style="height: 50%;">text</div>
      </div>
    </div>
    <div style="display: flex; width: 100px; height: 100px; align-items: stretch;">
      <div id="test1" style="flex-grow: 1; min-height: 100px;">
        <div style="height: 50%;">text</div>
      </div>
    </div>
  )HTML");

  auto* test1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test1"));
  auto* src1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src1"));

  LayoutCacheStatus cache_status;

  ConstraintSpace space =
      src1->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test1, space, &cache_status);

  // The "align-items: stretch" will make the final fixed block-size
  // indefinite, and we have a %-block-size child, so we need to miss the
  // cache.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitColumnFlexBoxMeasureAndLayout) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      .bfc { display: flex; flex-direction: column; width: 100px; height: 100px; }
    </style>
    <div class="bfc">
      <div id="src1" style="flex-grow: 0;">
        <div style="height: 50px;"></div>
      </div>
    </div>
    <div class="bfc">
      <div id="src2" style="flex-grow: 1;">
        <div style="height: 50px;"></div>
      </div>
    </div>
    <div class="bfc">
      <div id="test1" style="flex-grow: 2;">
        <div style="height: 50px;"></div>
      </div>
    </div>
  )HTML");

  auto* test1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test1"));
  auto* src1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src1"));
  auto* src2 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src2"));

  LayoutCacheStatus cache_status;

  // "src1" only had one "measure" pass performed, and should hit the "measure"
  // cache-slot for "test1".
  ConstraintSpace space =
      src1->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test1, space, &cache_status);

  EXPECT_EQ(space.CacheSlot(), LayoutResultCacheSlot::kMeasure);
  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);

  // "src2" had both a "measure" and "layout" pass performed, and should hit
  // the "layout" cache-slot for "test1".
  space = src2->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  result = TestCachedLayoutResult(test1, space, &cache_status);

  EXPECT_EQ(space.CacheSlot(), LayoutResultCacheSlot::kLayout);
  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitRowFlexBoxMeasureAndLayout) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      .bfc { display: flex; width: 100px; }
    </style>
    <div class="bfc">
      <div id="src1">
        <div style="height: 50px;"></div>
      </div>
    </div>
    <div class="bfc">
      <div id="src2">
        <div style="height: 70px;"></div>
      </div>
      <div style="width: 0px; height: 100px;"></div>
    </div>
    <div class="bfc">
      <div id="test1">
        <div style="height: 50px;"></div>
      </div>
      <div style="width: 0px; height: 100px;"></div>
    </div>
  )HTML");

  auto* test1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test1"));
  auto* src1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src1"));
  auto* src2 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src2"));

  LayoutCacheStatus cache_status;

  // "src1" only had one "measure" pass performed, and should hit the "measure"
  // cache-slot for "test1".
  ConstraintSpace space =
      src1->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test1, space, &cache_status);

  EXPECT_EQ(space.CacheSlot(), LayoutResultCacheSlot::kMeasure);
  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);

  // "src2" had both a "measure" and "layout" pass performed, and should hit
  // the "layout" cache-slot for "test1".
  space = src2->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  result = TestCachedLayoutResult(test1, space, &cache_status);

  EXPECT_EQ(space.CacheSlot(), LayoutResultCacheSlot::kLayout);
  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitFlexLegacyImg) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flex; flex-direction: column; width: 300px; }
      .bfc > * { display: flex; }
    </style>
    <div class="bfc">
      <div id="test">
        <img />
      </div>
    </div>
    <div class="bfc" style="height: 200px;">
      <div id="src">
        <img />
      </div>
    </div>
  )HTML");

  auto* test = To<LayoutBlock>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlock>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitFlexLegacyGrid) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flex; flex-direction: column; width: 300px; }
      .bfc > * { display: flex; }
      .grid { display: grid; }
    </style>
    <div class="bfc">
      <div id="test">
        <div class="grid"></div>
      </div>
    </div>
    <div class="bfc" style="height: 200px;">
      <div id="src">
        <div class="grid"></div>
      </div>
    </div>
  )HTML");

  auto* test = To<LayoutBlock>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlock>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitFlexDefiniteChange) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex; flex-direction: column;">
      <div style="height: 200px;" id=target1>
        <div style="height: 100px"></div>
      </div>
    </div>
  )HTML");

  auto* target1 = To<LayoutBlock>(GetLayoutObjectByElementId("target1"));

  const LayoutResult* result1 = target1->GetSingleCachedLayoutResult();
  const LayoutResult* measure1 =
      target1->GetSingleCachedMeasureResultForTesting();
  EXPECT_EQ(measure1->IntrinsicBlockSize(), 100);
  EXPECT_EQ(result1->GetPhysicalFragment().Size().height, 200);

  EXPECT_EQ(result1->GetConstraintSpaceForCaching().CacheSlot(),
            LayoutResultCacheSlot::kMeasure);
  EXPECT_EQ(result1, measure1);
}

TEST_F(LayoutResultCachingTest, HitOrthogonalRoot) {
  SetBodyInnerHTML(R"HTML(
    <style>
      span { display: inline-block; width: 20px; height: 250px }
    </style>
    <div id="target" style="display: flex;">
      <div style="writing-mode: vertical-rl; line-height: 0;">
        <span></span><span></span>
      </div>
    </div>
  )HTML");

  auto* target = To<LayoutBlock>(GetLayoutObjectByElementId("target"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      target->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(target, space, &cache_status);

  // We should hit the cache using the same constraint space.
  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, SimpleTable) {
  SetBodyInnerHTML(R"HTML(
    <table>
      <td id="target1">abc</td>
      <td id="target2">abc</td>
    </table>
  )HTML");

  auto* target1 = To<LayoutBlock>(GetLayoutObjectByElementId("target1"));
  auto* target2 = To<LayoutBlock>(GetLayoutObjectByElementId("target2"));

  // Both "target1", and "target1" should have  only had one "measure" pass
  // performed.
  const LayoutResult* result1 = target1->GetSingleCachedLayoutResult();
  const LayoutResult* measure1 =
      target1->GetSingleCachedMeasureResultForTesting();
  EXPECT_EQ(result1->GetConstraintSpaceForCaching().CacheSlot(),
            LayoutResultCacheSlot::kMeasure);
  EXPECT_NE(result1, nullptr);
  EXPECT_EQ(result1, measure1);

  const LayoutResult* result2 = target2->GetSingleCachedLayoutResult();
  const LayoutResult* measure2 =
      target2->GetSingleCachedMeasureResultForTesting();
  EXPECT_EQ(result2->GetConstraintSpaceForCaching().CacheSlot(),
            LayoutResultCacheSlot::kMeasure);
  EXPECT_NE(result2, nullptr);
  EXPECT_EQ(result2, measure2);
}

TEST_F(LayoutResultCachingTest, MissTableCellMiddleAlignment) {
  SetBodyInnerHTML(R"HTML(
    <table>
      <td id="target" style="vertical-align: middle;">abc</td>
      <td>abc<br>abc</td>
    </table>
  )HTML");

  auto* target = To<LayoutBlock>(GetLayoutObjectByElementId("target"));

  // "target" should be stretched, and miss the measure cache.
  const LayoutResult* result = target->GetSingleCachedLayoutResult();
  const LayoutResult* measure =
      target->GetSingleCachedMeasureResultForTesting();
  EXPECT_NE(measure, nullptr);
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(measure->GetConstraintSpaceForCaching().CacheSlot(),
            LayoutResultCacheSlot::kMeasure);
  EXPECT_EQ(result->GetConstraintSpaceForCaching().CacheSlot(),
            LayoutResultCacheSlot::kLayout);
  EXPECT_NE(result, measure);
}

TEST_F(LayoutResultCachingTest, MissTableCellBottomAlignment) {
  SetBodyInnerHTML(R"HTML(
    <table>
      <td id="target" style="vertical-align: bottom;">abc</td>
      <td>abc<br>abc</td>
    </table>
  )HTML");

  auto* target = To<LayoutBlock>(GetLayoutObjectByElementId("target"));

  // "target" should be stretched, and miss the measure cache.
  const LayoutResult* result = target->GetSingleCachedLayoutResult();
  const LayoutResult* measure =
      target->GetSingleCachedMeasureResultForTesting();
  EXPECT_NE(measure, nullptr);
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(measure->GetConstraintSpaceForCaching().CacheSlot(),
            LayoutResultCacheSlot::kMeasure);
  EXPECT_EQ(result->GetConstraintSpaceForCaching().CacheSlot(),
            LayoutResultCacheSlot::kLayout);
  EXPECT_NE(result, measure);
}

TEST_F(LayoutResultCachingTest, HitTableCellBaselineAlignment) {
  SetBodyInnerHTML(R"HTML(
    <style>
      td { vertical-align: baseline; }
    </style>
    <table>
      <td id="target">abc</td>
      <td>def</td>
    </table>
  )HTML");

  auto* target = To<LayoutBlock>(GetLayoutObjectByElementId("target"));

  // "target" should align to the baseline, but hit the cache.
  const LayoutResult* result = target->GetSingleCachedLayoutResult();
  const LayoutResult* measure =
      target->GetSingleCachedMeasureResultForTesting();
  EXPECT_EQ(result->GetConstraintSpaceForCaching().CacheSlot(),
            LayoutResultCacheSlot::kMeasure);
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result, measure);
}

TEST_F(LayoutResultCachingTest, MissTableCellBaselineAlignment) {
  SetBodyInnerHTML(R"HTML(
    <style>
      td { vertical-align: baseline; }
    </style>
    <table>
      <td id="target">abc</td>
      <td><span style="font-size: 32px">def</span></td>
    </table>
  )HTML");

  auto* target = To<LayoutBlock>(GetLayoutObjectByElementId("target"));

  // "target" should align to the baseline, but miss the cache.
  const LayoutResult* result = target->GetSingleCachedLayoutResult();
  const LayoutResult* measure =
      target->GetSingleCachedMeasureResultForTesting();
  EXPECT_NE(measure, nullptr);
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(measure->GetConstraintSpaceForCaching().CacheSlot(),
            LayoutResultCacheSlot::kMeasure);
  EXPECT_EQ(result->GetConstraintSpaceForCaching().CacheSlot(),
            LayoutResultCacheSlot::kLayout);
  EXPECT_NE(result, measure);
}

TEST_F(LayoutResultCachingTest, MissTablePercent) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .bfc { display: flow-root; width: 100px; }
      table { height: 100%; }
      caption { height: 50px; }
    </style>
    <div class="bfc" style="height: 50px;">
      <table id="test">
        <caption></caption>
        <td></td>
      </table>
    </div>
    <div class="bfc" style="height: 100px;">
      <table id="src">
        <caption></caption>
        <td></td>
      </table>
    </div>
  )HTML");

  auto* test = To<LayoutBlock>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlock>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitTableRowAdd) {
  SetBodyInnerHTML(R"HTML(
    <table>
      <tr><td>a</td><td>b</td></tr>
      <tr id="test"><td>text</td><td>more text</td></tr>
    </table>
    <table>
      <tr id="src"><td>text</td><td>more text</td></tr>
    </table>
  )HTML");

  auto* test = To<LayoutBlock>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlock>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, MissTableRowAdd) {
  SetBodyInnerHTML(R"HTML(
    <table>
      <tr><td>longwordhere</td><td>b</td></tr>
      <tr id="test"><td>text</td><td>more text</td></tr>
    </table>
    <table>
      <tr id="src"><td>text</td><td>more text</td></tr>
    </table>
  )HTML");

  auto* test = To<LayoutBlock>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlock>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitTableRowRemove) {
  SetBodyInnerHTML(R"HTML(
    <table>
      <tr id="test"><td>text</td><td>more text</td></tr>
    </table>
    <table>
      <tr><td>a</td><td>b</td></tr>
      <tr id="src"><td>text</td><td>more text</td></tr>
    </table>
  )HTML");

  auto* test = To<LayoutBlock>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlock>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, MissTableRowRemove) {
  SetBodyInnerHTML(R"HTML(
    <table>
      <tr id="test"><td>text</td><td>more text</td></tr>
    </table>
    <table>
      <tr><td>longwordhere</td><td>b</td></tr>
      <tr id="src"><td>text</td><td>more text</td></tr>
    </table>
  )HTML");

  auto* test = To<LayoutBlock>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlock>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitTableSectionAdd) {
  SetBodyInnerHTML(R"HTML(
    <table>
      <tbody><tr><td>a</td><td>b</td></tr></tbody>
      <tbody id="test"><tr><td>text</td><td>more text</td></tr></tbody>
    </table>
    <table>
      <tbody id="src"><tr><td>text</td><td>more text</td></tr></tbody>
    </table>
  )HTML");

  auto* test = To<LayoutBlock>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlock>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitTableSectionRemove) {
  SetBodyInnerHTML(R"HTML(
    <table>
      <tbody id="test"><tr><td>text</td><td>more text</td></tr></tbody>
    </table>
    <table>
      <tbody><tr><td>a</td><td>b</td></tr></tbody>
      <tbody id="src"><tr><td>text</td><td>more text</td></tr></tbody>
    </table>
  )HTML");

  auto* test = To<LayoutBlock>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlock>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, FragmentainerSizeChange) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .multicol { columns:2; column-fill:auto; }
      .child { height:120px; }
    </style>
    <div class="multicol" style="height:50px;">
      <div id="test" class="child"></div>
    </div>
    <div class="multicol" style="height:51px;">
      <div id="src" class="child"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  const LayoutResult* test_result1 = test->GetCachedLayoutResult(nullptr);
  ASSERT_TRUE(test_result1);
  const ConstraintSpace& test_space1 =
      test_result1->GetConstraintSpaceForCaching();
  const auto* test_break_token1 =
      To<BlockBreakToken>(test_result1->GetPhysicalFragment().GetBreakToken());
  ASSERT_TRUE(test_break_token1);
  const LayoutResult* test_result2 =
      test->GetCachedLayoutResult(test_break_token1);
  ASSERT_TRUE(test_result2);
  const ConstraintSpace& test_space2 =
      test_result2->GetConstraintSpaceForCaching();
  const auto* test_break_token2 =
      To<BlockBreakToken>(test_result2->GetPhysicalFragment().GetBreakToken());
  ASSERT_TRUE(test_break_token2);
  const LayoutResult* test_result3 =
      test->GetCachedLayoutResult(test_break_token2);
  ASSERT_TRUE(test_result3);
  const ConstraintSpace& test_space3 =
      test_result3->GetConstraintSpaceForCaching();
  EXPECT_FALSE(test_result3->GetPhysicalFragment().GetBreakToken());

  const LayoutResult* src_result1 = src->GetCachedLayoutResult(nullptr);
  ASSERT_TRUE(src_result1);
  const ConstraintSpace& src_space1 =
      src_result1->GetConstraintSpaceForCaching();
  const auto* src_break_token1 =
      To<BlockBreakToken>(src_result1->GetPhysicalFragment().GetBreakToken());
  ASSERT_TRUE(src_break_token1);
  const LayoutResult* src_result2 =
      src->GetCachedLayoutResult(src_break_token1);
  ASSERT_TRUE(src_result2);
  const ConstraintSpace& src_space2 =
      src_result2->GetConstraintSpaceForCaching();
  const auto* src_break_token2 =
      To<BlockBreakToken>(src_result2->GetPhysicalFragment().GetBreakToken());
  ASSERT_TRUE(src_break_token2);
  const LayoutResult* src_result3 =
      src->GetCachedLayoutResult(src_break_token2);
  ASSERT_TRUE(src_result3);
  const ConstraintSpace& src_space3 =
      src_result3->GetConstraintSpaceForCaching();
  EXPECT_FALSE(src_result3->GetPhysicalFragment().GetBreakToken());

  // If the extrinsic constraints are unchanged, hit the cache, even if
  // fragmented:
  EXPECT_TRUE(TestCachedLayoutResultWithBreakToken(src, src_space1, nullptr));
  EXPECT_TRUE(
      TestCachedLayoutResultWithBreakToken(src, src_space2, src_break_token1));
  EXPECT_TRUE(
      TestCachedLayoutResultWithBreakToken(src, src_space3, src_break_token2));

  // If the fragmentainer size changes, though, miss the cache:
  EXPECT_FALSE(TestCachedLayoutResultWithBreakToken(src, test_space1, nullptr));
  EXPECT_FALSE(TestCachedLayoutResultWithBreakToken(src, test_space2,
                                                    test_break_token1));
  EXPECT_FALSE(TestCachedLayoutResultWithBreakToken(src, test_space3,
                                                    test_break_token2));
}

TEST_F(LayoutResultCachingTest, BlockOffsetChangeInFragmentainer) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .multicol { columns:2; column-fill:auto; height:100px; }
      .second { height:80px; }
    </style>
    <div class="multicol">
      <div style="height:19px;"></div>
      <div id="test1" class="second"></div>
    </div>
    <div class="multicol">
      <div style="height:20px;"></div>
      <div id="test2" class="second"></div>
    </div>
    <div class="multicol">
      <div style="height:21px;"></div>
      <div id="test3" class="second"></div>
    </div>
    <div class="multicol">
      <div style="height:10px;"></div>
      <div id="src" class="second"></div>
    </div>
  )HTML");

  auto* test1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test1"));
  auto* test2 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test2"));
  auto* test3 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test3"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  const ConstraintSpace& test1_space =
      test1->GetCachedLayoutResult(nullptr)->GetConstraintSpaceForCaching();
  const ConstraintSpace& test2_space =
      test2->GetCachedLayoutResult(nullptr)->GetConstraintSpaceForCaching();
  const ConstraintSpace& test3_space =
      test3->GetCachedLayoutResult(nullptr)->GetConstraintSpaceForCaching();

  // The element is one pixel above the fragmentation line. Still unbroken. We
  // can hit the cache.
  EXPECT_TRUE(TestCachedLayoutResult(src, test1_space));

  // The element ends exactly at the fragmentation line. Still unbroken. We can
  // hit the cache.
  EXPECT_TRUE(TestCachedLayoutResult(src, test2_space));

  // The element crosses the fragmentation line by one pixel, so it needs to
  // break. We need to miss the cache.
  EXPECT_FALSE(TestCachedLayoutResult(src, test3_space));
}

TEST_F(LayoutResultCachingTest, BfcRootBlockOffsetChangeInFragmentainer) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .multicol { columns:2; column-fill:auto; height:100px; }
      .second { display: flow-root; height:80px; }
    </style>
    <div class="multicol">
      <div style="height:19px;"></div>
      <div id="test1" class="second"></div>
    </div>
    <div class="multicol">
      <div style="height:20px;"></div>
      <div id="test2" class="second"></div>
    </div>
    <div class="multicol">
      <div style="height:21px;"></div>
      <div id="test3" class="second"></div>
    </div>
    <div class="multicol">
      <div style="height:10px;"></div>
      <div id="src" class="second"></div>
    </div>
  )HTML");

  auto* test1 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test1"));
  auto* test2 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test2"));
  auto* test3 = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test3"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  const ConstraintSpace& test1_space =
      test1->GetCachedLayoutResult(nullptr)->GetConstraintSpaceForCaching();
  const ConstraintSpace& test2_space =
      test2->GetCachedLayoutResult(nullptr)->GetConstraintSpaceForCaching();
  const ConstraintSpace& test3_space =
      test3->GetCachedLayoutResult(nullptr)->GetConstraintSpaceForCaching();

  // The element is one pixel above the fragmentation line. Still unbroken. We
  // can hit the cache.
  EXPECT_TRUE(TestCachedLayoutResult(src, test1_space));

  // The element ends exactly at the fragmentation line. Still unbroken. We can
  // hit the cache.
  EXPECT_TRUE(TestCachedLayoutResult(src, test2_space));

  // The element crosses the fragmentation line by one pixel, so it needs to
  // break. We need to miss the cache.
  EXPECT_FALSE(TestCachedLayoutResult(src, test3_space));
}

TEST_F(LayoutResultCachingTest, HitBlockOffsetUnchangedInFragmentainer) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .multicol { columns:2; column-fill:auto; height:100px; }
      .third { height:50px; }
    </style>
    <div class="multicol">
      <div height="10px;"></div>
      <div height="20px;"></div>
      <div id="test" class="third"></div>
    </div>
    <div class="multicol">
      <div height="20px;"></div>
      <div height="10px;"></div>
      <div id="src" class="third"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  ASSERT_NE(src->GetSingleCachedLayoutResult(), nullptr);
  ASSERT_NE(test->GetSingleCachedLayoutResult(), nullptr);
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, HitNewFormattingContextInFragmentainer) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .multicol { columns:2; }
      .newfc { display: flow-root; height:50px; }
    </style>
    <div class="multicol">
      <div id="test" class="newfc"></div>
      <div style="height: 100px;"></div>
    </div>
    <div class="multicol">
      <div id="src" class="newfc"></div>
      <div style="height: 90px;"></div>
    </div>
  )HTML");

  auto* test = To<LayoutBlock>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlock>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  ASSERT_NE(src->GetSingleCachedLayoutResult(), nullptr);
  ASSERT_NE(test->GetSingleCachedLayoutResult(), nullptr);
  const ConstraintSpace& space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  EXPECT_TRUE(space.IsInitialColumnBalancingPass());
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kHit);
  EXPECT_NE(result, nullptr);
}

TEST_F(LayoutResultCachingTest, MissMonolithicChangeInFragmentainer) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .multicol { columns:2; column-fill:auto; height:100px; }
      .container { height:150px; }
      .child { height:150px; }
    </style>
    <div class="multicol">
      <div class="container">
        <div id="test" class="child"></div>
      </div>
    </div>
    <div class="multicol">
      <div class="container" style="contain:size;">
        <div id="src" class="child"></div>
      </div>
    </div>
  )HTML");

  auto* test = To<LayoutBlockFlow>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlockFlow>(GetLayoutObjectByElementId("src"));
  const ConstraintSpace& src_space =
      src->GetCachedLayoutResult(nullptr)->GetConstraintSpaceForCaching();
  const ConstraintSpace& test_space =
      test->GetCachedLayoutResult(nullptr)->GetConstraintSpaceForCaching();

  EXPECT_FALSE(TestCachedLayoutResult(src, test_space));
  EXPECT_FALSE(TestCachedLayoutResult(test, src_space));
}

TEST_F(LayoutResultCachingTest, MissGridIncorrectIntrinsicSize) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div style="display: flex; width: 100px; height: 200px; align-items: stretch;">
      <div id="test" style="flex-grow: 1; min-height: 100px; display: grid;">
        <div></div>
      </div>
    </div>
    <div style="display: flex; width: 100px; height: 200px; align-items: start;">
      <div id="src" style="flex-grow: 1; min-height: 100px; display: grid;">
        <div></div>
      </div>
    </div>
  )HTML");

  auto* test = To<LayoutBlock>(GetLayoutObjectByElementId("test"));
  auto* src = To<LayoutBlock>(GetLayoutObjectByElementId("src"));

  LayoutCacheStatus cache_status;
  ConstraintSpace space =
      src->GetSingleCachedLayoutResult()->GetConstraintSpaceForCaching();
  const LayoutResult* result =
      TestCachedLayoutResult(test, space, &cache_status);

  EXPECT_EQ(cache_status, LayoutCacheStatus::kNeedsLayout);
  EXPECT_EQ(result, nullptr);
}

}  // namespace
}  // namespace blink
