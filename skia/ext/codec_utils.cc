// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/codec_utils.h"

#include "base/base64.h"
#include "skia/ext/skia_utils_base.h"
#include "skia/rusty_png_feature.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"

namespace skia {

sk_sp<SkData> EncodePngAsSkData(const SkPixmap& src) {
  SkDynamicMemoryWStream stream;
  const SkPngEncoder::Options kDefaultOptions = {};
  if (!skia::EncodePng(&stream, src, kDefaultOptions)) {
    return nullptr;
  }
  return stream.detachAsData();
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
