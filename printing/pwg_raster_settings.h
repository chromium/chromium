// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PWG_RASTER_SETTINGS_H_
#define PRINTING_PWG_RASTER_SETTINGS_H_

#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"

namespace printing {

enum PwgRasterTransformType {
  TRANSFORM_NORMAL,
  TRANSFORM_ROTATE_180,
  TRANSFORM_FLIP_HORIZONTAL,
  TRANSFORM_FLIP_VERTICAL,
  TRANSFORM_TYPE_LAST = TRANSFORM_FLIP_VERTICAL
};

struct PwgRasterSettings {
  mojom::DuplexMode duplex_mode;
  // How to transform odd-numbered pages.
  PwgRasterTransformType odd_page_transform;
  // Rotate all pages (on top of odd-numbered page transform).
  bool rotate_all_pages;
  // Rasterize pages in reverse order.
  bool reverse_page_order;
  // Rasterize pages in color.
  bool use_color;
};

}  // namespace printing

#endif  // PRINTING_PWG_RASTER_SETTINGS_H_
