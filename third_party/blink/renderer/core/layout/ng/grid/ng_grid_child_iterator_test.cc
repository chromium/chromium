// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_child_iterator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"

namespace blink {
namespace {

TEST_F(NGLayoutTest, TestNGGridChildIterator) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #parent {
      display: grid;
      grid-template-columns: 10px 10px;
      grid-template-rows: 10px 10px;
    }
    </style>
    <div id="parent">
      <div id="child1">Child 1</div>
      <div id="child2">Child 2</div>
      <div id="child3">Child 3</div>
      <div id="child4">Child 4</div>
    </div>
  )HTML");

  NGBlockNode parent_block(GetLayoutBoxByElementId("parent"));

  int index = 0;
  NGGridChildIterator iterator(parent_block);
  for (NGBlockNode child = iterator.NextChild(); child;
       child = iterator.NextChild()) {
    StringBuilder cell_id;
    cell_id.Append("child");
    cell_id.Append(AtomicString::Number(++index));
    NGBlockNode cell_block(
        GetLayoutBoxByElementId(cell_id.ToString().Ascii().c_str()));
    EXPECT_EQ(child, cell_block);
  }

  EXPECT_EQ(index, 4);
}

TEST_F(NGLayoutTest, TestNGGridChildIteratorWithOrderReversed) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #parent {
      display: grid;
      grid-template-columns: 10px 10px;
      grid-template-rows: 10px 10px;
    }
    </style>
    <div id="parent">
      <div id="child1" style="order: 4">Child 1</div>
      <div id="child2" style="order: 3">Child 2</div>
      <div id="child3" style="order: 2">Child 3</div>
      <div id="child4" style="order: 1">Child 4</div>
    </div>
  )HTML");

  NGBlockNode parent_block(GetLayoutBoxByElementId("parent"));

  int index = 4;
  NGGridChildIterator iterator(parent_block);
  for (NGBlockNode child = iterator.NextChild(); child;
       child = iterator.NextChild()) {
    StringBuilder cell_id;
    cell_id.Append("child");
    cell_id.Append(AtomicString::Number(index));
    NGBlockNode cell_block(
        GetLayoutBoxByElementId(cell_id.ToString().Ascii().c_str()));
    EXPECT_EQ(child, cell_block);
    --index;
  }

  EXPECT_EQ(index, 0);
}

TEST_F(NGLayoutTest, TestNGGridChildIteratorWithOrderMixed) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #parent {
      display: grid;
      grid-template-columns: 10px 10px;
      grid-template-rows: 10px 10px;
    }
    </style>
    <div id="parent"">
      <div id="child1" style="order: 3">Child 1</div>
      <div id="child2" style="order: 3">Child 2</div>
      <div id="child3" style="order: -1">Child 3</div>
      <div id="child4" style="order: 0">Child 4</div>
    </div>
  )HTML");

  NGBlockNode parent_block(GetLayoutBoxByElementId("parent"));
  int expected_order[] = {3, 4, 1, 2};

  int index = 0;
  NGGridChildIterator iterator(parent_block);
  for (NGBlockNode child = iterator.NextChild(); child;
       child = iterator.NextChild()) {
    StringBuilder cell_id;
    cell_id.Append("child");
    cell_id.Append(AtomicString::Number(expected_order[index]));
    NGBlockNode cell_block(
        GetLayoutBoxByElementId(cell_id.ToString().Ascii().c_str()));
    EXPECT_EQ(child, cell_block);
    ++index;
  }

  EXPECT_EQ(index, 4);
}

}  // namespace

}  // namespace blink
