// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_anchor_query.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {
namespace {

class NGAnchorQueryTest : public RenderingTest,
                          private ScopedCSSAnchorPositioningForTest {
 public:
  NGAnchorQueryTest() : ScopedCSSAnchorPositioningForTest(true) {}

  const NGPhysicalAnchorQuery* AnchorQuery(const Element& element) const {
    const LayoutBlockFlow* container =
        To<LayoutBlockFlow>(element.GetLayoutObject());
    if (!container->PhysicalFragmentCount())
      return nullptr;
    const NGPhysicalBoxFragment* fragment = container->GetPhysicalFragment(0);
    DCHECK(fragment);
    return fragment->AnchorQuery();
  }

  const NGPhysicalAnchorQuery* AnchorQueryByElementId(const char* id) const {
    if (const Element* element = GetElementById(id))
      return AnchorQuery(*element);
    return nullptr;
  }
};

struct AnchorTestData {
  static Vector<AnchorTestData> ToList(
      const NGPhysicalAnchorQuery& anchor_query) {
    Vector<AnchorTestData> items;
    for (auto entry : anchor_query) {
      if (auto** name = absl::get_if<const ScopedCSSName*>(&entry.key)) {
        items.push_back(AnchorTestData{(*name)->GetName(), entry.value->rect});
      }
    }
    std::sort(items.begin(), items.end(),
              [](const AnchorTestData& a, const AnchorTestData& b) {
                return CodeUnitCompare(a.name, b.name) < 0;
              });
    return items;
  }
  bool operator==(const AnchorTestData& other) const {
    return name == other.name && rect == other.rect;
  }

  AtomicString name;
  PhysicalRect rect;
};

std::ostream& operator<<(std::ostream& os, const AnchorTestData& value) {
  return os << value.name << ": " << value.rect;
}

TEST_F(NGAnchorQueryTest, AnchorNameAdd) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      width: 800px;
    }
    #div1 {
      width: 50px;
      height: 20px;
    }
    .after #div1 {
      anchor-name: --div1a;
    }
    </style>
    <div id="container">
      <div id="div1"></div>
    </div>
  )HTML");
  Element* container = GetElementById("container");
  const NGPhysicalAnchorQuery* anchor_query = AnchorQuery(*container);
  EXPECT_FALSE(anchor_query);

  // Add the "after" class and test anchors are updated accordingly.
  container->classList().Add(AtomicString("after"));
  UpdateAllLifecyclePhasesForTest();
  anchor_query = AnchorQuery(*container);
  ASSERT_NE(anchor_query, nullptr);
  EXPECT_THAT(AnchorTestData::ToList(*anchor_query),
              testing::ElementsAre(AnchorTestData{AtomicString("--div1a"),
                                                  PhysicalRect(0, 0, 50, 20)}));
}

TEST_F(NGAnchorQueryTest, AnchorNameChange) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      width: 800px;
    }
    #div1 {
      anchor-name: --div1;
      width: 50px;
      height: 20px;
    }
    .after #div1 {
      anchor-name: --div1a;
    }
    </style>
    <div id="container">
      <div id="div1"></div>
    </div>
  )HTML");
  Element* container = GetElementById("container");
  const NGPhysicalAnchorQuery* anchor_query = AnchorQuery(*container);
  ASSERT_NE(anchor_query, nullptr);
  EXPECT_THAT(AnchorTestData::ToList(*anchor_query),
              testing::ElementsAre(AnchorTestData{AtomicString("--div1"),
                                                  PhysicalRect(0, 0, 50, 20)}));

  // Add the "after" class and test anchors are updated accordingly.
  container->classList().Add(AtomicString("after"));
  UpdateAllLifecyclePhasesForTest();
  anchor_query = AnchorQuery(*container);
  ASSERT_NE(anchor_query, nullptr);
  EXPECT_THAT(AnchorTestData::ToList(*anchor_query),
              testing::ElementsAre(AnchorTestData{AtomicString("--div1a"),
                                                  PhysicalRect(0, 0, 50, 20)}));
}

TEST_F(NGAnchorQueryTest, AnchorNameRemove) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      width: 800px;
    }
    #div1 {
      anchor-name: --div1;
      width: 50px;
      height: 20px;
    }
    .after #div1 {
      anchor-name: none;
    }
    </style>
    <div id="container">
      <div id="div1"></div>
    </div>
  )HTML");
  Element* container = GetElementById("container");
  const NGPhysicalAnchorQuery* anchor_query = AnchorQuery(*container);
  ASSERT_NE(anchor_query, nullptr);
  EXPECT_THAT(AnchorTestData::ToList(*anchor_query),
              testing::ElementsAre(AnchorTestData{AtomicString("--div1"),
                                                  PhysicalRect(0, 0, 50, 20)}));

  // Add the "after" class and test anchors are updated accordingly.
  container->classList().Add(AtomicString("after"));
  UpdateAllLifecyclePhasesForTest();
  anchor_query = AnchorQuery(*container);
  EXPECT_FALSE(anchor_query);
}

TEST_F(NGAnchorQueryTest, BlockFlow) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      width: 800px;
    }
    #div1 {
      height: 20px;
    }
    .after #div1 {
      height: 40px;
    }
    </style>
    <div id="container">
      <div id="div1" style="anchor-name: --div1; width: 400px"></div>
      <div style="anchor-name: --div2"></div>
      <div>
        <div style="height: 30px"></div> <!-- spacer -->
        <div style="anchor-name: --div3"></div>
      </div>
    </div>
  )HTML");
  Element* container = GetElementById("container");
  const NGPhysicalAnchorQuery* anchor_query = AnchorQuery(*container);
  ASSERT_NE(anchor_query, nullptr);
  EXPECT_THAT(
      AnchorTestData::ToList(*anchor_query),
      testing::UnorderedElementsAre(
          AnchorTestData{AtomicString("--div1"), PhysicalRect(0, 0, 400, 20)},
          AnchorTestData{AtomicString("--div2"), PhysicalRect(0, 20, 800, 0)},
          AnchorTestData{AtomicString("--div3"), PhysicalRect(0, 50, 800, 0)}));

  // Add the "after" class and test anchors are updated accordingly.
  container->classList().Add(AtomicString("after"));
  UpdateAllLifecyclePhasesForTest();
  anchor_query = AnchorQuery(*container);
  ASSERT_NE(anchor_query, nullptr);
  EXPECT_THAT(
      AnchorTestData::ToList(*anchor_query),
      testing::UnorderedElementsAre(
          AnchorTestData{AtomicString("--div1"), PhysicalRect(0, 0, 400, 40)},
          AnchorTestData{AtomicString("--div2"), PhysicalRect(0, 40, 800, 0)},
          AnchorTestData{AtomicString("--div3"), PhysicalRect(0, 70, 800, 0)}));
}

TEST_F(NGAnchorQueryTest, Inline) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      width: 800px;
      font-family: Ahem;
      font-size: 10px;
      line-height: 1;
    }
    img {
      width: 10px;
      height: 8px;
    }
    .after .add {
      anchor-name: --add;
    }
    </style>
    <div id="container">
      0
      <!-- culled and non-culled inline boxes. -->
      <span style="anchor-name: --culled">23</span>
      <span style="anchor-name: --non-culled; background: yellow">56</span>

      <!-- Adding `anchor-name` dynamically should uncull. -->
      <span class="add">89</span>

      <!-- Atomic inlines: replaced elements and inline blocks. -->
      <img style="anchor-name: --img" src="data:image/gif;base64,R0lGODlhAQABAAAAACw=">
      <span style="anchor-name: --inline-block; display: inline-block">X</span>
    </div>
  )HTML");
  Element* container = GetElementById("container");
  const NGPhysicalAnchorQuery* anchor_query = AnchorQuery(*container);
  ASSERT_NE(anchor_query, nullptr);
  EXPECT_THAT(
      AnchorTestData::ToList(*anchor_query),
      testing::UnorderedElementsAre(
          AnchorTestData{AtomicString("--culled"), PhysicalRect(20, 0, 20, 10)},
          AnchorTestData{AtomicString("--img"), PhysicalRect(110, 0, 10, 8)},
          AnchorTestData{AtomicString("--inline-block"),
                         PhysicalRect(130, 0, 10, 10)},
          AnchorTestData{AtomicString("--non-culled"),
                         PhysicalRect(50, 0, 20, 10)}));

  // Add the "after" class and test anchors are updated accordingly.
  container->classList().Add(AtomicString("after"));
  UpdateAllLifecyclePhasesForTest();
  anchor_query = AnchorQuery(*container);
  ASSERT_NE(anchor_query, nullptr);
  EXPECT_THAT(
      AnchorTestData::ToList(*anchor_query),
      testing::UnorderedElementsAre(
          AnchorTestData{AtomicString("--add"), PhysicalRect(80, 0, 20, 10)},
          AnchorTestData{AtomicString("--culled"), PhysicalRect(20, 0, 20, 10)},
          AnchorTestData{AtomicString("--img"), PhysicalRect(110, 0, 10, 8)},
          AnchorTestData{AtomicString("--inline-block"),
                         PhysicalRect(130, 0, 10, 10)},
          AnchorTestData{AtomicString("--non-culled"),
                         PhysicalRect(50, 0, 20, 10)}));
}

TEST_F(NGAnchorQueryTest, OutOfFlow) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      width: 800px;
    }
    </style>
    <div id="container" style="position: relative">
      <div id="middle">
        <div style="anchor-name: --abs1; position: absolute; left: 100px; top: 50px; width: 400px; height: 20px"></div>
      </div>
    </div>
  )HTML");
  const NGPhysicalAnchorQuery* anchor_query =
      AnchorQueryByElementId("container");
  ASSERT_NE(anchor_query, nullptr);
  EXPECT_THAT(AnchorTestData::ToList(*anchor_query),
              testing::ElementsAre(AnchorTestData{
                  AtomicString("--abs1"), PhysicalRect(100, 50, 400, 20)}));

  // Anchor names of out-of-flow positioned objects are propagated to their
  // containing blocks.
  EXPECT_EQ(AnchorQueryByElementId("middle"), nullptr);
}

// Relative-positioning should shift the rectangles.
TEST_F(NGAnchorQueryTest, Relative) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      width: 800px;
    }
    </style>
    <div id="container">
      <div style="anchor-name: --relpos; position: relative; left: 20px; top: 10px"></div>
    </div>
  )HTML");
  const NGPhysicalAnchorQuery* anchor_query =
      AnchorQueryByElementId("container");
  ASSERT_NE(anchor_query, nullptr);
  EXPECT_THAT(AnchorTestData::ToList(*anchor_query),
              testing::ElementsAre(AnchorTestData{
                  AtomicString("--relpos"), PhysicalRect(20, 10, 800, 0)}));
}

// CSS Transform should not shift the rectangles.
TEST_F(NGAnchorQueryTest, Transform) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      width: 800px;
    }
    </style>
    <div id="container">
      <div style="anchor-name: --transform; transform: translate(100px, 100px)"></div>
    </div>
  )HTML");
  const NGPhysicalAnchorQuery* anchor_query =
      AnchorQueryByElementId("container");
  ASSERT_NE(anchor_query, nullptr);
  EXPECT_THAT(AnchorTestData::ToList(*anchor_query),
              testing::ElementsAre(AnchorTestData{AtomicString("--transform"),
                                                  PhysicalRect(0, 0, 800, 0)}));
}

// Scroll positions should not shift the rectangles.
TEST_F(NGAnchorQueryTest, Scroll) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      width: 800px;
    }
    </style>
    <div id="container" style="overflow: scroll; width: 200px; height: 200px">
      <div style="anchor-name: --inner; width: 400px; height: 500px"></div>
    </div>
  )HTML");
  Element* container = GetElementById("container");
  ASSERT_NE(container, nullptr);
  container->scrollTo(30, 20);
  UpdateAllLifecyclePhasesForTest();

  const NGPhysicalAnchorQuery* anchor_query = AnchorQuery(*container);
  ASSERT_NE(anchor_query, nullptr);
  EXPECT_THAT(AnchorTestData::ToList(*anchor_query),
              testing::ElementsAre(AnchorTestData{
                  AtomicString("--inner"), PhysicalRect(0, 0, 400, 500)}));
}

TEST_F(NGAnchorQueryTest, FragmentedContainingBlock) {
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      width: 800px;
    }
    #cb {
      position: relative;
    }
    #columns {
      column-count: 3;
      column-fill: auto;
      column-gap: 10px;
      column-width: 100px;
      width: 320px;
      height: 100px;
    }
    </style>
    <div id="container">
      <div style="height: 10px"></div>
      <div id="columns">
        <div style="height: 10px"></div>
        <div id="cb">
          <div style="height: 140px"></div>
          <!-- This anchor box starts at the middle of the 2nd column. -->
          <div style="anchor-name: --a1; width: 100px; height: 100px"></div>
        </div>
      </div>
    </div>
  )HTML");
  auto* cb = To<LayoutBox>(GetLayoutObjectByElementId("cb"));
  ASSERT_EQ(cb->PhysicalFragmentCount(), 3u);
  const NGPhysicalBoxFragment* cb_fragment1 = cb->GetPhysicalFragment(1);
  const NGPhysicalAnchorQuery* cb_anchor_query1 = cb_fragment1->AnchorQuery();
  ASSERT_NE(cb_anchor_query1, nullptr);
  EXPECT_THAT(AnchorTestData::ToList(*cb_anchor_query1),
              testing::ElementsAre(AnchorTestData{
                  AtomicString("--a1"), PhysicalRect(0, 50, 100, 50)}));
  const NGPhysicalBoxFragment* cb_fragment2 = cb->GetPhysicalFragment(2);
  const NGPhysicalAnchorQuery* cb_anchor_query2 = cb_fragment2->AnchorQuery();
  ASSERT_NE(cb_anchor_query2, nullptr);
  EXPECT_THAT(AnchorTestData::ToList(*cb_anchor_query2),
              testing::ElementsAre(AnchorTestData{
                  AtomicString("--a1"), PhysicalRect(0, 0, 100, 50)}));

  const NGPhysicalAnchorQuery* columns_anchor_query =
      AnchorQueryByElementId("columns");
  ASSERT_NE(columns_anchor_query, nullptr);
  EXPECT_THAT(AnchorTestData::ToList(*columns_anchor_query),
              testing::ElementsAre(AnchorTestData{
                  AtomicString("--a1"), PhysicalRect(110, 0, 210, 100)}));

  const NGPhysicalAnchorQuery* container_anchor_query =
      AnchorQueryByElementId("container");
  ASSERT_NE(container_anchor_query, nullptr);
  EXPECT_THAT(AnchorTestData::ToList(*container_anchor_query),
              testing::ElementsAre(AnchorTestData{
                  AtomicString("--a1"), PhysicalRect(110, 10, 210, 100)}));
}

}  // namespace
}  // namespace blink
