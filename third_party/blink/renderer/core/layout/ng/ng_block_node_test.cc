// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"

#include "third_party/blink/renderer/core/layout/min_max_size.h"
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
  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
  EXPECT_FALSE(container.IsFloating());
}

TEST_F(NGBlockNodeForTest, ChildInlineAndBlock) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id=container>Hello!<div></div></div>
  )HTML");
  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
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
  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
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
  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
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
  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
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
  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
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
  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
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
  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
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
  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
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
  NGBlockNode container(ToLayoutBox(GetLayoutObjectByElementId("container")));
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

  NGBlockNode box(ToLayoutBox(GetLayoutObjectByElementId("box")));
  MinMaxSize sizes = box.ComputeMinMaxSize(
      WritingMode::kHorizontalTb,
      MinMaxSizeInput(/* percentage_resolution_block_size */ LayoutUnit()));
  EXPECT_EQ(LayoutUnit(kWidth), sizes.min_size);
  EXPECT_EQ(LayoutUnit(kWidth), sizes.max_size);
}
}  // namespace
}  // namespace blink
