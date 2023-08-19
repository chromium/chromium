/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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

#include "third_party/blink/renderer/core/paint/clip_rect.h"

#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/platform/geometry/infinite_int_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/float_clip_rect.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

ClipRect::ClipRect()
    : rect_(InfiniteIntRect()), has_radius_(false), is_infinite_(true) {}

ClipRect::ClipRect(const FloatClipRect& rect) {
  SetRectInternal(rect);
}

void ClipRect::SetRect(const FloatClipRect& rect) {
  if (rect.IsInfinite() && IsInfinite())
    return;
  SetRectInternal(rect);
}

void ClipRect::SetRectInternal(const FloatClipRect& rect) {
  has_radius_ = rect.HasRadius();
  is_infinite_ = rect.IsInfinite();
  rect_ = PhysicalRect::FastAndLossyFromRectF(rect.Rect());
}

void ClipRect::SetRect(const PhysicalRect& rect) {
  rect_ = rect;
  has_radius_ = false;
  is_infinite_ = false;
}

void ClipRect::Intersect(const PhysicalRect& other) {
  if (IsInfinite()) {
    rect_ = other;
    is_infinite_ = false;
  } else {
    rect_.Intersect(other);
  }
}

void ClipRect::Intersect(const ClipRect& other) {
  if (other.IsInfinite())
    return;
  Intersect(other.Rect());
  if (other.HasRadius())
    has_radius_ = true;
}

bool ClipRect::Intersects(const HitTestLocation& hit_test_location) const {
  if (is_infinite_)
    return true;
  return hit_test_location.Intersects(rect_);
}

void ClipRect::Reset() {
  if (is_infinite_)
    return;
  has_radius_ = true;
  is_infinite_ = true;
  rect_ = PhysicalRect(InfiniteIntRect());
}

String ClipRect::ToString() const {
  return rect_.ToString() + (has_radius_ ? " hasRadius" : " noRadius") +
         (is_infinite_ ? " isInfinite" : " notInfinite");
}

std::ostream& operator<<(std::ostream& ostream, const ClipRect& rect) {
  return ostream << rect.ToString();
}

}  // namespace blink
