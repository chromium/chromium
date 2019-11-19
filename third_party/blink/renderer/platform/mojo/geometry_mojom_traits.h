// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_GEOMETRY_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_GEOMETRY_MOJOM_TRAITS_H_

#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "ui/gfx/geometry/mojom/geometry.mojom-blink.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::PointDataView, ::blink::WebPoint> {
  static int x(const ::blink::WebPoint& point) { return point.x; }
  static int y(const ::blink::WebPoint& point) { return point.y; }
  static bool Read(gfx::mojom::PointDataView, ::blink::WebPoint* out);
};

template <>
struct StructTraits<gfx::mojom::PointFDataView, ::blink::WebFloatPoint> {
  static float x(const ::blink::WebFloatPoint& point) { return point.x; }
  static float y(const ::blink::WebFloatPoint& point) { return point.y; }
  static bool Read(gfx::mojom::PointFDataView, ::blink::WebFloatPoint* out);
};

template <>
struct StructTraits<gfx::mojom::Point3FDataView, ::blink::FloatPoint3D> {
  static float x(const gfx::Point3F& p) { return p.x(); }
  static float y(const gfx::Point3F& p) { return p.y(); }
  static float z(const gfx::Point3F& p) { return p.z(); }
  static bool Read(gfx::mojom::Point3FDataView data,
                   ::blink::FloatPoint3D* out);
};

template <>
struct StructTraits<gfx::mojom::RectFDataView, ::blink::WebFloatRect> {
  static float x(const ::blink::WebFloatRect& rect) { return rect.x; }
  static float y(const ::blink::WebFloatRect& rect) { return rect.y; }
  static float width(const ::blink::WebFloatRect& rect) { return rect.width; }
  static float height(const ::blink::WebFloatRect& rect) { return rect.height; }
  static bool Read(gfx::mojom::RectFDataView, ::blink::WebFloatRect* out);
};

template <>
struct StructTraits<gfx::mojom::RectDataView, ::blink::WebRect> {
  static int x(const ::blink::WebRect& rect) { return rect.x; }
  static int y(const ::blink::WebRect& rect) { return rect.y; }
  static int width(const ::blink::WebRect& rect) { return rect.width; }
  static int height(const ::blink::WebRect& rect) { return rect.height; }
  static bool Read(gfx::mojom::RectDataView, ::blink::WebRect* out);
};

template <>
struct StructTraits<gfx::mojom::SizeDataView, ::blink::WebSize> {
  static int width(const ::blink::WebSize& size) { return size.width; }
  static int height(const ::blink::WebSize& size) { return size.height; }
  static bool Read(gfx::mojom::SizeDataView, ::blink::WebSize* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_GEOMETRY_MOJOM_TRAITS_H_
