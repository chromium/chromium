/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/svg/svg_path_byte_stream_builder.h"

#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream.h"
#include "third_party/blink/renderer/core/svg/svg_path_data.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

// Helper class that coalesces writes to a SVGPathByteStream to a local buffer.
class SVGPathByteStreamBuilder::CoalescingBuffer {
 public:
  explicit CoalescingBuffer(SVGPathByteStreamBuilderStorage& result)
      : remaining_(bytes_), result_(result) {}
  ~CoalescingBuffer() { result_.AppendRange(bytes_, remaining_.data()); }

  template <size_t N>
  void WriteBytes(std::array<uint8_t, N> value) {
    remaining_.take_first<N>().copy_from(value);
  }

  void WriteFlag(bool value) { WriteBytes(base::U8ToNativeEndian(value)); }
  void WriteFloat(float value) { WriteBytes(base::FloatToNativeEndian(value)); }
  void WritePoint(const gfx::PointF& point) {
    WriteFloat(point.x());
    WriteFloat(point.y());
  }
  void WriteSegmentType(uint16_t value) {
    WriteBytes(base::U16ToNativeEndian(value));
  }

 private:
  // Adjust size to fit the largest command (in serialized/byte-stream format).
  // Currently a cubic segment.
  unsigned char bytes_[sizeof(uint16_t) + sizeof(gfx::PointF) * 3];
  // A span pointing to the unused part of `bytes_`.
  base::span<uint8_t> remaining_;
  SVGPathByteStreamBuilderStorage& result_;
};

SVGPathByteStreamBuilder::SVGPathByteStreamBuilder() = default;

void SVGPathByteStreamBuilder::EmitSegment(const PathSegmentData& segment) {
  CoalescingBuffer buffer(result_);
  buffer.WriteSegmentType(segment.command);

  switch (segment.command) {
    case kPathSegMoveToRel:
    case kPathSegMoveToAbs:
    case kPathSegLineToRel:
    case kPathSegLineToAbs:
    case kPathSegCurveToQuadraticSmoothRel:
    case kPathSegCurveToQuadraticSmoothAbs:
      buffer.WritePoint(segment.target_point);
      break;
    case kPathSegLineToHorizontalRel:
    case kPathSegLineToHorizontalAbs:
      buffer.WriteFloat(segment.target_point.x());
      break;
    case kPathSegLineToVerticalRel:
    case kPathSegLineToVerticalAbs:
      buffer.WriteFloat(segment.target_point.y());
      break;
    case kPathSegClosePath:
      break;
    case kPathSegCurveToCubicRel:
    case kPathSegCurveToCubicAbs:
      buffer.WritePoint(segment.point1);
      buffer.WritePoint(segment.point2);
      buffer.WritePoint(segment.target_point);
      break;
    case kPathSegCurveToCubicSmoothRel:
    case kPathSegCurveToCubicSmoothAbs:
      buffer.WritePoint(segment.point2);
      buffer.WritePoint(segment.target_point);
      break;
    case kPathSegCurveToQuadraticRel:
    case kPathSegCurveToQuadraticAbs:
      buffer.WritePoint(segment.point1);
      buffer.WritePoint(segment.target_point);
      break;
    case kPathSegArcRel:
    case kPathSegArcAbs:
      buffer.WritePoint(segment.point1);
      buffer.WriteFloat(segment.point2.x());
      buffer.WriteFlag(segment.arc_large);
      buffer.WriteFlag(segment.arc_sweep);
      buffer.WritePoint(segment.target_point);
      break;
    default:
      NOTREACHED();
  }
}

SVGPathByteStream SVGPathByteStreamBuilder::CopyByteStream() {
  return SVGPathByteStream(result_);
}

}  // namespace blink
