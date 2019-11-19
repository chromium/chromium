/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
 */

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_INLINE_BOX_POSITION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_INLINE_BOX_POSITION_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

class InlineBox;
enum class UnicodeBidi : unsigned;

struct InlineBoxPosition {
  STACK_ALLOCATED();

 public:
  const InlineBox* inline_box;
  int offset_in_box;

  InlineBoxPosition() : inline_box(nullptr), offset_in_box(0) {}

  InlineBoxPosition(const InlineBox* inline_box, int offset_in_box)
      : inline_box(inline_box), offset_in_box(offset_in_box) {
    DCHECK(inline_box);
    DCHECK_GE(offset_in_box, 0);
  }

  bool operator==(const InlineBoxPosition& other) const {
    return inline_box == other.inline_box &&
           offset_in_box == other.offset_in_box;
  }

  bool operator!=(const InlineBoxPosition& other) const {
    return !operator==(other);
  }
};

CORE_EXPORT InlineBoxPosition
ComputeInlineBoxPosition(const PositionWithAffinity&);
CORE_EXPORT InlineBoxPosition
ComputeInlineBoxPosition(const PositionInFlatTreeWithAffinity&);
CORE_EXPORT InlineBoxPosition ComputeInlineBoxPosition(const VisiblePosition&);

PositionWithAffinity ComputeInlineAdjustedPosition(const PositionWithAffinity&);
PositionInFlatTreeWithAffinity ComputeInlineAdjustedPosition(
    const PositionInFlatTreeWithAffinity&);
PositionWithAffinity ComputeInlineAdjustedPosition(const VisiblePosition&);

InlineBoxPosition ComputeInlineBoxPositionForInlineAdjustedPosition(
    const PositionWithAffinity&);
InlineBoxPosition ComputeInlineBoxPositionForInlineAdjustedPosition(
    const PositionInFlatTreeWithAffinity&);

// The print for |InlineBoxPosition| is available only for testing
// in "blink_unittests", and implemented in
// "core/editing/inline_box_position_test.cc".
std::ostream& operator<<(std::ostream&, const InlineBoxPosition&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_INLINE_BOX_POSITION_H_
