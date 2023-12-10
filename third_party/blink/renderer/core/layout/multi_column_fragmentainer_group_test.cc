// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/multi_column_fragmentainer_group.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_set.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

class MultiColumnFragmentainerGroupTest : public RenderingTest {
 public:
  MultiColumnFragmentainerGroupTest()
      : flow_thread_(nullptr), column_set_(nullptr) {}

 protected:
  void SetUp() override;
  void TearDown() override;

  LayoutMultiColumnSet& ColumnSet() { return *column_set_; }

  static int GroupCount(const MultiColumnFragmentainerGroupList&);

 private:
  Persistent<LayoutMultiColumnFlowThread> flow_thread_;
  Persistent<LayoutMultiColumnSet> column_set_;
};

void MultiColumnFragmentainerGroupTest::SetUp() {
  RenderingTest::SetUp();
  const ComputedStyle& style = GetDocument().GetStyleResolver().InitialStyle();
  flow_thread_ =
      LayoutMultiColumnFlowThread::CreateAnonymous(GetDocument(), style);
  column_set_ = LayoutMultiColumnSet::CreateAnonymous(*flow_thread_,
                                                      *flow_thread_->Style());
}

void MultiColumnFragmentainerGroupTest::TearDown() {
  column_set_->Destroy();
  flow_thread_->Destroy();
  RenderingTest::TearDown();
}

int MultiColumnFragmentainerGroupTest::GroupCount(
    const MultiColumnFragmentainerGroupList& group_list) {
  int count = 0;
  for (const auto& dummy_group : group_list) {
    (void)dummy_group;
    count++;
  }
  return count;
}

TEST_F(MultiColumnFragmentainerGroupTest, Create) {
  MultiColumnFragmentainerGroupList group_list(ColumnSet());
  EXPECT_EQ(GroupCount(group_list), 1);
}

TEST_F(MultiColumnFragmentainerGroupTest, DeleteExtra) {
  MultiColumnFragmentainerGroupList group_list(ColumnSet());
  EXPECT_EQ(GroupCount(group_list), 1);
  group_list.DeleteExtraGroups();
  EXPECT_EQ(GroupCount(group_list), 1);
}

TEST_F(MultiColumnFragmentainerGroupTest, AddThenDeleteExtra) {
  MultiColumnFragmentainerGroupList group_list(ColumnSet());
  EXPECT_EQ(GroupCount(group_list), 1);
  group_list.AddExtraGroup();
  EXPECT_EQ(GroupCount(group_list), 2);
  group_list.DeleteExtraGroups();
  EXPECT_EQ(GroupCount(group_list), 1);
}

TEST_F(MultiColumnFragmentainerGroupTest,
       AddTwoThenDeleteExtraThenAddThreeThenDeleteExtra) {
  MultiColumnFragmentainerGroupList group_list(ColumnSet());
  EXPECT_EQ(GroupCount(group_list), 1);
  group_list.AddExtraGroup();
  EXPECT_EQ(GroupCount(group_list), 2);
  group_list.AddExtraGroup();
  EXPECT_EQ(GroupCount(group_list), 3);
  group_list.DeleteExtraGroups();
  EXPECT_EQ(GroupCount(group_list), 1);
  group_list.AddExtraGroup();
  EXPECT_EQ(GroupCount(group_list), 2);
  group_list.AddExtraGroup();
  EXPECT_EQ(GroupCount(group_list), 3);
  group_list.AddExtraGroup();
  EXPECT_EQ(GroupCount(group_list), 4);
  group_list.DeleteExtraGroups();
  EXPECT_EQ(GroupCount(group_list), 1);
}

// The following test tests that we DON'T restrict actual column count, when
// there's a legitimate reason to use many columns. The code that checks the
// allowance and potentially applies this limitation is in
// MultiColumnFragmentainerGroup::ActualColumnCount().
TEST_F(MultiColumnFragmentainerGroupTest, LotsOfContent) {
  StringBuilder builder;
  builder.Append(
      "<div id='multicol' style='columns:3; column-gap:1px; width:101px; "
      "line-height:50px; orphans:1; widows:1; height:60px;'>");
  for (int i = 0; i < 100; i++)
    builder.Append("line<br>");
  builder.Append("</div>");
  String html;
  SetBodyInnerHTML(builder.ToString());
  const auto* multicol = GetLayoutObjectByElementId("multicol");
  ASSERT_TRUE(multicol);
  ASSERT_TRUE(multicol->IsLayoutBlockFlow());
  const auto* column_set = multicol->SlowLastChild();
  ASSERT_TRUE(column_set);
  ASSERT_TRUE(column_set->IsLayoutMultiColumnSet());
  const auto& fragmentainer_group =
      To<LayoutMultiColumnSet>(column_set)->FirstFragmentainerGroup();
  EXPECT_EQ(fragmentainer_group.ActualColumnCount(), 100U);
  EXPECT_EQ(fragmentainer_group.GroupLogicalHeight(), LayoutUnit(60));
  auto overflow = To<LayoutBox>(multicol)->ScrollableOverflowRect();
  EXPECT_EQ(To<LayoutBox>(multicol)->LogicalWidth(), LayoutUnit(101));
  EXPECT_EQ(To<LayoutBox>(multicol)->LogicalHeight(), LayoutUnit(60));
  EXPECT_EQ(overflow.Width(), LayoutUnit(3399));
  EXPECT_EQ(overflow.Height(), LayoutUnit(60));
}

// The following test tests that we DON'T restrict actual column count, when
// there's a legitimate reason to use many columns. The code that checks the
// allowance and potentially applies this limitation is in
// MultiColumnFragmentainerGroup::ActualColumnCount().
TEST_F(MultiColumnFragmentainerGroupTest, LotsOfNestedBlocksWithText) {
  StringBuilder builder;
  builder.Append(
      "<div id='multicol' style='columns:3; column-gap:1px; width:101px; "
      "line-height:50px; height:200px;'>");
  for (int i = 0; i < 1000; i++)
    builder.Append("<div><div><div>line</div></div></div>");
  builder.Append("</div>");
  String html;
  SetBodyInnerHTML(builder.ToString());
  const auto* multicol = GetLayoutObjectByElementId("multicol");
  ASSERT_TRUE(multicol);
  ASSERT_TRUE(multicol->IsLayoutBlockFlow());
  const auto* column_set = multicol->SlowLastChild();
  ASSERT_TRUE(column_set);
  ASSERT_TRUE(column_set->IsLayoutMultiColumnSet());
  const auto& fragmentainer_group =
      To<LayoutMultiColumnSet>(column_set)->FirstFragmentainerGroup();
  EXPECT_EQ(fragmentainer_group.ActualColumnCount(), 250U);
  EXPECT_EQ(fragmentainer_group.GroupLogicalHeight(), LayoutUnit(200));
  auto overflow = To<LayoutBox>(multicol)->ScrollableOverflowRect();
  EXPECT_EQ(To<LayoutBox>(multicol)->LogicalWidth(), LayoutUnit(101));
  EXPECT_EQ(To<LayoutBox>(multicol)->LogicalHeight(), LayoutUnit(200));
  EXPECT_EQ(overflow.Width(), LayoutUnit(8499));
  EXPECT_EQ(overflow.Height(), LayoutUnit(200));
}

// The following test tests that we DON'T restrict actual column count, when
// there's a legitimate reason to use many columns. The code that checks the
// allowance and potentially applies this limitation is in
// MultiColumnFragmentainerGroup::ActualColumnCount().
TEST_F(MultiColumnFragmentainerGroupTest, NestedBlocksWithLotsOfContent) {
  StringBuilder builder;
  builder.Append(
      "<div id='multicol' style='columns:3; column-gap:1px; width:101px; "
      "line-height:50px; orphans:1; widows:1; height:60px;'><div><div><div>");
  for (int i = 0; i < 100; i++)
    builder.Append("line<br>");
  builder.Append("</div></div></div></div>");
  String html;
  SetBodyInnerHTML(builder.ToString());
  const auto* multicol = GetLayoutObjectByElementId("multicol");
  ASSERT_TRUE(multicol);
  ASSERT_TRUE(multicol->IsLayoutBlockFlow());
  const auto* column_set = multicol->SlowLastChild();
  ASSERT_TRUE(column_set);
  ASSERT_TRUE(column_set->IsLayoutMultiColumnSet());
  const auto& fragmentainer_group =
      To<LayoutMultiColumnSet>(column_set)->FirstFragmentainerGroup();
  EXPECT_EQ(fragmentainer_group.ActualColumnCount(), 100U);
  EXPECT_EQ(fragmentainer_group.GroupLogicalHeight(), LayoutUnit(60));
  auto overflow = To<LayoutBox>(multicol)->ScrollableOverflowRect();
  EXPECT_EQ(To<LayoutBox>(multicol)->LogicalWidth(), LayoutUnit(101));
  EXPECT_EQ(To<LayoutBox>(multicol)->LogicalHeight(), LayoutUnit(60));
  EXPECT_EQ(overflow.Width(), LayoutUnit(3399));
  EXPECT_EQ(overflow.Height(), LayoutUnit(60));
}

}  // anonymous namespace

}  // namespace blink
