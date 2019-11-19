/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2005 Nokia.  All rights reserved.
 *               2008 Eric Seidel <eric@webkit.org>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_SIZE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_SIZE_H_

#include <iosfwd>

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/geometry/int_point.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

#if defined(OS_MACOSX)
typedef struct CGSize CGSize;

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif
#endif

namespace blink {

class PLATFORM_EXPORT FloatSize {
  DISALLOW_NEW();

 public:
  constexpr FloatSize() : width_(0), height_(0) {}
  constexpr FloatSize(float width, float height)
      : width_(width), height_(height) {}
  constexpr explicit FloatSize(const IntSize& s)
      : FloatSize(s.Width(), s.Height()) {}
  constexpr explicit FloatSize(const gfx::SizeF& s)
      : FloatSize(s.width(), s.height()) {}
  explicit FloatSize(const SkSize& s) : FloatSize(s.width(), s.height()) {}
  // We also have conversion operator to FloatSize defined in LayoutSize.

  static FloatSize NarrowPrecision(double width, double height);

  constexpr float Width() const { return width_; }
  constexpr float Height() const { return height_; }

  constexpr void SetWidth(float width) { width_ = width; }
  constexpr void SetHeight(float height) { height_ = height; }

  constexpr bool IsEmpty() const { return width_ <= 0 || height_ <= 0; }
  constexpr bool IsZero() const {
    // Not using fabs as it is not a constexpr in LLVM libc++
    return -std::numeric_limits<float>::epsilon() < width_ &&
           width_ < std::numeric_limits<float>::epsilon() &&
           -std::numeric_limits<float>::epsilon() < height_ &&
           height_ < std::numeric_limits<float>::epsilon();
  }
  bool IsValid() const {
    return width_ != -std::numeric_limits<float>::infinity() &&
           height_ != -std::numeric_limits<float>::infinity();
  }
  bool IsExpressibleAsIntSize() const;

  float AspectRatio() const { return width_ / height_; }

  float Area() const { return width_ * height_; }

  void Expand(float width, float height) {
    width_ += width;
    height_ += height;
  }

  void Scale(float s) { Scale(s, s); }

  void Scale(float scale_x, float scale_y) {
    width_ *= scale_x;
    height_ *= scale_y;
  }

  void ScaleAndFloor(float scale) {
    width_ = floorf(width_ * scale);
    height_ = floorf(height_ * scale);
  }

  FloatSize ExpandedTo(const FloatSize& other) const {
    return FloatSize(width_ > other.width_ ? width_ : other.width_,
                     height_ > other.height_ ? height_ : other.height_);
  }

  FloatSize ShrunkTo(const FloatSize& other) const {
    return FloatSize(width_ < other.width_ ? width_ : other.width_,
                     height_ < other.height_ ? height_ : other.height_);
  }

  void ClampNegativeToZero() { *this = ExpandedTo(FloatSize()); }

  float DiagonalLength() const;
  float DiagonalLengthSquared() const {
    return width_ * width_ + height_ * height_;
  }

  FloatSize TransposedSize() const { return FloatSize(height_, width_); }

  FloatSize ScaledBy(float scale) const { return ScaledBy(scale, scale); }

  FloatSize ScaledBy(float scale_x, float scale_y) const {
    return FloatSize(width_ * scale_x, height_ * scale_y);
  }

#if defined(OS_MACOSX)
  explicit FloatSize(
      const CGSize&);  // don't do this implicitly since it's lossy
  operator CGSize() const;
#endif

  explicit operator SkSize() const { return SkSize::Make(width_, height_); }
  // Use this only for logical sizes, which can not be negative. Things that are
  // offsets instead, and can be negative, should use a gfx::Vector2dF.
  constexpr explicit operator gfx::SizeF() const {
    return gfx::SizeF(width_, height_);
  }
  // FloatSize is used as an offset, which can be negative, but gfx::SizeF can
  // not. The Vector2dF type is used for offsets instead.
  constexpr explicit operator gfx::Vector2dF() const {
    return gfx::Vector2dF(width_, height_);
  }

  String ToString() const;

 private:
  float width_, height_;

  friend struct ::WTF::DefaultHash<blink::FloatSize>;
  friend struct ::WTF::HashTraits<blink::FloatSize>;
};

inline FloatSize& operator+=(FloatSize& a, const FloatSize& b) {
  a.SetWidth(a.Width() + b.Width());
  a.SetHeight(a.Height() + b.Height());
  return a;
}

constexpr FloatSize& operator-=(FloatSize& a, const FloatSize& b) {
  a.SetWidth(a.Width() - b.Width());
  a.SetHeight(a.Height() - b.Height());
  return a;
}

constexpr FloatSize operator+(const FloatSize& a, const FloatSize& b) {
  return FloatSize(a.Width() + b.Width(), a.Height() + b.Height());
}

constexpr FloatSize operator-(const FloatSize& a, const FloatSize& b) {
  return FloatSize(a.Width() - b.Width(), a.Height() - b.Height());
}

constexpr FloatSize operator-(const FloatSize& size) {
  return FloatSize(-size.Width(), -size.Height());
}

constexpr FloatSize operator*(const FloatSize& a, const float b) {
  return FloatSize(a.Width() * b, a.Height() * b);
}

constexpr FloatSize operator*(const float a, const FloatSize& b) {
  return FloatSize(a * b.Width(), a * b.Height());
}

constexpr bool operator==(const FloatSize& a, const FloatSize& b) {
  return a.Width() == b.Width() && a.Height() == b.Height();
}

constexpr bool operator!=(const FloatSize& a, const FloatSize& b) {
  return !(a == b);
}

inline IntSize RoundedIntSize(const FloatSize& p) {
  return IntSize(clampTo<int>(roundf(p.Width())),
                 clampTo<int>(roundf(p.Height())));
}

inline IntSize FlooredIntSize(const FloatSize& p) {
  return IntSize(clampTo<int>(floorf(p.Width())),
                 clampTo<int>(floorf(p.Height())));
}

inline IntSize ExpandedIntSize(const FloatSize& p) {
  return IntSize(clampTo<int>(ceilf(p.Width())),
                 clampTo<int>(ceilf(p.Height())));
}

inline IntPoint FlooredIntPoint(const FloatSize& p) {
  return IntPoint(clampTo<int>(floorf(p.Width())),
                  clampTo<int>(floorf(p.Height())));
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const FloatSize&);
PLATFORM_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const FloatSize&);

}  // namespace blink

// Allows this class to be stored in a HeapVector.
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::FloatSize)

namespace WTF {

template <>
struct DefaultHash<blink::FloatSize> {
  STATIC_ONLY(DefaultHash);
  struct Hash {
    STATIC_ONLY(Hash);
    typedef typename IntTypes<sizeof(float)>::UnsignedType Bits;
    static unsigned GetHash(const blink::FloatSize& key) {
      return HashInts(bit_cast<Bits>(key.Width()),
                      bit_cast<Bits>(key.Height()));
    }
    static bool Equal(const blink::FloatSize& a, const blink::FloatSize& b) {
      return bit_cast<Bits>(a.Width()) == bit_cast<Bits>(b.Width()) &&
             bit_cast<Bits>(a.Height()) == bit_cast<Bits>(b.Height());
    }
    static const bool safe_to_compare_to_empty_or_deleted = true;
  };
};

template <>
struct HashTraits<blink::FloatSize> : GenericHashTraits<blink::FloatSize> {
  STATIC_ONLY(HashTraits);
  static const bool kEmptyValueIsZero = false;
  static blink::FloatSize EmptyValue() {
    return blink::FloatSize(std::numeric_limits<float>::infinity(),
                            std::numeric_limits<float>::infinity());
  }
  static void ConstructDeletedValue(blink::FloatSize& slot, bool) {
    slot = blink::FloatSize(-std::numeric_limits<float>::infinity(),
                            -std::numeric_limits<float>::infinity());
  }
  static bool IsDeletedValue(const blink::FloatSize& value) {
    return !value.IsValid();
  }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_FLOAT_SIZE_H_
