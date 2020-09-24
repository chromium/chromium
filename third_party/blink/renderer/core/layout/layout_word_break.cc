/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/layout/layout_word_break.h"

#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

LayoutWordBreak::LayoutWordBreak(HTMLElement* element)
    : LayoutText(element, StringImpl::empty_) {}

bool LayoutWordBreak::IsWordBreak() const {
  return true;
}

Position LayoutWordBreak::PositionForCaretOffset(unsigned offset) const {
  if (!GetNode())
    return Position();
  // The only allowed caret offset is 0, since LayoutWordBreak always has
  // |TextLength() == 0|.
  DCHECK_EQ(0u, offset) << offset;
  return Position::BeforeNode(*GetNode());
}

base::Optional<unsigned> LayoutWordBreak::CaretOffsetForPosition(
    const Position& position) const {
  if (position.IsNull() || position.AnchorNode() != GetNode())
    return base::nullopt;
  DCHECK(position.IsBeforeAnchor() || position.IsAfterAnchor());
  // The only allowed caret offset is 0, since LayoutWordBreak always has
  // |TextLength() == 0|.
  return 0;
}

}  // namespace blink
