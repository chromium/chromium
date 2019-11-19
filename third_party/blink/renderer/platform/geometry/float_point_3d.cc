/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"

#include <math.h>

#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/point3_f.h"

namespace blink {

FloatPoint3D::FloatPoint3D(const gfx::Point3F& point)
    : x_(point.x()), y_(point.y()), z_(point.z()) {}

void FloatPoint3D::Normalize() {
  float temp_length = length();

  if (temp_length) {
    x_ /= temp_length;
    y_ /= temp_length;
    z_ /= temp_length;
  }
}

float FloatPoint3D::AngleBetween(const FloatPoint3D& y) const {
  float x_length = this->length();
  float y_length = y.length();

  if (x_length && y_length) {
    float cos_angle = this->Dot(y) / (x_length * y_length);
    // Due to round-off |cosAngle| can have a magnitude greater than 1.  Clamp
    // the value to [-1, 1] before computing the angle.
    return acos(clampTo(cos_angle, -1.0, 1.0));
  }
  return 0;
}

std::ostream& operator<<(std::ostream& ostream, const FloatPoint3D& point) {
  return ostream << point.ToString();
}

String FloatPoint3D::ToString() const {
  return String::Format("%lg,%lg,%lg", X(), Y(), Z());
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const FloatPoint3D& p) {
  ts << "x=" << p.X() << " y=" << p.Y() << " z=" << p.Z();
  return ts;
}

}  // namespace blink
