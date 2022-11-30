/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/layout/layout_frame_set.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/frame_edge_info.h"
#include "third_party/blink/renderer/core/html/html_dimension.h"
#include "third_party/blink/renderer/core/html/html_frame_element.h"
#include "third_party/blink/renderer/core/html/html_frame_set_element.h"
#include "third_party/blink/renderer/core/layout/layout_frame.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/frame_set_painter.h"
#include "third_party/blink/renderer/platform/cursors.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "ui/base/cursor/cursor.h"

namespace blink {

// Adjusts proportionally the size with remaining size.
static int AdjustSizeToRemainingSize(int current,
                                     int remaining,
                                     int64_t total) {
  // Performs the math operations step by step to avoid the overflow.
  base::CheckedNumeric<int64_t> temp_product = current;
  temp_product *= remaining;
  temp_product /= total;
  return base::checked_cast<int>(temp_product.ValueOrDie());
}

LayoutFrameSet::LayoutFrameSet(Element* element) : LayoutBox(element) {
  DCHECK(IsA<HTMLFrameSetElement>(element));
  SetInline(false);
}

LayoutFrameSet::~LayoutFrameSet() = default;

void LayoutFrameSet::Trace(Visitor* visitor) const {
  visitor->Trace(children_);
  LayoutBox::Trace(visitor);
}

HTMLFrameSetElement* LayoutFrameSet::FrameSet() const {
  NOT_DESTROYED();
  return To<HTMLFrameSetElement>(GetNode());
}

void LayoutFrameSet::Paint(const PaintInfo& paint_info) const {
  NOT_DESTROYED();
  FrameSetPainter(*this).Paint(paint_info);
}

void LayoutFrameSet::GridAxis::Resize(int size) {
  sizes_.resize(size);
}

void LayoutFrameSet::LayOutAxis(GridAxis& axis,
                                const Vector<HTMLDimension>& grid,
                                const Vector<int>& deltas,
                                int available_len) {
  NOT_DESTROYED();
  DCHECK_EQ(axis.sizes_.size(), deltas.size());

  available_len = max(available_len, 0);

  DCHECK_EQ(axis.sizes_.size(), deltas.size());

  if (grid.empty()) {
    axis.sizes_[0] = LayoutUnit(available_len);
    return;
  }

  Vector<int> grid_layout(axis.sizes_.size());
  int grid_len = axis.sizes_.size();
  DCHECK(grid_len);

  int64_t total_relative = 0;
  int64_t total_fixed = 0;
  int64_t total_percent = 0;
  int count_relative = 0;
  int count_fixed = 0;
  int count_percent = 0;

  float effective_zoom = StyleRef().EffectiveZoom();

  // First we need to investigate how many columns of each type we have and
  // how much space these columns are going to require.
  for (int i = 0; i < grid_len; ++i) {
    // Count the total length of all of the fixed columns/rows -> totalFixed.
    // Count the number of columns/rows which are fixed -> countFixed.
    if (grid[i].IsAbsolute()) {
      grid_layout[i] = ClampTo<int>(max(grid[i].Value() * effective_zoom, 0.0));
      total_fixed += grid_layout[i];
      count_fixed++;
    }

    // Count the total percentage of all of the percentage columns/rows ->
    // totalPercent. Count the number of columns/rows which are percentages ->
    // countPercent.
    if (grid[i].IsPercentage()) {
      grid_layout[i] =
          ClampTo<int>(max(grid[i].Value() * available_len / 100., 0.0));
      total_percent += grid_layout[i];
      count_percent++;
    }

    // Count the total relative of all the relative columns/rows ->
    // totalRelative. Count the number of columns/rows which are relative ->
    // countRelative.
    if (grid[i].IsRelative()) {
      total_relative += ClampTo<int>(max(grid[i].Value(), 1.0));
      count_relative++;
    }
  }

  int remaining_len = available_len;

  // Fixed columns/rows are our first priority. If there is not enough space to
  // fit all fixed columns/rows we need to proportionally adjust their size.
  if (total_fixed > remaining_len) {
    int remaining_fixed = remaining_len;

    for (int i = 0; i < grid_len; ++i) {
      if (grid[i].IsAbsolute()) {
        grid_layout[i] = AdjustSizeToRemainingSize(
            grid_layout[i], remaining_fixed, total_fixed);
        remaining_len -= grid_layout[i];
      }
    }
  } else {
    remaining_len -= total_fixed;
  }

  // Percentage columns/rows are our second priority. Divide the remaining space
  // proportionally over all percentage columns/rows.
  // NOTE: the size of each column/row is not relative to 100%, but to the total
  // percentage. For example, if there are three columns, each of 75%, and the
  // available space is 300px, each column will become 100px in width.
  if (total_percent > remaining_len) {
    int remaining_percent = remaining_len;

    for (int i = 0; i < grid_len; ++i) {
      if (grid[i].IsPercentage()) {
        grid_layout[i] = AdjustSizeToRemainingSize(
            grid_layout[i], remaining_percent, total_percent);
        remaining_len -= grid_layout[i];
      }
    }
  } else {
    remaining_len -= total_percent;
  }

  // Relative columns/rows are our last priority. Divide the remaining space
  // proportionally over all relative columns/rows.
  // NOTE: the relative value of 0* is treated as 1*.
  if (count_relative) {
    int last_relative = 0;
    int64_t remaining_relative = remaining_len;

    for (int i = 0; i < grid_len; ++i) {
      if (grid[i].IsRelative()) {
        grid_layout[i] = ClampTo<int>(
            (ClampTo<int>(max(grid[i].Value(), 1.)) * remaining_relative) /
            total_relative);
        remaining_len -= grid_layout[i];
        DCHECK_GE(remaining_len, 0);
        last_relative = i;
      }
    }

    // If we could not evenly distribute the available space of all of the
    // relative columns/rows, the remainder will be added to the last column/
    // row. For example: if we have a space of 100px and three columns (*,*,*),
    // the remainder will be 1px and will be added to the last column: 33px,
    // 33px, 34px.
    if (remaining_len) {
      grid_layout[last_relative] += remaining_len;
      remaining_len = 0;
    }
  }

  // If we still have some left over space we need to divide it over the already
  // existing columns/rows
  if (remaining_len) {
    // Our first priority is to spread if over the percentage columns. The
    // remaining space is spread evenly, for example: if we have a space of
    // 100px, the columns definition of 25%,25% used to result in two columns of
    // 25px. After this the columns will each be 50px in width.
    if (count_percent && total_percent) {
      int remaining_percent = remaining_len;
      int change_percent = 0;

      for (int i = 0; i < grid_len; ++i) {
        if (grid[i].IsPercentage()) {
          change_percent = AdjustSizeToRemainingSize(
              grid_layout[i], remaining_percent, total_percent);
          grid_layout[i] += change_percent;
          remaining_len -= change_percent;
        }
      }
    } else if (total_fixed) {
      // Our last priority is to spread the remaining space over the fixed
      // columns. For example if we have 100px of space and two column of each
      // 40px, both columns will become exactly 50px.
      int remaining_fixed = remaining_len;
      int change_fixed = 0;

      for (int i = 0; i < grid_len; ++i) {
        if (grid[i].IsAbsolute()) {
          change_fixed = AdjustSizeToRemainingSize(
              grid_layout[i], remaining_fixed, total_fixed);
          grid_layout[i] += change_fixed;
          remaining_len -= change_fixed;
        }
      }
    }
  }

  // If we still have some left over space we probably ended up with a remainder
  // of a division. We cannot spread it evenly anymore. If we have any
  // percentage columns/rows simply spread the remainder equally over all
  // available percentage columns, regardless of their size.
  if (remaining_len && count_percent) {
    int remaining_percent = remaining_len;
    int change_percent = 0;

    for (int i = 0; i < grid_len; ++i) {
      if (grid[i].IsPercentage()) {
        change_percent = remaining_percent / count_percent;
        grid_layout[i] += change_percent;
        remaining_len -= change_percent;
      }
    }
  } else if (remaining_len && count_fixed) {
    // If we don't have any percentage columns/rows we only have fixed columns.
    // Spread the remainder equally over all fixed columns/rows.
    int remaining_fixed = remaining_len;
    int change_fixed = 0;

    for (int i = 0; i < grid_len; ++i) {
      if (grid[i].IsAbsolute()) {
        change_fixed = remaining_fixed / count_fixed;
        grid_layout[i] += change_fixed;
        remaining_len -= change_fixed;
      }
    }
  }

  // Still some left over. Add it to the last column, because it is impossible
  // spread it evenly or equally.
  if (remaining_len)
    grid_layout[grid_len - 1] += remaining_len;

  // now we have the final layout, distribute the delta over it
  bool worked = true;
  for (int i = 0; i < grid_len; ++i) {
    if (grid_layout[i] && grid_layout[i] + deltas[i] <= 0)
      worked = false;
    grid_layout[i] += deltas[i];
  }
  // if the deltas broke something, undo them
  if (!worked) {
    for (int i = 0; i < grid_len; ++i)
      grid_layout[i] -= deltas[i];
  }

  for (int i = 0; i < grid_len; ++i)
    axis.sizes_[i] = LayoutUnit(grid_layout[i]);
}

void LayoutFrameSet::UpdateLayout() {
  NOT_DESTROYED();
  DCHECK(NeedsLayout());

  if (!Parent()->IsFrameSet()) {
    SetWidth(LayoutUnit(View()->ViewWidth()));
    SetHeight(LayoutUnit(View()->ViewHeight()));
  }

  unsigned cols = FrameSet()->TotalCols();
  unsigned rows = FrameSet()->TotalRows();
  const Vector<int>& rows_deltas = FrameSet()->RowDeltas();
  const Vector<int>& cols_deltas = FrameSet()->ColDeltas();

  if (rows_.sizes_.size() != rows || cols_.sizes_.size() != cols) {
    rows_.Resize(rows);
    cols_.Resize(cols);
  }

  LayoutUnit border_thickness(FrameSet()->Border(StyleRef()));
  LayOutAxis(rows_, FrameSet()->RowLengths(), rows_deltas,
             (Size().Height() - (rows - 1) * border_thickness).ToInt());
  LayOutAxis(cols_, FrameSet()->ColLengths(), cols_deltas,
             (Size().Width() - (cols - 1) * border_thickness).ToInt());

  PositionFrames();

  LayoutBox::UpdateLayout();

  cols_.allow_border_ = FrameSet()->AllowBorderColumns();
  rows_.allow_border_ = FrameSet()->AllowBorderRows();

  UpdateAfterLayout();

  ClearNeedsLayout();
}

static void ClearNeedsLayoutOnHiddenFrames(LayoutBox* frame) {
  for (; frame; frame = frame->NextSiblingBox()) {
    frame->SetWidth(LayoutUnit());
    frame->SetHeight(LayoutUnit());
    frame->ClearNeedsLayout();
    ClearNeedsLayoutOnHiddenFrames(frame->FirstChildBox());
  }
}

void LayoutFrameSet::PositionFrames() {
  NOT_DESTROYED();
  LayoutBox* child = FirstChildBox();
  if (!child)
    return;

  int rows = FrameSet()->TotalRows();
  int cols = FrameSet()->TotalCols();

  int border_thickness = FrameSet()->Border(StyleRef());
  LayoutSize size;
  LayoutPoint position;
  for (int r = 0; r < rows; r++) {
    position.SetX(LayoutUnit());
    size.SetHeight(LayoutUnit(rows_.sizes_[r]));
    for (int c = 0; c < cols; c++) {
      child->SetLocation(position);
      size.SetWidth(LayoutUnit(cols_.sizes_[c]));

      // If we have a new size, we need to resize and layout the child. If the
      // size is 0x0 we also need to lay out, since this may mean that we're
      // dealing with a child frameset that wasn't previously initialized
      // properly, because it was previously hidden, but no longer is, because
      // rows * cols may have increased.
      if (size != child->Size() || size.IsEmpty()) {
        child->SetSize(size);
        child->SetNeedsLayoutAndFullPaintInvalidation(
            layout_invalidation_reason::kSizeChanged);
        child->UpdateLayout();
      }

      position.SetX(position.X() + size.Width() + border_thickness);

      child = child->NextSiblingBox();
      if (!child)
        return;
    }
    position.SetY(position.Y() + size.Height() + border_thickness);
  }

  // All the remaining frames are hidden to avoid ugly spurious unflowed frames.
  ClearNeedsLayoutOnHiddenFrames(child);
}

bool LayoutFrameSet::IsChildAllowed(LayoutObject* child,
                                    const ComputedStyle&) const {
  NOT_DESTROYED();
  return child->IsFrame() || child->IsFrameSet();
}

void LayoutFrameSet::AddChild(LayoutObject* new_child,
                              LayoutObject* before_child) {
  LayoutBox::AddChild(new_child, before_child);
  FrameSet()->DirtyEdgeInfoAndFullPaintInvalidation();
}

void LayoutFrameSet::RemoveChild(LayoutObject* child) {
  LayoutBox::RemoveChild(child);
  if (!DocumentBeingDestroyed())
    FrameSet()->DirtyEdgeInfoAndFullPaintInvalidation();
}

CursorDirective LayoutFrameSet::GetCursor(const PhysicalOffset& point,
                                          ui::Cursor& cursor) const {
  NOT_DESTROYED();
  gfx::Point rounded_point = ToRoundedPoint(point);
  if (FrameSet()->CanResizeRow(rounded_point)) {
    cursor = RowResizeCursor();
    return kSetCursor;
  }
  if (FrameSet()->CanResizeColumn(rounded_point)) {
    cursor = ColumnResizeCursor();
    return kSetCursor;
  }
  return LayoutBox::GetCursor(point, cursor);
}

}  // namespace blink
