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
#include "third_party/blink/renderer/core/html/html_dimension.h"
#include "third_party/blink/renderer/core/html/html_frame_set_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_frame.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/frame_set_painter.h"
#include "third_party/blink/renderer/platform/cursors.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"

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

LayoutFrameSet::LayoutFrameSet(HTMLFrameSetElement* frame_set)
    : LayoutBox(frame_set), is_resizing_(false) {
  SetInline(false);
}

LayoutFrameSet::~LayoutFrameSet() = default;

LayoutFrameSet::GridAxis::GridAxis() : split_being_resized_(kNoSplit) {}

HTMLFrameSetElement* LayoutFrameSet::FrameSet() const {
  return To<HTMLFrameSetElement>(GetNode());
}

void LayoutFrameSet::Paint(const PaintInfo& paint_info) const {
  FrameSetPainter(*this).Paint(paint_info);
}

void LayoutFrameSet::GridAxis::Resize(int size) {
  sizes_.resize(size);
  deltas_.resize(size);
  deltas_.Fill(0);

  // To track edges for resizability and borders, we need to be (size + 1). This
  // is because a parent frameset may ask us for information about our left/top/
  // right/bottom edges in order to make its own decisions about what to do. We
  // are capable of tainting that parent frameset's borders, so we have to cache
  // this info.
  prevent_resize_.resize(size + 1);
  allow_border_.resize(size + 1);
}

void LayoutFrameSet::LayOutAxis(GridAxis& axis,
                                const Vector<HTMLDimension>& grid,
                                int available_len) {
  available_len = max(available_len, 0);

  int* grid_layout = axis.sizes_.data();

  if (grid.IsEmpty()) {
    grid_layout[0] = available_len;
    return;
  }

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
      grid_layout[i] = max<int>(grid[i].Value() * effective_zoom, 0);
      total_fixed += grid_layout[i];
      count_fixed++;
    }

    // Count the total percentage of all of the percentage columns/rows ->
    // totalPercent. Count the number of columns/rows which are percentages ->
    // countPercent.
    if (grid[i].IsPercentage()) {
      grid_layout[i] = max<int>(grid[i].Value() * available_len / 100., 0);
      total_percent += grid_layout[i];
      count_percent++;
    }

    // Count the total relative of all the relative columns/rows ->
    // totalRelative. Count the number of columns/rows which are relative ->
    // countRelative.
    if (grid[i].IsRelative()) {
      total_relative += max<int>(grid[i].Value(), 1);
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
    int remaining_relative = remaining_len;

    for (int i = 0; i < grid_len; ++i) {
      if (grid[i].IsRelative()) {
        grid_layout[i] =
            (max(grid[i].Value(), 1.) * remaining_relative) / total_relative;
        remaining_len -= grid_layout[i];
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
  int* grid_delta = axis.deltas_.data();
  for (int i = 0; i < grid_len; ++i) {
    if (grid_layout[i] && grid_layout[i] + grid_delta[i] <= 0)
      worked = false;
    grid_layout[i] += grid_delta[i];
  }
  // if the deltas broke something, undo them
  if (!worked) {
    for (int i = 0; i < grid_len; ++i)
      grid_layout[i] -= grid_delta[i];
    axis.deltas_.Fill(0);
  }
}

void LayoutFrameSet::NotifyFrameEdgeInfoChanged() {
  if (NeedsLayout())
    return;
  // FIXME: We should only recompute the edge info with respect to the frame
  // that changed and its adjacent frame(s) instead of recomputing the edge info
  // for the entire frameset.
  ComputeEdgeInfo();
}

void LayoutFrameSet::FillFromEdgeInfo(const FrameEdgeInfo& edge_info,
                                      int r,
                                      int c) {
  if (edge_info.AllowBorder(kLeftFrameEdge))
    cols_.allow_border_[c] = true;
  if (edge_info.AllowBorder(kRightFrameEdge))
    cols_.allow_border_[c + 1] = true;
  if (edge_info.PreventResize(kLeftFrameEdge))
    cols_.prevent_resize_[c] = true;
  if (edge_info.PreventResize(kRightFrameEdge))
    cols_.prevent_resize_[c + 1] = true;

  if (edge_info.AllowBorder(kTopFrameEdge))
    rows_.allow_border_[r] = true;
  if (edge_info.AllowBorder(kBottomFrameEdge))
    rows_.allow_border_[r + 1] = true;
  if (edge_info.PreventResize(kTopFrameEdge))
    rows_.prevent_resize_[r] = true;
  if (edge_info.PreventResize(kBottomFrameEdge))
    rows_.prevent_resize_[r + 1] = true;
}

void LayoutFrameSet::ComputeEdgeInfo() {
  rows_.prevent_resize_.Fill(FrameSet()->NoResize());
  rows_.allow_border_.Fill(false);
  cols_.prevent_resize_.Fill(FrameSet()->NoResize());
  cols_.allow_border_.Fill(false);

  LayoutObject* child = FirstChild();
  if (!child)
    return;

  size_t rows = rows_.sizes_.size();
  size_t cols = cols_.sizes_.size();
  for (size_t r = 0; r < rows; ++r) {
    for (size_t c = 0; c < cols; ++c) {
      FrameEdgeInfo edge_info;
      if (child->IsFrameSet())
        edge_info = ToLayoutFrameSet(child)->EdgeInfo();
      else
        edge_info = ToLayoutFrame(child)->EdgeInfo();
      FillFromEdgeInfo(edge_info, r, c);
      child = child->NextSibling();
      if (!child)
        return;
    }
  }
}

FrameEdgeInfo LayoutFrameSet::EdgeInfo() const {
  FrameEdgeInfo result(FrameSet()->NoResize(), true);

  int rows = FrameSet()->TotalRows();
  int cols = FrameSet()->TotalCols();
  if (rows && cols) {
    result.SetPreventResize(kLeftFrameEdge, cols_.prevent_resize_[0]);
    result.SetAllowBorder(kLeftFrameEdge, cols_.allow_border_[0]);
    result.SetPreventResize(kRightFrameEdge, cols_.prevent_resize_[cols]);
    result.SetAllowBorder(kRightFrameEdge, cols_.allow_border_[cols]);
    result.SetPreventResize(kTopFrameEdge, rows_.prevent_resize_[0]);
    result.SetAllowBorder(kTopFrameEdge, rows_.allow_border_[0]);
    result.SetPreventResize(kBottomFrameEdge, rows_.prevent_resize_[rows]);
    result.SetAllowBorder(kBottomFrameEdge, rows_.allow_border_[rows]);
  }

  return result;
}

void LayoutFrameSet::UpdateLayout() {
  DCHECK(NeedsLayout());

  if (!Parent()->IsFrameSet() && !GetDocument().Printing()) {
    SetWidth(LayoutUnit(View()->ViewWidth()));
    SetHeight(LayoutUnit(View()->ViewHeight()));
  }

  unsigned cols = FrameSet()->TotalCols();
  unsigned rows = FrameSet()->TotalRows();

  if (rows_.sizes_.size() != rows || cols_.sizes_.size() != cols) {
    rows_.Resize(rows);
    cols_.Resize(cols);
  }

  LayoutUnit border_thickness(FrameSet()->Border());
  LayOutAxis(rows_, FrameSet()->RowLengths(),
             (Size().Height() - (rows - 1) * border_thickness).ToInt());
  LayOutAxis(cols_, FrameSet()->ColLengths(),
             (Size().Width() - (cols - 1) * border_thickness).ToInt());

  PositionFrames();

  LayoutBox::UpdateLayout();

  ComputeEdgeInfo();

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
  LayoutBox* child = FirstChildBox();
  if (!child)
    return;

  int rows = FrameSet()->TotalRows();
  int cols = FrameSet()->TotalCols();

  int border_thickness = FrameSet()->Border();
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

void LayoutFrameSet::StartResizing(GridAxis& axis, int position) {
  int split = HitTestSplit(axis, position);
  if (split == kNoSplit || axis.prevent_resize_[split]) {
    axis.split_being_resized_ = kNoSplit;
    return;
  }
  axis.split_being_resized_ = split;
  axis.split_resize_offset_ = position - SplitPosition(axis, split);
}

void LayoutFrameSet::ContinueResizing(GridAxis& axis, int position) {
  if (NeedsLayout())
    return;
  if (axis.split_being_resized_ == kNoSplit)
    return;
  int current_split_position = SplitPosition(axis, axis.split_being_resized_);
  int delta = (position - current_split_position) - axis.split_resize_offset_;
  if (!delta)
    return;
  axis.deltas_[axis.split_being_resized_ - 1] += delta;
  axis.deltas_[axis.split_being_resized_] -= delta;
  SetNeedsLayoutAndFullPaintInvalidation(
      layout_invalidation_reason::kSizeChanged);
}

bool LayoutFrameSet::UserResize(const MouseEvent& evt) {
  if (!is_resizing_) {
    if (NeedsLayout())
      return false;
    if (evt.type() == event_type_names::kMousedown &&
        evt.button() ==
            static_cast<int16_t>(WebPointerProperties::Button::kLeft)) {
      FloatPoint local_pos =
          AbsoluteToLocalFloatPoint(FloatPoint(evt.AbsoluteLocation()));
      StartResizing(cols_, local_pos.X());
      StartResizing(rows_, local_pos.Y());
      if (cols_.split_being_resized_ != kNoSplit ||
          rows_.split_being_resized_ != kNoSplit) {
        SetIsResizing(true);
        return true;
      }
    }
  } else {
    if (evt.type() == event_type_names::kMousemove ||
        (evt.type() == event_type_names::kMouseup &&
         evt.button() ==
             static_cast<int16_t>(WebPointerProperties::Button::kLeft))) {
      FloatPoint local_pos =
          AbsoluteToLocalFloatPoint(FloatPoint(evt.AbsoluteLocation()));
      ContinueResizing(cols_, local_pos.X());
      ContinueResizing(rows_, local_pos.Y());
      if (evt.type() == event_type_names::kMouseup &&
          evt.button() ==
              static_cast<int16_t>(WebPointerProperties::Button::kLeft)) {
        SetIsResizing(false);
        return true;
      }
    }
  }

  return false;
}

void LayoutFrameSet::SetIsResizing(bool is_resizing) {
  is_resizing_ = is_resizing;
  if (LocalFrame* frame = GetFrame()) {
    frame->GetEventHandler().SetResizingFrameSet(is_resizing ? FrameSet()
                                                             : nullptr);
  }
}

bool LayoutFrameSet::CanResizeRow(const IntPoint& p) const {
  int r = HitTestSplit(rows_, p.Y());
  return r != kNoSplit && !rows_.prevent_resize_[r];
}

bool LayoutFrameSet::CanResizeColumn(const IntPoint& p) const {
  int c = HitTestSplit(cols_, p.X());
  return c != kNoSplit && !cols_.prevent_resize_[c];
}

int LayoutFrameSet::SplitPosition(const GridAxis& axis, int split) const {
  if (NeedsLayout())
    return 0;

  int border_thickness = FrameSet()->Border();

  int size = axis.sizes_.size();
  if (!size)
    return 0;

  int position = 0;
  for (int i = 0; i < split && i < size; ++i)
    position += axis.sizes_[i] + border_thickness;
  return position - border_thickness;
}

int LayoutFrameSet::HitTestSplit(const GridAxis& axis, int position) const {
  if (NeedsLayout())
    return kNoSplit;

  int border_thickness = FrameSet()->Border();
  if (border_thickness <= 0)
    return kNoSplit;

  size_t size = axis.sizes_.size();
  if (!size)
    return kNoSplit;

  int split_position = axis.sizes_[0];
  for (size_t i = 1; i < size; ++i) {
    if (position >= split_position &&
        position < split_position + border_thickness)
      return i;
    split_position += border_thickness + axis.sizes_[i];
  }
  return kNoSplit;
}

bool LayoutFrameSet::IsChildAllowed(LayoutObject* child,
                                    const ComputedStyle&) const {
  return child->IsFrame() || child->IsFrameSet();
}

CursorDirective LayoutFrameSet::GetCursor(const PhysicalOffset& point,
                                          ui::Cursor& cursor) const {
  IntPoint rounded_point = RoundedIntPoint(point);
  if (CanResizeRow(rounded_point)) {
    cursor = RowResizeCursor();
    return kSetCursor;
  }
  if (CanResizeColumn(rounded_point)) {
    cursor = ColumnResizeCursor();
    return kSetCursor;
  }
  return LayoutBox::GetCursor(point, cursor);
}

}  // namespace blink
