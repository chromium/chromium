// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/image.h"

#include <stdint.h>

#include <algorithm>

#include "base/files/file_util.h"
#include "base/hash/md5.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "printing/metafile.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"

namespace printing {

Image::Image(const Metafile& metafile) : row_length_(0), ignore_alpha_(true) {
  LoadMetafile(metafile);
}

Image::Image(const Image& image) = default;

Image::~Image() = default;

std::string Image::checksum() const {
  base::MD5Digest digest;
  base::MD5Sum(&data_[0], data_.size(), &digest);
  return base::MD5DigestToBase16(digest);
}

bool Image::SaveToPng(const base::FilePath& filepath) const {
  DCHECK(!data_.empty());
  std::vector<unsigned char> compressed;
  bool success = gfx::PNGCodec::Encode(
      &*data_.begin(), gfx::PNGCodec::FORMAT_BGRA, size_, row_length_, true,
      std::vector<gfx::PNGCodec::Comment>(), &compressed);
  DCHECK(success && compressed.size());
  if (success) {
    int write_bytes =
        base::WriteFile(filepath, reinterpret_cast<char*>(&*compressed.begin()),
                        base::checked_cast<int>(compressed.size()));
    success = (write_bytes == static_cast<int>(compressed.size()));
    DCHECK(success);
  }
  return success;
}

double Image::PercentageDifferent(const Image& rhs) const {
  if (size_.width() == 0 || size_.height() == 0 || rhs.size_.width() == 0 ||
      rhs.size_.height() == 0)
    return 100.;

  int width = std::min(size_.width(), rhs.size_.width());
  int height = std::min(size_.height(), rhs.size_.height());
  // Compute pixels different in the overlap
  int pixels_different = 0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      uint32_t lhs_pixel = pixel_at(x, y);
      uint32_t rhs_pixel = rhs.pixel_at(x, y);
      if (lhs_pixel != rhs_pixel)
        ++pixels_different;
    }

    // Look for extra right lhs pixels. They should be white.
    for (int x = width; x < size_.width(); ++x) {
      uint32_t lhs_pixel = pixel_at(x, y);
      if (lhs_pixel != Color(SK_ColorWHITE))
        ++pixels_different;
    }

    // Look for extra right rhs pixels. They should be white.
    for (int x = width; x < rhs.size_.width(); ++x) {
      uint32_t rhs_pixel = rhs.pixel_at(x, y);
      if (rhs_pixel != Color(SK_ColorWHITE))
        ++pixels_different;
    }
  }

  // Look for extra bottom lhs pixels. They should be white.
  for (int y = height; y < size_.height(); ++y) {
    for (int x = 0; x < size_.width(); ++x) {
      uint32_t lhs_pixel = pixel_at(x, y);
      if (lhs_pixel != Color(SK_ColorWHITE))
        ++pixels_different;
    }
  }

  // Look for extra bottom rhs pixels. They should be white.
  for (int y = height; y < rhs.size_.height(); ++y) {
    for (int x = 0; x < rhs.size_.width(); ++x) {
      uint32_t rhs_pixel = rhs.pixel_at(x, y);
      if (rhs_pixel != Color(SK_ColorWHITE))
        ++pixels_different;
    }
  }

  // Like the WebKit ImageDiff tool, we define percentage different in terms
  // of the size of the 'actual' bitmap.
  double total_pixels =
      static_cast<double>(size_.width()) * static_cast<double>(height);
  return static_cast<double>(pixels_different) / total_pixels * 100.;
}

bool Image::LoadPng(const std::string& compressed) {
  int w;
  int h;
  bool success = gfx::PNGCodec::Decode(
      reinterpret_cast<const unsigned char*>(compressed.c_str()),
      compressed.size(), gfx::PNGCodec::FORMAT_BGRA, &data_, &w, &h);
  size_.SetSize(w, h);
  row_length_ = size_.width() * sizeof(uint32_t);
  return success;
}

}  // namespace printing
