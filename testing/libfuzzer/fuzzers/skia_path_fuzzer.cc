// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "testing/libfuzzer/fuzzers/skia_path_common.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathUtils.h"
#include "third_party/skia/include/core/SkSurface.h"

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> bytes) {
  base::SpanReader reader(bytes);

  uint8_t w, h, anti_alias;
  SkScalar a, b, c, d;
  if (!read(reader, &w, &h, &anti_alias, &a, &b, &c, &d)) {
    return 0;
  }

  // In this case, we specifically don't want to include kDone_Verb.
  const SkPath path = BuildPath(reader, SkPath::Verb::kClose_Verb);

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

  SkPath dst_path = skpathutils::FillPathWithPaint(path, paint_stroke);

  // Width and height should never be 0.
  auto surface(
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w ? w : 1, h ? h : 1)));

  surface->getCanvas()->drawPath(path, paint_fill);
  surface->getCanvas()->drawPath(path, paint_stroke);
  surface->getCanvas()->drawPath(dst_path, paint_fill);

  return 0;
}
