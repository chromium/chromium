/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PATH_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PATH_BUILDER_H_

#include "third_party/blink/renderer/core/svg/svg_path_consumer.h"
#include "third_party/blink/renderer/core/svg/svg_path_data.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

class Path;

class SVGPathBuilder final : public SVGPathConsumer {
 public:
  SVGPathBuilder(Path& path) : path_(path), last_command_(kPathSegUnknown) {}

  void EmitSegment(const PathSegmentData&) override;

 private:
  void EmitClose();
  void EmitMoveTo(const gfx::PointF&);
  void EmitLineTo(const gfx::PointF&);
  void EmitQuadTo(const gfx::PointF&, const gfx::PointF&);
  void EmitSmoothQuadTo(const gfx::PointF&);
  void EmitCubicTo(const gfx::PointF&, const gfx::PointF&, const gfx::PointF&);
  void EmitSmoothCubicTo(const gfx::PointF&, const gfx::PointF&);
  void EmitArcTo(const gfx::PointF&,
                 float radius_x,
                 float radius_y,
                 float rotate,
                 bool large_arc,
                 bool sweep);

  gfx::PointF SmoothControl(bool is_smooth) const;

  Path& path_;

  SVGPathSegType last_command_;
  gfx::PointF subpath_point_;
  gfx::PointF current_point_;
  gfx::PointF last_control_point_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PATH_BUILDER_H_
