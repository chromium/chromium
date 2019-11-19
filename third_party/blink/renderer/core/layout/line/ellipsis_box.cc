/**
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
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
 */

#include "third_party/blink/renderer/core/layout/line/ellipsis_box.h"

#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/layout/text_run_constructor.h"
#include "third_party/blink/renderer/core/paint/ellipsis_box_painter.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/text/text_run.h"

namespace blink {

void EllipsisBox::Paint(const PaintInfo& paint_info,
                        const LayoutPoint& paint_offset,
                        LayoutUnit line_top,
                        LayoutUnit line_bottom) const {
  EllipsisBoxPainter(*this).Paint(paint_info, paint_offset, line_top,
                                  line_bottom);
}

IntRect EllipsisBox::SelectionRect() const {
  const ComputedStyle& style = GetLineLayoutItem().StyleRef(IsFirstLineStyle());
  const Font& font = style.GetFont();
  return EnclosingIntRect(font.SelectionRectForText(
      ConstructTextRun(font, str_, style, TextRun::kAllowTrailingExpansion),
      FloatPoint(LogicalLeft().ToInt(),
                 (LogicalTop() + Root().SelectionTop()).ToInt()),
      Root().SelectionHeight().ToInt()));
}

bool EllipsisBox::NodeAtPoint(HitTestResult& result,
                              const HitTestLocation& hit_test_location,
                              const PhysicalOffset& accumulated_offset,
                              LayoutUnit line_top,
                              LayoutUnit line_bottom) {
  PhysicalOffset adjusted_location = accumulated_offset + PhysicalLocation();
  PhysicalRect bounds_rect(adjusted_location, Size());
  if (VisibleToHitTestRequest(result.GetHitTestRequest()) &&
      bounds_rect.Intersects(
          HitTestLocation::RectForPoint(hit_test_location.Point()))) {
    GetLineLayoutItem().UpdateHitTestResult(
        result, hit_test_location.Point() - adjusted_location);
    if (result.AddNodeToListBasedTestResult(GetLineLayoutItem().GetNode(),
                                            hit_test_location,
                                            bounds_rect) == kStopHitTesting)
      return true;
  }

  return false;
}

const char* EllipsisBox::BoxName() const {
  return "EllipsisBox";
}

}  // namespace blink
