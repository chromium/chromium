/*
 * Copyright (c) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_SIZE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_SIZE_H_

#include <iosfwd>
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

// This class is deprecated.  Use PhysicalSize, gfx::Size, gfx::SizeF or
// LogicalSize instead.
class PLATFORM_EXPORT DeprecatedLayoutSize {
  DISALLOW_NEW();

 public:
  constexpr DeprecatedLayoutSize() = default;
  constexpr DeprecatedLayoutSize(LayoutUnit width, LayoutUnit height)
      : width_(width), height_(height) {}

  constexpr explicit DeprecatedLayoutSize(const gfx::SizeF& size)
      : width_(size.width()), height_(size.height()) {}

  constexpr explicit operator gfx::SizeF() const {
    return gfx::SizeF(width_.ToFloat(), height_.ToFloat());
  }

  // This is deleted to avoid unwanted lossy conversion from float or double to
  // LayoutUnit or int. Use explicit LayoutUnit constructor for each parameter
  // instead.
  DeprecatedLayoutSize(double, double) = delete;

  constexpr LayoutUnit Width() const { return width_; }
  constexpr LayoutUnit Height() const { return height_; }

  String ToString() const;

 private:
  LayoutUnit width_, height_;
};

inline DeprecatedLayoutSize RoundedLayoutSize(const gfx::SizeF& s) {
  return DeprecatedLayoutSize(s);
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&,
                                         const DeprecatedLayoutSize&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_LAYOUT_SIZE_H_
