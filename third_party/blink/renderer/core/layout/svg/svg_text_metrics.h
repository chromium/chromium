/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_TEXT_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_SVG_TEXT_METRICS_H_

#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class FloatSize;
enum class FontOrientation;

class SVGTextMetrics {
  DISALLOW_NEW();

 public:
  enum MetricsType { kSkippedSpaceMetrics };

  SVGTextMetrics(MetricsType);
  SVGTextMetrics(unsigned length, float width, float height);

  bool IsEmpty() const { return !width_ && !height_ && length_ <= 1; }

  FloatSize Extents() const;

  // TODO(kojii): We should store logical width (advance) and height instead
  // of storing physical and calculate logical. crbug.com/544767
  float Advance(FontOrientation) const;
  float Advance(bool is_vertical) const {
    return is_vertical ? height_ : width_;
  }
  unsigned length() const { return length_; }

 private:
  float width_;
  float height_;
  unsigned length_;
};

}  // namespace blink

#endif
