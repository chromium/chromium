// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid.h"
#include "third_party/blink/renderer/core/layout/layout_grid.h"

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

class GridTest : public RenderingTest {
 protected:
  LayoutGrid* GetGridByElementId(const char* id) {
    return To<LayoutGrid>(GetLayoutObjectByElementId(id));
  }
};

TEST_F(GridTest, EmptyGrid) {
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
      .grid { display: grid; }
    </style>
    <div id=target class=grid>
    </div>
  )HTML");
  auto* layout_grid = GetGridByElementId("target");
  auto* grid = layout_grid->InternalGrid();
  ASSERT_NE(grid, nullptr);

  EXPECT_EQ(0u, grid->NumTracks(kForRows));
  EXPECT_EQ(0u, grid->NumTracks(kForColumns));

  EXPECT_FALSE(grid->HasGridItems());

  EXPECT_EQ(0u, grid->ExplicitGridStart(kForRows));
  EXPECT_EQ(0u, grid->ExplicitGridStart(kForColumns));

  EXPECT_EQ(0u, grid->AutoRepeatTracks(kForRows));
  EXPECT_EQ(0u, grid->AutoRepeatTracks(kForColumns));
  EXPECT_FALSE(grid->HasAutoRepeatEmptyTracks(kForRows));
  EXPECT_FALSE(grid->HasAutoRepeatEmptyTracks(kForColumns));
}

TEST_F(GridTest, SingleChild) {
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
      .grid { display: grid; }
    </style>
    <div id=target class=grid>
      <div id=child></div>
    </div>
  )HTML");
  auto* layout_grid = GetGridByElementId("target");
  auto* grid = layout_grid->InternalGrid();
  ASSERT_NE(grid, nullptr);
  auto* child = GetLayoutBoxByElementId("child");
  ASSERT_NE(child, nullptr);

  EXPECT_EQ(1u, grid->NumTracks(kForRows));
  EXPECT_EQ(1u, grid->NumTracks(kForColumns));

  EXPECT_TRUE(grid->HasGridItems());

  EXPECT_EQ(0u, grid->ExplicitGridStart(kForRows));
  EXPECT_EQ(0u, grid->ExplicitGridStart(kForColumns));

  auto area = grid->GridItemArea(*child);
  EXPECT_EQ(0u, area.columns.StartLine());
  EXPECT_EQ(1u, area.columns.EndLine());
  EXPECT_EQ(0u, area.rows.StartLine());
  EXPECT_EQ(1u, area.rows.EndLine());
}

TEST_F(GridTest, OverlappingChildren) {
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
      .grid { display: grid; grid-template: repeat(3, 20px) / repeat(3, 20px); }
      #child1 { grid-row: 1 / 3; grid-column: 1 / 3 }
      #child2 { grid-row: 1 / 3; grid-column: 2 / 4 }
      #child3 { grid-row: 2 / 4; grid-column: 1 / 3 }
      #child4 { grid-row: 2 / 4; grid-column: 2 / 4 }
    </style>
    <div id=target class=grid>
      <div id=child1></div>
      <div id=child2></div>
      <div id=child3></div>
      <div id=child4></div>
    </div>
  )HTML");
  auto* layout_grid = GetGridByElementId("target");
  auto* grid = layout_grid->InternalGrid();
  ASSERT_NE(grid, nullptr);

  wtf_size_t num_rows = grid->NumTracks(kForRows);
  wtf_size_t num_cols = grid->NumTracks(kForColumns);
  EXPECT_EQ(3u, num_rows);
  EXPECT_EQ(3u, num_cols);

  EXPECT_TRUE(grid->HasGridItems());

  wtf_size_t index = 0;
  Vector<wtf_size_t> expected_items_per_cell = {1, 2, 1, 2, 4, 2, 1, 2, 1};
  for (wtf_size_t row = 0; row < num_rows; ++row) {
    for (wtf_size_t col = 0; col < num_cols; ++col)
      EXPECT_EQ(expected_items_per_cell[index++], grid->Cell(row, col).size());
  }
}

TEST_F(GridTest, PartiallyOverlappingChildren) {
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
      .grid { display: grid; grid-template: repeat(1, 20px) / repeat(3, 20px); }
      #child1 { grid-row: 1; grid-column: 1; }
      #child2 { grid-row: 1; grid-column: 3; }
      #child3 { grid-row: 1; grid-column: 1 / 3 }
    </style>
    <div id=target class=grid>
      <div id=child1></div>
      <div id=child2></div>
      <div id=child3></div>
    </div>
  )HTML");
  auto* layout_grid = GetGridByElementId("target");
  auto* grid = layout_grid->InternalGrid();
  ASSERT_NE(grid, nullptr);

  wtf_size_t num_rows = grid->NumTracks(kForRows);
  wtf_size_t num_cols = grid->NumTracks(kForColumns);
  EXPECT_EQ(1u, num_rows);
  EXPECT_EQ(3u, num_cols);

  EXPECT_TRUE(grid->HasGridItems());

  wtf_size_t index = 0;
  Vector<wtf_size_t> expected_items_per_cell = {2, 1, 1};
  for (wtf_size_t col = 0; col < num_cols; ++col)
    EXPECT_EQ(expected_items_per_cell[index++], grid->Cell(0, col).size());
}

TEST_F(GridTest, IntrinsicGrid) {
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
      .grid { display: grid; grid-template-rows: repeat(2, 10px); }
      #child1 { grid-row: -1 / -5; }
      #child2 { grid-row: 3 / span 4; }
    </style>
    <div id=target class=grid>
      <div id=child1></div>
      <div id=child2></div>
    </div>
  )HTML");
  auto* layout_grid = GetGridByElementId("target");
  auto* grid = layout_grid->InternalGrid();
  ASSERT_NE(grid, nullptr);
  auto* child1 = GetLayoutBoxByElementId("child1");
  ASSERT_NE(child1, nullptr);
  auto* child2 = GetLayoutBoxByElementId("child2");
  ASSERT_NE(child2, nullptr);

  EXPECT_EQ(8u, grid->NumTracks(kForRows));
  EXPECT_EQ(1u, grid->NumTracks(kForColumns));

  EXPECT_TRUE(grid->HasGridItems());

  EXPECT_EQ(2u, grid->ExplicitGridStart(kForRows));
  EXPECT_EQ(0u, grid->ExplicitGridStart(kForColumns));

  auto area = grid->GridItemArea(*child1);
  EXPECT_EQ(0u, area.columns.StartLine());
  EXPECT_EQ(1u, area.columns.EndLine());
  EXPECT_EQ(0u, area.rows.StartLine());
  EXPECT_EQ(4u, area.rows.EndLine());

  area = grid->GridItemArea(*child2);
  EXPECT_EQ(0u, area.columns.StartLine());
  EXPECT_EQ(1u, area.columns.EndLine());
  EXPECT_EQ(4u, area.rows.StartLine());
  EXPECT_EQ(8u, area.rows.EndLine());
}

TEST_F(GridTest, AutoFit) {
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
      .grid { display: grid; width: 100px; grid-template-columns: repeat(auto-fit, 10px); }
      #child { grid-column: 2 / 6; }
      #child2 { grid-column: 9; }
    </style>
    <div id=target class=grid>
      <div id=child></div>
      <div id=child2></div>
    </div>
  )HTML");
  auto* layout_grid = GetGridByElementId("target");
  auto* grid = layout_grid->InternalGrid();
  ASSERT_NE(grid, nullptr);

  EXPECT_EQ(1u, grid->NumTracks(kForRows));
  EXPECT_EQ(10u, grid->NumTracks(kForColumns));

  EXPECT_TRUE(grid->HasGridItems());

  EXPECT_EQ(0u, grid->AutoRepeatTracks(kForRows));
  EXPECT_EQ(10u, grid->AutoRepeatTracks(kForColumns));
  EXPECT_FALSE(grid->HasAutoRepeatEmptyTracks(kForRows));
  EXPECT_TRUE(grid->HasAutoRepeatEmptyTracks(kForColumns));

  auto* empty_tracks = grid->AutoRepeatEmptyTracks(kForColumns);
  ASSERT_NE(empty_tracks, nullptr);
  ASSERT_EQ(empty_tracks->size(), 5u);
  Vector<size_t> expected_empty_tracks = {0, 5, 6, 7, 9};
  wtf_size_t index = 0;
  for (auto track : *empty_tracks) {
    EXPECT_EQ(expected_empty_tracks[index++], track);
    EXPECT_TRUE(grid->IsEmptyAutoRepeatTrack(kForColumns, track));
  }
}

TEST_F(GridTest, AutoFill) {
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
      .grid { display: grid; width: 100px; grid-template-columns: repeat(auto-fill, 10px); }
      #child { grid-column: 2 / 6; }
      #child2 { grid-column: 9; }
    </style>
    <div id=target class=grid>
      <div id=child></div>
      <div id=child2></div>
    </div>
  )HTML");
  auto* layout_grid = GetGridByElementId("target");
  auto* grid = layout_grid->InternalGrid();
  ASSERT_NE(grid, nullptr);

  EXPECT_EQ(1u, grid->NumTracks(kForRows));
  EXPECT_EQ(10u, grid->NumTracks(kForColumns));

  EXPECT_TRUE(grid->HasGridItems());

  EXPECT_EQ(0u, grid->AutoRepeatTracks(kForRows));
  EXPECT_EQ(10u, grid->AutoRepeatTracks(kForColumns));
  EXPECT_FALSE(grid->HasAutoRepeatEmptyTracks(kForRows));
  EXPECT_FALSE(grid->HasAutoRepeatEmptyTracks(kForColumns));
}

TEST_F(GridTest, AutoPositionedItems) {
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
      .grid { display: grid; grid-template-rows: repeat(3, 10px); grid-auto-flow: column }
      .vertical { writing-mode: vertical-rl}
    </style>
    <div id=target class=grid>
      <div></div>
      <div class=vertical></div>
      <div></div>
      <div></div>
    </div>
  )HTML");
  auto* layout_grid = GetGridByElementId("target");
  auto* grid = layout_grid->InternalGrid();
  ASSERT_NE(grid, nullptr);

  EXPECT_EQ(3u, grid->NumTracks(kForRows));
  EXPECT_EQ(2u, grid->NumTracks(kForColumns));

  EXPECT_TRUE(grid->HasGridItems());
  EXPECT_FALSE(grid->NeedsItemsPlacement());
}

TEST_F(GridTest, ExplicitlyPositionedChild) {
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
      .grid { display: grid; }
      #child { grid-row: 1; grid-column: 2 / span 3; }
    </style>
    <div id=target class=grid>
      <div id=child></div>
    </div>
  )HTML");
  auto* layout_grid = GetGridByElementId("target");
  auto* grid = layout_grid->InternalGrid();
  ASSERT_NE(grid, nullptr);
  auto* child = GetLayoutBoxByElementId("child");
  ASSERT_NE(child, nullptr);

  EXPECT_EQ(1u, grid->NumTracks(kForRows));
  EXPECT_EQ(4u, grid->NumTracks(kForColumns));

  EXPECT_TRUE(grid->HasGridItems());

  EXPECT_EQ(0u, grid->ExplicitGridStart(kForRows));
  EXPECT_EQ(0u, grid->ExplicitGridStart(kForColumns));

  auto area = grid->GridItemArea(*child);
  EXPECT_EQ(1u, area.columns.StartLine());
  EXPECT_EQ(4u, area.columns.EndLine());
  EXPECT_EQ(0u, area.rows.StartLine());
  EXPECT_EQ(1u, area.rows.EndLine());

  for (auto row : area.rows) {
    for (auto column : area.columns) {
      auto& cell = grid->Cell(row, column);
      EXPECT_EQ(1u, cell.size());
    }
  }
}

TEST_F(GridTest, CellInsert) {
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  auto* track = MakeGarbageCollected<ListGrid::GridTrack>(0, kForColumns);
  auto* cell = MakeGarbageCollected<ListGrid::GridCell>(0, 0);

  auto result = track->Insert(cell);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(cell, result.node);

  auto* cell2 = MakeGarbageCollected<ListGrid::GridCell>(1, 0);
  result = track->Insert(cell2);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(cell2, result.node);

  result = track->InsertAfter(cell2, cell);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(cell2, result.node);

  auto* cell3 = MakeGarbageCollected<ListGrid::GridCell>(2, 0);
  result = track->InsertAfter(cell3, cell2);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(cell3, result.node);
}

}  // anonymous namespace

}  // namespace blink
