// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/codec_utils.h"

#include "base/base64.h"
#include "base/check.h"
#include "skia/ext/skia_utils_base.h"
#include "skia/rusty_png_feature.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"

namespace skia {

namespace {

sk_sp<SkData> EncodePngAsSkData(const SkPixmap& src,
                                const SkPngEncoder::Options& options) {
  SkDynamicMemoryWStream stream;
  if (!skia::EncodePng(&stream, src, options)) {
    return nullptr;
  }
  return stream.detachAsData();
}

}  // namespace

sk_sp<SkData> EncodePngAsSkData(const SkPixmap& src) {
  const SkPngEncoder::Options kDefaultOptions = {};
  return EncodePngAsSkData(src, kDefaultOptions);
}

sk_sp<SkData> EncodePngAsSkData(GrDirectContext* context,
                                const SkImage* src,
                                int zlib_compression_level) {
  if (!src) {
    return nullptr;
  }

  sk_sp<SkImage> raster_image = src->makeRasterImage(context);
  if (!raster_image) {
    return nullptr;
  }

  SkPixmap pixmap;
  bool success = raster_image->peekPixels(&pixmap);

  // `peekPixels` should always succeed for raster images.
  CHECK(success);

  const SkPngEncoder::Options options = {.fZLibLevel = zlib_compression_level};
  return EncodePngAsSkData(pixmap, options);
}

sk_sp<SkData> EncodePngAsSkData(GrDirectContext* context, const SkImage* src) {
  // This is the default level in
  // `third_party/skia/include/encode/SkPngEncoder.h`.
  const int kDefaultZlibCompressionLevel = 6;

  return EncodePngAsSkData(context, src, kDefaultZlibCompressionLevel);
}

std::string EncodePngAsDataUri(const SkPixmap& src) {
  std::string result;
  if (sk_sp<SkData> data = EncodePngAsSkData(src); data) {
    result += "data:image/png;base64,";
    result += base::Base64Encode(skia::as_byte_span(*data));
  }
  return result;
}

}  // namespace skia
