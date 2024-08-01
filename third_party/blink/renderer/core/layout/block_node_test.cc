// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/block_node.h"

#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {
namespace {

using BlockNodeForTest = RenderingTest;

TEST_F(BlockNodeForTest, IsFloatingForOutOfFlowFloating) {
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
  BlockNode container(GetLayoutBoxByElementId("container"));
  EXPECT_FALSE(container.IsFloating());
}

TEST_F(BlockNodeForTest, ChildInlineAndBlock) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id=container>Hello!<div></div></div>
  )HTML");
  BlockNode container(GetLayoutBoxByElementId("container"));
  LayoutInputNode child1 = container.FirstChild();
  EXPECT_TRUE(child1 && child1.IsBlock());
  LayoutInputNode child2 = child1.NextSibling();
  EXPECT_TRUE(child2 && child2.IsBlock());
  LayoutInputNode child3 = child2.NextSibling();
  EXPECT_EQ(child3, nullptr);
}

TEST_F(BlockNodeForTest, ChildBlockAndInline) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id=container><div></div>Hello!</div>
  )HTML");
  BlockNode container(GetLayoutBoxByElementId("container"));
  LayoutInputNode child1 = container.FirstChild();
  EXPECT_TRUE(child1 && child1.IsBlock());
  LayoutInputNode child2 = child1.NextSibling();
  EXPECT_TRUE(child2 && child2.IsBlock());
  LayoutInputNode child3 = child2.NextSibling();
  EXPECT_EQ(child3, nullptr);
}

TEST_F(BlockNodeForTest, ChildFloatBeforeBlock) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      float { float: left; }
    </style>
    <div id=container><float></float><div></div></div>
  )HTML");
  BlockNode container(GetLayoutBoxByElementId("container"));
  LayoutInputNode child1 = container.FirstChild();
  EXPECT_TRUE(child1 && child1.IsBlock());
  LayoutInputNode child2 = child1.NextSibling();
  EXPECT_TRUE(child2 && child2.IsBlock());
  LayoutInputNode child3 = child2.NextSibling();
  EXPECT_EQ(child3, nullptr);
}

TEST_F(BlockNodeForTest, ChildFloatBeforeInline) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      float { float: left; }
    </style>
    <div id=container><float></float>Hello!</div>
  )HTML");
  BlockNode container(GetLayoutBoxByElementId("container"));
  LayoutInputNode child1 = container.FirstChild();
  EXPECT_TRUE(child1 && child1.IsInline());
  LayoutInputNode child2 = child1.NextSibling();
  EXPECT_EQ(child2, nullptr);
}

TEST_F(BlockNodeForTest, ChildFloatAfterInline) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      float { float: left; }
    </style>
    <div id=container>Hello<float></float></div>
  )HTML");
  BlockNode container(GetLayoutBoxByElementId("container"));
  LayoutInputNode child1 = container.FirstChild();
  EXPECT_TRUE(child1 && child1.IsInline());
  LayoutInputNode child2 = child1.NextSibling();
  EXPECT_EQ(child2, nullptr);
}

TEST_F(BlockNodeForTest, ChildFloatOnly) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      float { float: left; }
    </style>
    <div id=container><float></float></div>
  )HTML");
  BlockNode container(GetLayoutBoxByElementId("container"));
  LayoutInputNode child1 = container.FirstChild();
  EXPECT_TRUE(child1 && child1.IsBlock());
  LayoutInputNode child2 = child1.NextSibling();
  EXPECT_EQ(child2, nullptr);
}

TEST_F(BlockNodeForTest, ChildFloatWithSpaces) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      float { float: left; }
    </style>
    <div id=container>
      <float></float>
    </div>
  )HTML");
  BlockNode container(GetLayoutBoxByElementId("container"));
  LayoutInputNode child1 = container.FirstChild();
  EXPECT_TRUE(child1 && child1.IsBlock());
  LayoutInputNode child2 = child1.NextSibling();
  EXPECT_EQ(child2, nullptr);
}

TEST_F(BlockNodeForTest, ChildOofBeforeInline) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      oof { position: absolute; }
    </style>
    <div id=container><oof></oof>Hello!</div>
  )HTML");
  BlockNode container(GetLayoutBoxByElementId("container"));
  LayoutInputNode child1 = container.FirstChild();
  EXPECT_TRUE(child1 && child1.IsInline());
  LayoutInputNode child2 = child1.NextSibling();
  EXPECT_EQ(child2, nullptr);
}

TEST_F(BlockNodeForTest, ChildOofAfterInline) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      oof { position: absolute; }
    </style>
    <div id=container>Hello!<oof></oof></div>
  )HTML");
  BlockNode container(GetLayoutBoxByElementId("container"));
  LayoutInputNode child1 = container.FirstChild();
  EXPECT_TRUE(child1 && child1.IsInline());
  LayoutInputNode child2 = child1.NextSibling();
  EXPECT_EQ(child2, nullptr);
}

// crbug.com/1107291
TEST_F(BlockNodeForTest, MinContentForControls) {
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

  // The space doesn't matter for this test.
  const auto space =
      ConstraintSpaceBuilder(WritingMode::kHorizontalTb,
                             {WritingMode::kHorizontalTb, TextDirection::kLtr},
                             /* is_new_fc */ true)
          .ToConstraintSpace();

  for (const auto* id : ids) {
    BlockNode box(GetLayoutBoxByElementId(id));
    MinMaxSizes sizes = box.ComputeMinMaxSizes(WritingMode::kHorizontalTb,
                                               SizeType::kContent, space)
                            .sizes;
    EXPECT_EQ(LayoutUnit(kExpectedMinWidth), sizes.min_size);
  }
}

}  // namespace
}  // namespace blink
