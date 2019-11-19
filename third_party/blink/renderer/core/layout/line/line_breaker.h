/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 *               All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_LINE_BREAKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_LINE_BREAKER_H_

#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/line/inline_iterator.h"
#include "third_party/blink/renderer/core/layout/line/line_info.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

enum WhitespacePosition { kLeadingWhitespace, kTrailingWhitespace };

struct LayoutTextInfo;

class LineBreaker {
  STACK_ALLOCATED();

 public:
  friend class BreakingContext;
  LineBreaker(LineLayoutBlockFlow block) : block_(block) { Reset(); }

  InlineIterator NextLineBreak(InlineBidiResolver&,
                               LineInfo&,
                               LayoutTextInfo&,
                               WordMeasurements&);

  bool LineWasHyphenated() { return hyphenated_; }
  const Vector<LineLayoutBox>& PositionedObjects() {
    return positioned_objects_;
  }
  EClear Clear() { return clear_; }

 private:
  void Reset();

  void SkipLeadingWhitespace(InlineBidiResolver&, LineInfo&, LineWidth&);

  LineLayoutBlockFlow block_;
  bool hyphenated_;
  EClear clear_;
  Vector<LineLayoutBox> positioned_objects_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_LINE_BREAKER_H_
