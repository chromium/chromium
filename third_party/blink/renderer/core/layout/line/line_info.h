/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
                 All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_LINE_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_LINE_INFO_H_

#include "third_party/blink/renderer/core/layout/line/line_width.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LineInfo {
  STACK_ALLOCATED();

 public:
  LineInfo()
      : is_first_line_(true),
        is_last_line_(false),
        is_empty_(true),
        previous_line_broke_cleanly_(true),
        runs_from_leading_whitespace_(0),
        text_align_(ETextAlign::kLeft) {}

  bool IsFirstLine() const { return is_first_line_; }
  bool IsLastLine() const { return is_last_line_; }
  bool IsEmpty() const { return is_empty_; }
  bool PreviousLineBrokeCleanly() const { return previous_line_broke_cleanly_; }
  unsigned RunsFromLeadingWhitespace() const {
    return runs_from_leading_whitespace_;
  }
  void ResetRunsFromLeadingWhitespace() { runs_from_leading_whitespace_ = 0; }
  void IncrementRunsFromLeadingWhitespace() { runs_from_leading_whitespace_++; }

  void SetFirstLine(bool first_line) { is_first_line_ = first_line; }
  void SetLastLine(bool last_line) { is_last_line_ = last_line; }
  void SetEmpty(bool empty) { is_empty_ = empty; }

  void SetPreviousLineBrokeCleanly(bool previous_line_broke_cleanly) {
    previous_line_broke_cleanly_ = previous_line_broke_cleanly;
  }

  ETextAlign GetTextAlign() const { return text_align_; }
  void SetTextAlign(ETextAlign text_align) { text_align_ = text_align; }

 private:
  bool is_first_line_;
  bool is_last_line_;
  bool is_empty_;
  bool previous_line_broke_cleanly_;
  unsigned runs_from_leading_whitespace_;
  ETextAlign text_align_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_LINE_INFO_H_
