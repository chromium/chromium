// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/geometry_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<gfx::mojom::RectFDataView, ::blink::WebFloatRect>::Read(
    gfx::mojom::RectFDataView data,
    ::blink::WebFloatRect* out) {
  if (data.width() < 0 || data.height() < 0)
    return false;
  out->x = data.x();
  out->y = data.y();
  out->width = data.width();
  out->height = data.height();
  return true;
}

// static
bool StructTraits<gfx::mojom::RectDataView, ::blink::WebRect>::Read(
    gfx::mojom::RectDataView data,
    ::blink::WebRect* out) {
  if (data.width() < 0 || data.height() < 0)
    return false;
  out->x = data.x();
  out->y = data.y();
  out->width = data.width();
  out->height = data.height();
  return true;
}

// static
bool StructTraits<gfx::mojom::PointDataView, ::blink::WebPoint>::Read(
    gfx::mojom::PointDataView data,
    ::blink::WebPoint* out) {
  out->x = data.x();
  out->y = data.y();
  return true;
}

// static
bool StructTraits<gfx::mojom::PointFDataView, ::blink::WebFloatPoint>::Read(
    gfx::mojom::PointFDataView data,
    ::blink::WebFloatPoint* out) {
  out->x = data.x();
  out->y = data.y();
  return true;
}

bool StructTraits<gfx::mojom::Point3FDataView, ::blink::FloatPoint3D>::Read(
    gfx::mojom::Point3FDataView data,
    ::blink::FloatPoint3D* out) {
  out->SetX(data.x());
  out->SetY(data.y());
  out->SetZ(data.z());
  return true;
}

// static
bool StructTraits<gfx::mojom::SizeDataView, ::blink::WebSize>::Read(
    gfx::mojom::SizeDataView data,
    ::blink::WebSize* out) {
  if (data.width() < 0 || data.height() < 0)
    return false;
  out->width = data.width();
  out->height = data.height();
  return true;
}

}  // namespace mojo
