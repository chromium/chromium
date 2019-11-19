/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2005 Nokia.  All rights reserved.
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

#include "third_party/blink/renderer/platform/geometry/float_point.h"

#include <math.h>

#include <algorithm>
#include <limits>

#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkPoint.h"

namespace blink {

float FloatPoint::SlopeAngleRadians() const {
  return atan2f(y_, x_);
}

float FloatPoint::length() const {
  return hypotf(x_, y_);
}

FloatPoint FloatPoint::ExpandedTo(const FloatPoint& other) const {
  return FloatPoint(std::max(x_, other.x_), std::max(y_, other.y_));
}

FloatPoint FloatPoint::ShrunkTo(const FloatPoint& other) const {
  return FloatPoint(std::min(x_, other.x_), std::min(y_, other.y_));
}

FloatPoint FloatPoint::NarrowPrecision(double x, double y) {
  return FloatPoint(clampTo<float>(x), clampTo<float>(y));
}

bool FindIntersection(const FloatPoint& p1,
                      const FloatPoint& p2,
                      const FloatPoint& d1,
                      const FloatPoint& d2,
                      FloatPoint& intersection) {
  float px_length = p2.X() - p1.X();
  float py_length = p2.Y() - p1.Y();

  float dx_length = d2.X() - d1.X();
  float dy_length = d2.Y() - d1.Y();

  float denom = px_length * dy_length - py_length * dx_length;
  if (!denom)
    return false;

  float param =
      ((d1.X() - p1.X()) * dy_length - (d1.Y() - p1.Y()) * dx_length) / denom;

  intersection.SetX(p1.X() + param * px_length);
  intersection.SetY(p1.Y() + param * py_length);
  return true;
}

std::ostream& operator<<(std::ostream& ostream, const FloatPoint& point) {
  return ostream << point.ToString();
}

String FloatPoint::ToString() const {
  return String::Format("%lg,%lg", X(), Y());
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const FloatPoint& p) {
  ts << "(" << WTF::TextStream::FormatNumberRespectingIntegers(p.X());
  ts << "," << WTF::TextStream::FormatNumberRespectingIntegers(p.Y());
  ts << ")";
  return ts;
}

}  // namespace blink
