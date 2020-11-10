// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"

#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"

namespace blink {
namespace {

using NGBlockNodeForTest = NGLayoutTest;

TEST_F(NGBlockNodeForTest, IsFloatingForOutOfFlowFloating) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    #container {
      float: left;
      position: absolute;
    }
    </style>
    <div id=container></div>
  )HTML");
  NGBlockNode container(GetLayoutBoxByElementId("container"));
  EXPECT_FALSE(container.IsFloating());
}

TEST_F(NGBlockNodeForTest, ChildInlineAndBlock) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id=container>Hello!<div></div></div>
  )HTML");
  NGBlockNode container(GetLayoutBoxByElementId("container"));
  NGLayoutInputNode child1 = container.FirstChild();
  EXPECT_TRUE(child1 && child1.IsBlock());
  NGLayoutInputNode child2 = child1.NextSibling();
  EXPECT_TRUE(child2 && child2.IsBlock());
  NGLayoutInputNode child3 = child2.NextSibling();
  EXPECT_EQ(child3, nullptr);
}

TEST_F(NGBlockNodeForTest, ChildBlockAndInline) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id=container><div></div>Hello!</div>
  )HTML");
  NGBlockNode container(GetLayoutBoxByElementId("container"));
  NGLayoutInputNode child1 = container.FirstChild();
  EXPECT_TRUE(child1 && child1.IsBlock());
  NGLayoutInputNode child2 = child1.NextSibling();
  EXPECT_TRUE(child2 && child2.IsBlock());
  NGLayoutInputNode child3 = child2.NextSibling();
  EXPECT_EQ(child3, nullptr);
}

TEST_F(NGBlockNodeForTest, ChildFloatBeforeBlock) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      float { float: left; }
    </style>
    <div id=container><float></float><div></div></div>
  )HTML");
  NGBlockNode container(GetLayoutBoxByElementId("container"));
  NGLayoutInputNode child1 = container.FirstChild();
  EXPECT_TRUE(child1 && child1.IsBlock());
  NGLayoutInputNode child2 = child1.NextSibling();
  EXPECT_TRUE(child2 && child2.IsBlock());
  NGLayoutInputNode child3 = child2.NextSibling();
  EXPECT_EQ(child3, nullptr);
}

TEST_F(NGBlockNodeForTest, ChildFloatBeforeInline) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      float { float: left; }
    </style>
    <div id=container><float></float>Hello!</div>
  )HTML");
  NGBlockNode container(GetLayoutBoxByElementId("container"));
  NGLayoutInputNode child1 = container.FirstChild();
  EXPECT_TRUE(child1 && child1.IsInline());
  NGLayoutInputNode child2 = child1.NextSibling();
  EXPECT_EQ(child2, nullptr);
}

TEST_F(NGBlockNodeForTest, ChildFloatAfterInline) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      float { float: left; }
    </style>
    <div id=container>Hello<float></float></div>
  )HTML");
  NGBlockNode container(GetLayoutBoxByElementId("container"));
  NGLayoutInputNode child1 = container.FirstChild();
  EXPECT_TRUE(child1 && child1.IsInline());
  NGLayoutInputNode child2 = child1.NextSibling();
  EXPECT_EQ(child2, nullptr);
}

TEST_F(NGBlockNodeForTest, ChildFloatOnly) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      float { float: left; }
    </style>
    <div id=container><float></float></div>
  )HTML");
  NGBlockNode container(GetLayoutBoxByElementId("container"));
  NGLayoutInputNode child1 = container.FirstChild();
  EXPECT_TRUE(child1 && child1.IsBlock());
  NGLayoutInputNode child2 = child1.NextSibling();
  EXPECT_EQ(child2, nullptr);
}

TEST_F(NGBlockNodeForTest, ChildFloatWithSpaces) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      float { float: left; }
    </style>
    <div id=container>
      <float></float>
    </div>
  )HTML");
  NGBlockNode container(GetLayoutBoxByElementId("container"));
  NGLayoutInputNode child1 = container.FirstChild();
  EXPECT_TRUE(child1 && child1.IsBlock());
  NGLayoutInputNode child2 = child1.NextSibling();
  EXPECT_EQ(child2, nullptr);
}

TEST_F(NGBlockNodeForTest, ChildOofBeforeInline) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      oof { position: absolute; }
    </style>
    <div id=container><oof></oof>Hello!</div>
  )HTML");
  NGBlockNode container(GetLayoutBoxByElementId("container"));
  NGLayoutInputNode child1 = container.FirstChild();
  EXPECT_TRUE(child1 && child1.IsInline());
  NGLayoutInputNode child2 = child1.NextSibling();
  EXPECT_EQ(child2, nullptr);
}

TEST_F(NGBlockNodeForTest, ChildOofAfterInline) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      oof { position: absolute; }
    </style>
    <div id=container>Hello!<oof></oof></div>
  )HTML");
  NGBlockNode container(GetLayoutBoxByElementId("container"));
  NGLayoutInputNode child1 = container.FirstChild();
  EXPECT_TRUE(child1 && child1.IsInline());
  NGLayoutInputNode child2 = child1.NextSibling();
  EXPECT_EQ(child2, nullptr);
}

TEST_F(NGBlockNodeForTest, MinAndMaxContent) {
  SetBodyInnerHTML(R"HTML(
    <div id="box" >
      <div id="first_child" style="width:30px">
      </div>
    </div>
  )HTML");
  const int kWidth = 30;

  NGBlockNode box(GetLayoutBoxByElementId("box"));
  MinMaxSizes sizes =
      box.ComputeMinMaxSizes(
             WritingMode::kHorizontalTb,
             MinMaxSizesInput(
                 /* percentage_resolution_block_size */ LayoutUnit(),
                 MinMaxSizesType::kContent))
          .sizes;
  EXPECT_EQ(LayoutUnit(kWidth), sizes.min_size);
  EXPECT_EQ(LayoutUnit(kWidth), sizes.max_size);
}

// crbug.com/1107291
TEST_F(NGBlockNodeForTest, MinContentForControls) {
  SetBodyInnerHTML(R"HTML(
    <div style="display: flex;">
      <select id="box1" style="border: solid 2px blue; flex: 0; width: 10%;">
      </select>
      <input id="box2" type=file
          style="border: solid 2px blue; flex: 0; width: 10%;">
      <marquee id="box3" style="border: solid 2px blue; flex: 0;">foo</marquee>
    </div>)HTML");
  const char* ids[] = {"box1", "box2", "box3"};
  constexpr int kExpectedMinWidth = 4;

  for (const auto* id : ids) {
    NGBlockNode box(GetLayoutBoxByElementId(id));
    MinMaxSizes sizes =
        box.ComputeMinMaxSizes(
               WritingMode::kHorizontalTb,
               MinMaxSizesInput(
                   /* percentage_resolution_block_size */ LayoutUnit(-1),
                   MinMaxSizesType::kContent))
            .sizes;
    EXPECT_EQ(LayoutUnit(kExpectedMinWidth), sizes.min_size);
  }
}

}  // namespace
}  // namespace blink
