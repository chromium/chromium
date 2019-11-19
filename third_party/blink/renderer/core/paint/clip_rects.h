/*
 * Copyright (C) 2003, 2009, 2012 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_CLIP_RECTS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_CLIP_RECTS_H_

#include "third_party/blink/renderer/core/paint/clip_rect.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class ClipRects : public RefCounted<ClipRects> {
  USING_FAST_MALLOC(ClipRects);

 public:
  static scoped_refptr<ClipRects> Create() {
    return base::AdoptRef(new ClipRects);
  }
  static scoped_refptr<ClipRects> Create(const ClipRects& other) {
    return base::AdoptRef(new ClipRects(other));
  }

  ClipRects() : fixed_(0) {}

  void Reset(const PhysicalRect& r) {
    overflow_clip_rect_ = r;
    fixed_clip_rect_ = r;
    pos_clip_rect_ = r;
    fixed_ = 0;
  }

  const ClipRect& OverflowClipRect() const { return overflow_clip_rect_; }
  void SetOverflowClipRect(const ClipRect& r) { overflow_clip_rect_ = r; }

  const ClipRect& FixedClipRect() const { return fixed_clip_rect_; }
  void SetFixedClipRect(const ClipRect& r) { fixed_clip_rect_ = r; }

  const ClipRect& PosClipRect() const { return pos_clip_rect_; }
  void SetPosClipRect(const ClipRect& r) { pos_clip_rect_ = r; }

  bool Fixed() const { return static_cast<bool>(fixed_); }
  void SetFixed(bool fixed) { fixed_ = fixed ? 1 : 0; }

  bool operator==(const ClipRects& other) const {
    return overflow_clip_rect_ == other.OverflowClipRect() &&
           fixed_clip_rect_ == other.FixedClipRect() &&
           pos_clip_rect_ == other.PosClipRect() && Fixed() == other.Fixed();
  }

  bool operator!=(const ClipRects& other) const { return !(*this == other); }

  ClipRects& operator=(const ClipRects& other) {
    overflow_clip_rect_ = other.OverflowClipRect();
    fixed_clip_rect_ = other.FixedClipRect();
    pos_clip_rect_ = other.PosClipRect();
    fixed_ = other.Fixed();
    return *this;
  }

 private:
  ClipRects(const PhysicalRect& r)
      : overflow_clip_rect_(r),
        fixed_clip_rect_(r),
        pos_clip_rect_(r),
        fixed_(0) {}

  ClipRects(const ClipRects& other)
      : overflow_clip_rect_(other.OverflowClipRect()),
        fixed_clip_rect_(other.FixedClipRect()),
        pos_clip_rect_(other.PosClipRect()),
        fixed_(other.Fixed()) {}

  ClipRect overflow_clip_rect_;
  ClipRect fixed_clip_rect_;
  ClipRect pos_clip_rect_;
  unsigned fixed_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_CLIP_RECTS_H_
