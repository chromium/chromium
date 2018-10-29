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

#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/geometry/length_point.h"
#include "third_party/blink/renderer/platform/geometry/length_size.h"

namespace blink {

int IntValueForLength(const Length& length, int maximum_value) {
  return ValueForLength(length, LayoutUnit(maximum_value)).ToInt();
}

float FloatValueForLength(const Length& length, float maximum_value) {
  switch (length.GetType()) {
    case kFixed:
      return length.GetFloatValue();
    case kPercent:
      return static_cast<float>(maximum_value * length.Percent() / 100.0f);
    case kFillAvailable:
    case kAuto:
      return static_cast<float>(maximum_value);
    case kCalculated:
      return length.NonNanCalculatedValue(LayoutUnit(maximum_value));
    case kMinContent:
    case kMaxContent:
    case kFitContent:
    case kExtendToZoom:
    case kDeviceWidth:
    case kDeviceHeight:
    case kMaxSizeNone:
      NOTREACHED();
      return 0;
  }
  NOTREACHED();
  return 0;
}

LayoutUnit MinimumValueForLength(const Length& length,
                                 LayoutUnit maximum_value) {
  switch (length.GetType()) {
    case kFixed:
      return LayoutUnit(length.Value());
    case kPercent:
      // Don't remove the extra cast to float. It is needed for rounding on
      // 32-bit Intel machines that use the FPU stack.
      return LayoutUnit(
          static_cast<float>(maximum_value * length.Percent() / 100.0f));
    case kCalculated:
      return LayoutUnit(length.NonNanCalculatedValue(maximum_value));
    case kFillAvailable:
    case kAuto:
      return LayoutUnit();
    case kMinContent:
    case kMaxContent:
    case kFitContent:
    case kExtendToZoom:
    case kDeviceWidth:
    case kDeviceHeight:
    case kMaxSizeNone:
      NOTREACHED();
      return LayoutUnit();
  }
  NOTREACHED();
  return LayoutUnit();
}

LayoutUnit RoundedMinimumValueForLength(const Length& length,
                                        LayoutUnit maximum_value) {
  if (length.GetType() == kPercent)
    return static_cast<LayoutUnit>(
        round(maximum_value * length.Percent() / 100.0f));
  return MinimumValueForLength(length, maximum_value);
}

LayoutUnit ValueForLength(const Length& length, LayoutUnit maximum_value) {
  switch (length.GetType()) {
    case kFixed:
    case kPercent:
    case kCalculated:
      return MinimumValueForLength(length, maximum_value);
    case kFillAvailable:
    case kAuto:
      return maximum_value;
    case kMinContent:
    case kMaxContent:
    case kFitContent:
    case kExtendToZoom:
    case kDeviceWidth:
    case kDeviceHeight:
    case kMaxSizeNone:
      NOTREACHED();
      return LayoutUnit();
  }
  NOTREACHED();
  return LayoutUnit();
}

FloatSize FloatSizeForLengthSize(const LengthSize& length_size,
                                 const FloatSize& box_size) {
  return FloatSize(
      FloatValueForLength(length_size.Width(), box_size.Width()),
      FloatValueForLength(length_size.Height(), box_size.Height()));
}

FloatPoint FloatPointForLengthPoint(const LengthPoint& length_point,
                                    const FloatSize& box_size) {
  return FloatPoint(FloatValueForLength(length_point.X(), box_size.Width()),
                    FloatValueForLength(length_point.Y(), box_size.Height()));
}

}  // namespace blink
