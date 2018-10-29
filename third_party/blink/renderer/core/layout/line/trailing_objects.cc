/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 *               All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2014 Adobe Systems Inc.
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

#include "third_party/blink/renderer/core/layout/line/trailing_objects.h"

#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/line/breaking_context_inline_headers.h"
#include "third_party/blink/renderer/core/layout/line/inline_iterator.h"

namespace blink {

void TrailingObjects::UpdateMidpointsForTrailingObjects(
    LineMidpointState& line_midpoint_state,
    const InlineIterator& l_break,
    CollapseFirstSpaceOrNot collapse_first_space) {
  if (!whitespace_)
    return;

  // This object is either going to be part of the last midpoint, or it is going
  // to be the actual endpoint. In both cases we just decrease our pos by 1
  // level to exclude the space, allowing it to - in effect - collapse into the
  // newline.
  if (line_midpoint_state.NumMidpoints() % 2) {
    // Find the trailing space object's midpoint.
    int trailing_space_midpoint = line_midpoint_state.NumMidpoints() - 1;
    for (; trailing_space_midpoint > 0 &&
           line_midpoint_state.Midpoints()[trailing_space_midpoint]
                   .GetLineLayoutItem() != whitespace_;
         --trailing_space_midpoint) {
    }
    DCHECK_GE(trailing_space_midpoint, 0);
    if (collapse_first_space == kCollapseFirstSpace)
      line_midpoint_state.Midpoints()[trailing_space_midpoint].SetOffset(
          line_midpoint_state.Midpoints()[trailing_space_midpoint].Offset() -
          1);

    // Now make sure every single trailingPositionedBox following the
    // trailingSpaceMidpoint properly stops and starts ignoring spaces.
    wtf_size_t current_midpoint = trailing_space_midpoint + 1;
    for (wtf_size_t i = 0; i < objects_.size(); ++i) {
      if (current_midpoint >= line_midpoint_state.NumMidpoints()) {
        // We don't have a midpoint for this box yet.
        EnsureLineBoxInsideIgnoredSpaces(&line_midpoint_state,
                                         LineLayoutItem(objects_[i]));
      } else {
        DCHECK(line_midpoint_state.Midpoints()[current_midpoint]
                   .GetLineLayoutItem() == objects_[i]);
        DCHECK(line_midpoint_state.Midpoints()[current_midpoint + 1]
                   .GetLineLayoutItem() == objects_[i]);
      }
      current_midpoint += 2;
    }
  } else if (!l_break.GetLineLayoutItem()) {
    DCHECK_EQ(collapse_first_space, kCollapseFirstSpace);
    // Add a new end midpoint that stops right at the very end.
    unsigned length = whitespace_.TextLength();
    unsigned pos = length >= 2 ? length - 2 : UINT_MAX;
    InlineIterator end_mid(nullptr, whitespace_, pos);
    line_midpoint_state.StartIgnoringSpaces(end_mid);
    for (const LineLayoutItem& item : objects_) {
      EnsureLineBoxInsideIgnoredSpaces(&line_midpoint_state, item);
    }
  }
}

}  // namespace blink
