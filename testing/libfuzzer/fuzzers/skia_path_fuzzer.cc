// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "testing/libfuzzer/fuzzers/skia_path_common.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathUtils.h"
#include "third_party/skia/include/core/SkSurface.h"


extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  uint8_t w, h, anti_alias;
  if (!read<uint8_t>(&data, &size, &w))
    return 0;
  if (!read<uint8_t>(&data, &size, &h))
    return 0;
  if (!read<uint8_t>(&data, &size, &anti_alias))
    return 0;

  SkScalar a, b, c, d;
  if (!read<SkScalar>(&data, &size, &a))
    return 0;
  if (!read<SkScalar>(&data, &size, &b))
    return 0;
  if (!read<SkScalar>(&data, &size, &c))
    return 0;
  if (!read<SkScalar>(&data, &size, &d))
    return 0;

  // In this case, we specifically don't want to include kDone_Verb.
  SkPath path;
  BuildPath(&data, &size, &path, SkPath::Verb::kClose_Verb);

  // Try a few potentially interesting things with our path.
  path.contains(a, b);
  path.conservativelyContainsRect(SkRect::MakeLTRB(a, b, c, d));

  SkPaint paint_fill;
  paint_fill.setStyle(SkPaint::Style::kFill_Style);
  paint_fill.setAntiAlias(anti_alias & 1);

  SkPaint paint_stroke;
  paint_stroke.setStyle(SkPaint::Style::kStroke_Style);
  paint_stroke.setStrokeWidth(1);
  paint_stroke.setAntiAlias(anti_alias & 1);

  SkPath dst_path;
  skpathutils::FillPathWithPaint(path, paint_stroke, &dst_path, nullptr);

  // Width and height should never be 0.
  auto surface(
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w ? w : 1, h ? h : 1)));

  surface->getCanvas()->drawPath(path, paint_fill);
  surface->getCanvas()->drawPath(path, paint_stroke);

  return 0;
}
