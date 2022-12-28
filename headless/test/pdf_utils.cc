// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/pdf_utils.h"

#include "base/logging.h"
#include "pdf/pdf.h"
#include "printing/units.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace headless {

PDFPageBitmap::PDFPageBitmap() = default;
PDFPageBitmap::~PDFPageBitmap() = default;

bool PDFPageBitmap::Render(const std::string& pdf_data, int page_index) {
  auto pdf_span = base::make_span(
      reinterpret_cast<const uint8_t*>(pdf_data.data()), pdf_data.size());
  return Render(pdf_span, page_index);
}

bool PDFPageBitmap::Render(base::span<const uint8_t> pdf_data, int page_index) {
  absl::optional<gfx::SizeF> page_size_in_points =
      chrome_pdf::GetPDFPageSizeByIndex(pdf_data, page_index);
  if (!page_size_in_points) {
    return false;
  }

  gfx::SizeF page_size_in_pixels =
      gfx::ScaleSize(page_size_in_points.value(),
                     static_cast<float>(kDpi) / printing::kPointsPerInch);

  gfx::Rect page_rect(gfx::ToCeiledSize(page_size_in_pixels));

  constexpr chrome_pdf::RenderOptions options = {
      .stretch_to_bounds = false,
      .keep_aspect_ratio = true,
      .autorotate = true,
      .use_color = true,
      .render_device_type = chrome_pdf::RenderDeviceType::kPrinter,
  };

  bitmap_size_ = page_rect.size();
  bitmap_data_.resize(kColorChannels * bitmap_size_.GetArea());
  return chrome_pdf::RenderPDFPageToBitmap(pdf_data, page_index,
                                           bitmap_data_.data(), bitmap_size_,
                                           gfx::Size(kDpi, kDpi), options);
}

uint32_t PDFPageBitmap::GetPixelRGB(const gfx::Point& pt) const {
  return GetPixelRGB(pt.x(), pt.y());
}

uint32_t PDFPageBitmap::GetPixelRGB(int x, int y) const {
  CHECK_LT(x, bitmap_size_.width());
  CHECK_LT(y, bitmap_size_.height());

  int pixel_index =
      bitmap_size_.width() * y * kColorChannels + x * kColorChannels;
  return bitmap_data_[pixel_index + 0]             // B
         | (bitmap_data_[pixel_index + 1] << 8)    // G
         | (bitmap_data_[pixel_index + 2] << 16);  // R
}

bool PDFPageBitmap::CheckRect(uint32_t rect_color,
                              uint32_t bkgr_color,
                              int margins) {
  gfx::Rect body(bitmap_size_);
  if (margins) {
    body.Inset(margins);
  }

  // Build color rectangle by including every pixel with the specified
  // rectangle color into a rectangle.
  gfx::Rect rect;
  for (int y = body.y(); y < body.bottom(); y++) {
    for (int x = body.x(); x < body.right(); x++) {
      uint32_t color = GetPixelRGB(x, y);
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
      gfx::Point pt(x, y);
      uint32_t color = GetPixelRGB(pt);
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

  return true;
}

}  // namespace headless
