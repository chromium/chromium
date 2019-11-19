// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/markers/text_match_marker.h"

namespace blink {

TextMatchMarker::TextMatchMarker(unsigned start_offset,
                                 unsigned end_offset,
                                 MatchStatus status)
    : TextMarkerBase(start_offset, end_offset), match_status_(status) {}

DocumentMarker::MarkerType TextMatchMarker::GetType() const {
  return DocumentMarker::kTextMatch;
}

bool TextMatchMarker::IsActiveMatch() const {
  return match_status_ == MatchStatus::kActive;
}

void TextMatchMarker::SetIsActiveMatch(bool active) {
  match_status_ = active ? MatchStatus::kActive : MatchStatus::kInactive;
}

bool TextMatchMarker::IsRendered() const {
  return layout_status_ == LayoutStatus::kValidNotNull;
}

bool TextMatchMarker::Contains(const PhysicalOffset& point) const {
  DCHECK_EQ(layout_status_, LayoutStatus::kValidNotNull);
  return rect_.Contains(point);
}

void TextMatchMarker::SetRect(const PhysicalRect& rect) {
  if (layout_status_ == LayoutStatus::kValidNotNull && rect == rect_)
    return;
  layout_status_ = LayoutStatus::kValidNotNull;
  rect_ = rect;
}

const PhysicalRect& TextMatchMarker::GetRect() const {
  DCHECK_EQ(layout_status_, LayoutStatus::kValidNotNull);
  return rect_;
}

void TextMatchMarker::NullifyLayoutRect() {
  layout_status_ = LayoutStatus::kValidNull;
  // Now |rendered_rect_| can not be accessed until |SetRenderedRect| is
  // called.
}

void TextMatchMarker::Invalidate() {
  layout_status_ = LayoutStatus::kInvalid;
}

bool TextMatchMarker::IsValid() const {
  return layout_status_ != LayoutStatus::kInvalid;
}

}  // namespace blink
