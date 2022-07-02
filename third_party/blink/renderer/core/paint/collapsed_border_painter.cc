// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/collapsed_border_painter.h"

#include "third_party/blink/renderer/core/paint/block_painter.h"
#include "third_party/blink/renderer/core/paint/box_border_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/scoped_paint_state.h"
#include "third_party/blink/renderer/core/paint/table_cell_painter.h"

namespace blink {

void CollapsedBorderPainter::SetupBorders() {
  const auto* values = cell_.GetCollapsedBorderValues();
  DCHECK(values);
  if (values->StartBorder().IsVisible()) {
    start_.value = &values->StartBorder();
    start_.inner_width = cell_.CollapsedInnerBorderStart();
    start_.outer_width = cell_.CollapsedOuterBorderStart();
  } else {
    start_.value = nullptr;
  }

  if (values->EndBorder().IsVisible()) {
    end_.value = &values->EndBorder();
    end_.inner_width = cell_.CollapsedInnerBorderEnd();
    end_.outer_width = cell_.CollapsedOuterBorderEnd();
  } else {
    end_.value = nullptr;
  }

  if (values->BeforeBorder().IsVisible()) {
    before_.value = &values->BeforeBorder();
    before_.inner_width = cell_.CollapsedInnerBorderBefore();
    before_.outer_width = cell_.CollapsedOuterBorderBefore();
  } else {
    before_.value = nullptr;
  }

  if (values->AfterBorder().IsVisible()) {
    after_.value = &values->AfterBorder();
    after_.inner_width = cell_.CollapsedInnerBorderAfter();
    after_.outer_width = cell_.CollapsedOuterBorderAfter();
  } else {
    after_.value = nullptr;
  }

  // At first, let all borders paint the joints. This is to keep the current
  // behavior for web tests e.g. css2.1/t170602-bdr-conflict-w-01-d.html.
  // TODO(crbug.com/672216): Determine the best way to deal with this.
  if (start_.value && before_.value) {
    start_.begin_outset = before_.outer_width;
    before_.begin_outset = start_.outer_width;
  }
  if (end_.value && before_.value) {
    end_.begin_outset = before_.outer_width;
    before_.end_outset = end_.outer_width;
  }
  if (start_.value && after_.value) {
    start_.end_outset = after_.outer_width;
    after_.begin_outset = start_.outer_width;
  }
  if (after_.value && end_.value) {
    end_.end_outset = after_.outer_width;
    after_.end_outset = end_.outer_width;
  }

  // TODO(crbug.com/727173): We have a lot of bugs about mixed row direction.
  // Just bail out not to optimize duplicated borders.
  if (!cell_.Row()->HasSameDirectionAs(&table_) ||
      !cell_.Section()->HasSameDirectionAs(&table_))
    return;

  // Skip painting the start border if it will be painted by the preceding cell
  // as its end border.
  if (start_.value) {
    const auto* cell_preceding = table_.CellPreceding(cell_);
    if (cell_.StartsAtSameRow(cell_preceding) &&
        cell_preceding->ResolvedRowSpan() >= cell_.ResolvedRowSpan() &&
        // |cell_preceding| didn't paint the border if it is invisible.
        cell_preceding->StyleRef().Visibility() == EVisibility::kVisible) {
      start_.value = nullptr;
      // Otherwise we'll still paint the shared border twice which may cause
      // incorrect border conflict resolution for row/col spanning cells.
      // TODO(crbug.com/2902 etc.): Paint collapsed borders by grid cells.
    }
  }

  // Skip painting the before border if it will be painted by the above cell
  // as its after border. If we break page before the row with non-zero strut
  // (which means a gap between this row and the row above), or if we are
  // painting the top row of a footer that repeats on each page we need to paint
  // the before border separately.
  // TODO(crbug.com/751177) : This will double-paint the top border of the
  // footer on the last page.
  if (before_.value && !cell_.Row()->PaginationStrut()) {
    const auto* cell_above = table_.CellAbove(cell_);
    if (cell_.StartsAtSameColumn(cell_above) &&
        cell_above->ColSpan() >= cell_.ColSpan() &&
        // |cell_above| didn't paint the border if it is invisible.
        cell_above->StyleRef().Visibility() == EVisibility::kVisible &&
        cell_above->Row()->HasSameDirectionAs(&table_)) {
      bool cell_is_top_of_repeating_footer =
          cell_.Section()->IsRepeatingFooterGroup() &&
          (!cell_above || cell_above->Section() != cell_.Section());
      if (!cell_is_top_of_repeating_footer)
        before_.value = nullptr;
      // Otherwise we'll still paint the shared border twice which may cause
      // incorrect border conflict resolution for row/col spanning cells.
      // TODO(crbug.com/2902 etc.): Paint collapsed borders by grid cells.
    }
  }
}

static const CollapsedBorderValues* GetCollapsedBorderValues(
    const LayoutTableCell* cell) {
  return cell ? cell->GetCollapsedBorderValues() : nullptr;
}

void CollapsedBorderPainter::AdjustJoints() {
  // TODO(crbug.com/727173): We have a lot of bugs about mixed row direction.
  // Just bail out not to adjust the joints.
  if (!cell_.Row()->HasSameDirectionAs(&table_) ||
      !cell_.Section()->HasSameDirectionAs(&table_))
    return;

  // If we break page before the row with non-zero strut, we need to paint the
  // before border as if there is no cell above.
  const auto* cell_above =
      cell_.Row()->PaginationStrut() ? nullptr : table_.CellAbove(cell_);
  if (cell_above && (!cell_above->Row()->HasSameDirectionAs(&table_) ||
                     !cell_above->Section()->HasSameDirectionAs(&table_)))
    cell_above = nullptr;

  const auto* cell_below = table_.CellBelow(cell_);
  if (cell_below && (!cell_below->Row()->HasSameDirectionAs(&table_) ||
                     !cell_below->Section()->HasSameDirectionAs(&table_)))
    cell_above = nullptr;

  const auto* cell_preceding = table_.CellPreceding(cell_);
  const auto* cell_following = table_.CellFollowing(cell_);

  const auto* borders_preceding = GetCollapsedBorderValues(cell_preceding);
  const auto* borders_following = GetCollapsedBorderValues(cell_following);
  const auto* borders_above = GetCollapsedBorderValues(cell_above);
  const auto* borders_below = GetCollapsedBorderValues(cell_below);

  // These variables indicate whether |cell_| forms joints at the corners
  // with the borders of the adjacent cells. For example,
  // |before_start_adjoins_preceding| indicates that |cell_| shares the
  // before-start (logical top-left) corner with |cell_preceding_|.
  bool before_start_adjoins_preceding =
      borders_preceding && cell_.StartsAtSameRow(cell_preceding);
  bool before_start_adjoins_above =
      borders_above && cell_.StartsAtSameColumn(cell_above);
  bool before_end_adjoins_above =
      borders_above && cell_.EndsAtSameColumn(cell_above);
  bool before_end_adjoins_following =
      borders_following && cell_.StartsAtSameRow(cell_following);
  bool after_end_adjoins_following =
      borders_following && cell_.EndsAtSameRow(cell_following);
  bool after_end_adjoins_below =
      borders_below && cell_.EndsAtSameColumn(cell_below);
  bool after_start_adjoins_below =
      borders_below && cell_.StartsAtSameColumn(cell_below);
  bool after_start_adjoins_preceding =
      borders_preceding && cell_.EndsAtSameRow(cell_preceding);

  if (start_.value) {
    if (start_.value->CoversJoint(
            before_.value,
            before_start_adjoins_preceding ? &borders_preceding->BeforeBorder()
                                           : nullptr,
            before_start_adjoins_above ? &borders_above->StartBorder()
                                       : nullptr)) {
      if (before_start_adjoins_preceding) {
        start_.begin_outset = std::max<int>(
            start_.begin_outset, cell_preceding->CollapsedOuterBorderBefore());
      }
    } else {
      start_.begin_outset =
          -std::max<int>(cell_.CollapsedInnerBorderBefore(),
                         before_start_adjoins_preceding
                             ? cell_preceding->CollapsedInnerBorderBefore()
                             : 0);
    }
    if (start_.value->CoversJoint(
            after_.value,
            after_start_adjoins_preceding ? &borders_preceding->AfterBorder()
                                          : nullptr,
            after_start_adjoins_below ? &borders_below->StartBorder()
                                      : nullptr)) {
      if (after_start_adjoins_preceding) {
        start_.end_outset = std::max<int>(
            start_.end_outset, cell_preceding->CollapsedOuterBorderAfter());
      }
    } else {
      start_.end_outset =
          -std::max<int>(cell_.CollapsedInnerBorderAfter(),
                         after_start_adjoins_preceding
                             ? cell_preceding->CollapsedInnerBorderAfter()
                             : 0);
    }
  }

  if (end_.value) {
    if (end_.value->CoversJoint(
            before_.value,
            before_end_adjoins_following ? &borders_following->BeforeBorder()
                                         : nullptr,
            before_end_adjoins_above ? &borders_above->EndBorder() : nullptr)) {
      if (before_end_adjoins_following) {
        end_.begin_outset = std::max<int>(
            end_.begin_outset, cell_following->CollapsedOuterBorderBefore());
      }
    } else {
      end_.begin_outset =
          -std::max<int>(cell_.CollapsedInnerBorderBefore(),
                         before_end_adjoins_following
                             ? cell_following->CollapsedInnerBorderBefore()
                             : 0);
    }
    if (end_.value->CoversJoint(
            after_.value,
            after_end_adjoins_following ? &borders_following->AfterBorder()
                                        : nullptr,
            after_end_adjoins_below ? &borders_below->EndBorder() : nullptr)) {
      if (after_end_adjoins_following) {
        end_.end_outset = std::max<int>(
            end_.end_outset, cell_following->CollapsedOuterBorderAfter());
      }
    } else {
      end_.end_outset =
          -std::max<int>(cell_.CollapsedInnerBorderAfter(),
                         after_end_adjoins_following
                             ? cell_following->CollapsedInnerBorderAfter()
                             : 0);
    }
  }

  if (before_.value) {
    if (before_.value->CoversJoint(
            start_.value,
            before_start_adjoins_preceding ? &borders_preceding->BeforeBorder()
                                           : nullptr,
            before_start_adjoins_above ? &borders_above->StartBorder()
                                       : nullptr)) {
      if (before_start_adjoins_above) {
        before_.begin_outset = std::max<int>(
            before_.begin_outset, cell_above->CollapsedOuterBorderStart());
      }
    } else {
      before_.begin_outset = -std::max<int>(
          cell_.CollapsedInnerBorderStart(),
          before_start_adjoins_above ? cell_above->CollapsedInnerBorderStart()
                                     : 0);
    }
    if (before_.value->CoversJoint(
            end_.value,
            before_end_adjoins_following ? &borders_following->BeforeBorder()
                                         : nullptr,
            before_end_adjoins_above ? &borders_above->EndBorder() : nullptr)) {
      if (before_end_adjoins_above) {
        before_.end_outset = std::max<int>(
            before_.end_outset, cell_above->CollapsedOuterBorderEnd());
      }
    } else {
      before_.end_outset = -std::max<int>(
          cell_.CollapsedInnerBorderEnd(),
          before_end_adjoins_above ? cell_above->CollapsedInnerBorderEnd() : 0);
    }
  }

  if (after_.value) {
    if (after_.value->CoversJoint(
            start_.value,
            after_start_adjoins_preceding ? &borders_preceding->AfterBorder()
                                          : nullptr,
            after_start_adjoins_below ? &borders_below->StartBorder()
                                      : nullptr)) {
      if (after_start_adjoins_below) {
        after_.begin_outset = std::max<int>(
            after_.begin_outset, cell_below->CollapsedOuterBorderStart());
      }
    } else {
      after_.begin_outset = -std::max<int>(
          cell_.CollapsedInnerBorderStart(),
          after_start_adjoins_below ? cell_below->CollapsedInnerBorderStart()
                                    : 0);
    }
    if (after_.value->CoversJoint(
            end_.value,
            after_end_adjoins_following ? &borders_following->AfterBorder()
                                        : nullptr,
            after_end_adjoins_below ? &borders_below->EndBorder() : nullptr)) {
      if (after_end_adjoins_below) {
        after_.end_outset = std::max<int>(
            after_.end_outset, cell_below->CollapsedOuterBorderEnd());
      }
    } else {
      after_.end_outset = -std::max<int>(
          cell_.CollapsedInnerBorderEnd(),
          after_end_adjoins_below ? cell_below->CollapsedInnerBorderEnd() : 0);
    }
  }
}

void CollapsedBorderPainter::AdjustForWritingModeAndDirection() {
  const auto& style = cell_.TableStyle();
  if (!style.IsLeftToRightDirection()) {
    std::swap(start_, end_);
    std::swap(before_.begin_outset, before_.end_outset);
    std::swap(after_.begin_outset, after_.end_outset);
  }
  if (!style.IsHorizontalWritingMode()) {
    std::swap(after_, end_);
    std::swap(before_, start_);
    if (style.IsFlippedBlocksWritingMode()) {
      std::swap(start_, end_);
      std::swap(before_.begin_outset, before_.end_outset);
      std::swap(after_.begin_outset, after_.end_outset);
    }
  }
}

static EBorderStyle CollapsedBorderStyle(EBorderStyle style) {
  if (style == EBorderStyle::kOutset)
    return EBorderStyle::kGroove;
  if (style == EBorderStyle::kInset)
    return EBorderStyle::kRidge;
  return style;
}

void CollapsedBorderPainter::PaintCollapsedBorders(
    const PaintInfo& paint_info) {
  if (cell_.StyleRef().Visibility() != EVisibility::kVisible)
    return;

  if (!cell_.GetCollapsedBorderValues())
    return;

  GraphicsContext& context = paint_info.context;

  SetupBorders();
  if (table_.NeedsAdjustCollapsedBorderJoints())
    AdjustJoints();
  AdjustForWritingModeAndDirection();
  // Now left=start_, right=end_, before_=top, after_=bottom.

  // Collapsed borders are half inside and half outside of |rect|.
  ScopedPaintState paint_state(
      cell_, paint_info, /*painting_legacy_table_part_in_ancestor_layer*/ true);
  gfx::Rect rect = ToPixelSnappedRect(
      TableCellPainter(cell_).PaintRectNotIncludingVisualOverflow(
          paint_state.PaintOffset()));

  // We never paint diagonals at the joins.  We simply let the border with the
  // highest precedence paint on top of borders with lower precedence.
  if (before_.value) {
    gfx::Rect edge_rect(
        rect.x() - before_.begin_outset, rect.y() - before_.outer_width,
        rect.width() + before_.begin_outset + before_.end_outset,
        before_.outer_width + before_.inner_width);
    BoxBorderPainter::DrawBoxSide(
        context, edge_rect, BoxSide::kTop, before_.value->GetColor(),
        CollapsedBorderStyle(before_.value->Style()),
        BorderPaintAutoDarkMode(cell_.StyleRef(), before_.value->GetColor()));
  }
  if (after_.value) {
    gfx::Rect edge_rect(rect.x() - after_.begin_outset,
                        rect.bottom() - after_.inner_width,
                        rect.width() + after_.begin_outset + after_.end_outset,
                        after_.inner_width + after_.outer_width);
    BoxBorderPainter::DrawBoxSide(
        context, edge_rect, BoxSide::kBottom, after_.value->GetColor(),
        CollapsedBorderStyle(after_.value->Style()),
        BorderPaintAutoDarkMode(cell_.StyleRef(), after_.value->GetColor()));
  }
  if (start_.value) {
    gfx::Rect edge_rect(
        rect.x() - start_.outer_width, rect.y() - start_.begin_outset,
        start_.outer_width + start_.inner_width,
        rect.height() + start_.begin_outset + start_.end_outset);
    BoxBorderPainter::DrawBoxSide(
        context, edge_rect, BoxSide::kLeft, start_.value->GetColor(),
        CollapsedBorderStyle(start_.value->Style()),
        BorderPaintAutoDarkMode(cell_.StyleRef(), start_.value->GetColor()));
  }
  if (end_.value) {
    gfx::Rect edge_rect(rect.right() - end_.inner_width,
                        rect.y() - end_.begin_outset,
                        end_.inner_width + end_.outer_width,
                        rect.height() + end_.begin_outset + end_.end_outset);
    BoxBorderPainter::DrawBoxSide(
        context, edge_rect, BoxSide::kRight, end_.value->GetColor(),
        CollapsedBorderStyle(end_.value->Style()),
        BorderPaintAutoDarkMode(cell_.StyleRef(), end_.value->GetColor()));
  }
}

}  // namespace blink
