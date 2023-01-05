// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/bitmap_utils.h"

#include "base/logging.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace headless {

bool CheckColoredRect(const SkBitmap& bitmap,
                      SkColor rect_color,
                      SkColor bkgr_color,
                      int margins) {
  gfx::Rect body(bitmap.width(), bitmap.height());
  if (margins) {
    body.Inset(margins);
  }

  // Build color rectangle by including every pixel with the specified
  // rectangle color into a rectangle.
  gfx::Rect rect;
  for (int y = body.y(); y < body.bottom(); y++) {
    for (int x = body.x(); x < body.right(); x++) {
      SkColor color = bitmap.getColor(x, y);
      if (color == rect_color) {
        gfx::Rect pixel_rect(x, y, 1, 1);
        if (rect.IsEmpty()) {
          rect = pixel_rect;
        } else {
          rect.Union(pixel_rect);
        }
      }
    }
  }

  // Verify that all pixels outside the found color rectangle are of
  // the specified background color, and the ones that are inside
  // the found rectangle are all of the rectangle color.
  for (int y = body.y(); y < body.bottom(); y++) {
    for (int x = body.x(); x < body.right(); x++) {
      SkColor color = bitmap.getColor(x, y);
      gfx::Point pt(x, y);
      if (rect.Contains(pt)) {
        if (color != rect_color) {
          LOG(ERROR) << "pt=" << pt.ToString() << " color=" << color
                     << ", expected rect color=" << rect_color;
          return false;
        }
      } else {
        if (color != bkgr_color) {
          LOG(ERROR) << "pt=" << pt.ToString() << " color=" << color
                     << ", expected bkgr color=" << bkgr_color;
          return false;
        }
      }
    }
  }

  return !rect.IsEmpty();
}

bool CheckColoredRect(const SkBitmap& bitmap,
                      SkColor rect_color,
                      SkColor bkgr_color) {
  return CheckColoredRect(bitmap, rect_color, bkgr_color, /*margins=*/0);
}

}  // namespace headless
