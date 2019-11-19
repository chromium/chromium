/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_INT_RECT_OUTSETS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_INT_RECT_OUTSETS_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Specifies integral lengths to be used to expand a rectangle.
// For example, |top()| returns the distance the top edge should be moved
// upward.
//
// Negative lengths can be used to express insets.
class PLATFORM_EXPORT IntRectOutsets {
  DISALLOW_NEW();

 public:
  constexpr IntRectOutsets() : top_(0), right_(0), bottom_(0), left_(0) {}

  constexpr IntRectOutsets(int top, int right, int bottom, int left)
      : top_(top), right_(right), bottom_(bottom), left_(left) {}

  constexpr int Top() const { return top_; }
  constexpr int Right() const { return right_; }
  constexpr int Bottom() const { return bottom_; }
  constexpr int Left() const { return left_; }

  void SetTop(int top) { top_ = top; }
  void SetRight(int right) { right_ = right; }
  void SetBottom(int bottom) { bottom_ = bottom; }
  void SetLeft(int left) { left_ = left; }

  constexpr bool IsZero() const {
    return !Left() && !Right() && !Top() && !Bottom();
  }

 private:
  int top_;
  int right_;
  int bottom_;
  int left_;
};

inline void operator+=(IntRectOutsets& a, const IntRectOutsets& b) {
  a.SetTop(a.Top() + b.Top());
  a.SetRight(a.Right() + b.Right());
  a.SetBottom(a.Bottom() + b.Bottom());
  a.SetLeft(a.Left() + b.Left());
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_INT_RECT_OUTSETS_H_
