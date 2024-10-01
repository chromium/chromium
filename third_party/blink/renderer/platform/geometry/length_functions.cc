/*
    Copyright (C) 1999 Lars Knoll (knoll@kde.org)
    Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
    Copyright (C) 2011 Rik Cabanier (cabanier@adobe.com)
    Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
    Copyright (C) 2012 Motorola Mobility, Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "third_party/blink/renderer/platform/geometry/length_functions.h"

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/geometry/length_point.h"
#include "third_party/blink/renderer/platform/geometry/length_size.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

int IntValueForLength(const Length& length, int maximum_value) {
  return ValueForLength(length, LayoutUnit(maximum_value)).ToInt();
}

float FloatValueForLength(const Length& length,
                          float maximum_value,
                          const EvaluationInput& input) {
  switch (length.GetType()) {
    case Length::kFixed:
      return length.GetFloatValue();
    case Length::kPercent:
      return ClampTo<float>(maximum_value * length.Percent() / 100.0f);
    case Length::kStretch:
    case Length::kAuto:
      return static_cast<float>(maximum_value);
    case Length::kCalculated:
      return length.NonNanCalculatedValue(maximum_value, input);
    case Length::kMinContent:
    case Length::kMaxContent:
    case Length::kMinIntrinsic:
    case Length::kFitContent:
    case Length::kContent:
    case Length::kFlex:
    case Length::kExtendToZoom:
    case Length::kDeviceWidth:
    case Length::kDeviceHeight:
    case Length::kNone:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}

LayoutUnit MinimumValueForLengthInternal(const Length& length,
                                         LayoutUnit maximum_value,
                                         const EvaluationInput& input) {
  switch (length.GetType()) {
    case Length::kPercent:
      // Don't remove the extra cast to float. It is needed for rounding on
      // 32-bit Intel machines that use the FPU stack.
      return LayoutUnit(
          static_cast<float>(maximum_value * length.Percent() / 100.0f));
    case Length::kCalculated:
      return LayoutUnit(length.NonNanCalculatedValue(maximum_value, input));
    case Length::kStretch:
    case Length::kAuto:
      return LayoutUnit();
    case Length::kFixed:
    case Length::kMinContent:
    case Length::kMaxContent:
    case Length::kMinIntrinsic:
    case Length::kFitContent:
    case Length::kContent:
    case Length::kFlex:
    case Length::kExtendToZoom:
    case Length::kDeviceWidth:
    case Length::kDeviceHeight:
    case Length::kNone:
      NOTREACHED_IN_MIGRATION();
      return LayoutUnit();
  }
  NOTREACHED_IN_MIGRATION();
  return LayoutUnit();
}

LayoutUnit ValueForLength(const Length& length,
                          LayoutUnit maximum_value,
                          const EvaluationInput& input) {
  switch (length.GetType()) {
    case Length::kFixed:
    case Length::kPercent:
    case Length::kCalculated:
      return MinimumValueForLength(length, maximum_value, input);
    case Length::kStretch:
    case Length::kAuto:
      return maximum_value;
    case Length::kMinContent:
    case Length::kMaxContent:
    case Length::kMinIntrinsic:
    case Length::kFitContent:
    case Length::kContent:
    case Length::kFlex:
    case Length::kExtendToZoom:
    case Length::kDeviceWidth:
    case Length::kDeviceHeight:
    case Length::kNone:
      NOTREACHED_IN_MIGRATION();
      return LayoutUnit();
  }
  NOTREACHED_IN_MIGRATION();
  return LayoutUnit();
}

gfx::SizeF SizeForLengthSize(const LengthSize& length_size,
                             const gfx::SizeF& box_size) {
  return gfx::SizeF(
      FloatValueForLength(length_size.Width(), box_size.width()),
      FloatValueForLength(length_size.Height(), box_size.height()));
}

gfx::PointF PointForLengthPoint(const LengthPoint& length_point,
                                const gfx::SizeF& box_size) {
  return gfx::PointF(FloatValueForLength(length_point.X(), box_size.width()),
                     FloatValueForLength(length_point.Y(), box_size.height()));
}

}  // namespace blink
