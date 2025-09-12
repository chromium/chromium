// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/codec_utils.h"

#include "base/base64.h"
#include "base/check.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/skia/include/codec/SkCodec.h"
#include "third_party/skia/include/codec/SkPngDecoder.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/encode/SkPngRustEncoder.h"

namespace skia {

namespace {

sk_sp<SkData> EncodePngAsSkData(const SkPixmap& src,
                                const SkPngRustEncoder::Options& options) {
  SkDynamicMemoryWStream stream;
  if (!SkPngRustEncoder::Encode(&stream, src, options)) {
    return nullptr;
  }
  return stream.detachAsData();
}

sk_sp<SkData> EncodePngAsSkData(
    GrDirectContext* context,
    const SkImage* src,
    SkPngRustEncoder::CompressionLevel compression_level) {
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

  const SkPngRustEncoder::Options options = {.fCompressionLevel =
                                                 compression_level};
  return EncodePngAsSkData(pixmap, options);
}

}  // namespace

sk_sp<SkData> EncodePngAsSkData(const SkPixmap& src) {
  const SkPngRustEncoder::Options kDefaultOptions = {};
  return EncodePngAsSkData(src, kDefaultOptions);
}

sk_sp<SkData> EncodePngAsSkData(GrDirectContext* context, const SkImage* src) {
  return EncodePngAsSkData(context, src,
                           SkPngRustEncoder::CompressionLevel::kMedium);
}

sk_sp<SkData> FastEncodePngAsSkData(GrDirectContext* context,
                                    const SkImage* src) {
  return EncodePngAsSkData(context, src,
                           SkPngRustEncoder::CompressionLevel::kLow);
}

std::string EncodePngAsDataUri(const SkPixmap& src) {
  std::string result;
  if (sk_sp<SkData> data = EncodePngAsSkData(src); data) {
    result += "data:image/png;base64,";
    result += base::Base64Encode(skia::as_byte_span(*data));
  }
  return result;
}

void EnsurePNGDecoderRegistered() {
  SkCodecs::Register(SkPngDecoder::Decoder());
}

}  // namespace skia
