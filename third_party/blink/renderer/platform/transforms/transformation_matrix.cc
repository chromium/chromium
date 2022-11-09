/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

#include <cmath>
#include <cstdlib>

#include "base/logging.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

TransformationMatrix::TransformationMatrix(const AffineTransform& t) {
  *this = Affine(t.A(), t.B(), t.C(), t.D(), t.E(), t.F());
}

LayoutRect TransformationMatrix::MapRect(const LayoutRect& r) const {
  return EnclosingLayoutRect(MapRect(gfx::RectF(r)));
}

AffineTransform TransformationMatrix::ToAffineTransform() const {
  return AffineTransform(rc(0, 0), rc(1, 0), rc(0, 1), rc(1, 1), rc(0, 3),
                         rc(1, 3));
}

SkM44 TransformationMatrix::ToSkM44() const {
  return SkM44(
      ClampToFloat(rc(0, 0)), ClampToFloat(rc(0, 1)), ClampToFloat(rc(0, 2)),
      ClampToFloat(rc(0, 3)), ClampToFloat(rc(1, 0)), ClampToFloat(rc(1, 1)),
      ClampToFloat(rc(1, 2)), ClampToFloat(rc(1, 3)), ClampToFloat(rc(2, 0)),
      ClampToFloat(rc(2, 1)), ClampToFloat(rc(2, 2)), ClampToFloat(rc(2, 3)),
      ClampToFloat(rc(3, 0)), ClampToFloat(rc(3, 1)), ClampToFloat(rc(3, 2)),
      ClampToFloat(rc(3, 3)));
}

std::ostream& operator<<(std::ostream& ostream,
                         const TransformationMatrix& transform) {
  return ostream << transform.ToDecomposedString();
}

}  // namespace blink
