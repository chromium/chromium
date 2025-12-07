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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PATH_BYTE_STREAM_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PATH_BYTE_STREAM_SOURCE_H_

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/core/svg/svg_path_byte_stream.h"
#include "third_party/blink/renderer/core/svg/svg_path_data.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

class SVGPathByteStreamSource {
  STACK_ALLOCATED();

 public:
  explicit SVGPathByteStreamSource(const SVGPathByteStream& stream)
      : stream_(stream.Span()) {}
  SVGPathByteStreamSource(const SVGPathByteStreamSource&) = delete;
  SVGPathByteStreamSource& operator=(const SVGPathByteStreamSource&) = delete;

  bool HasMoreData() const { return !stream_.empty(); }
  PathSegmentData ParseSegment();

 private:
  bool ReadFlag() { return stream_.take_first<1u>()[0]; }

  float ReadFloat() {
    return base::FloatFromNativeEndian(stream_.take_first<sizeof(float)>());
  }

  uint16_t ReadSVGSegmentType() {
    return base::U16FromNativeEndian(stream_.take_first<sizeof(uint16_t)>());
  }

  gfx::PointF ReadPoint() {
    float x = ReadFloat();
    float y = ReadFloat();
    return gfx::PointF(x, y);
  }

  base::span<const uint8_t> stream_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PATH_BYTE_STREAM_SOURCE_H_
