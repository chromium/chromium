// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/rusty_png_feature.h"

#include "base/feature_list.h"
#include "base/notreached.h"
#include "third_party/skia/experimental/rust_png/encoder/SkPngRustEncoder.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"

namespace skia {

namespace {

SkPngRustEncoder::Options ConvertToRustOptions(
    const SkPngEncoder::Options& options) {
  SkPngRustEncoder::Options rust_options;
  if (options.fZLibLevel < 4) {
    rust_options.fCompressionLevel = SkPngRustEncoder::CompressionLevel::kLow;
  } else if (options.fZLibLevel < 7) {
    rust_options.fCompressionLevel =
        SkPngRustEncoder::CompressionLevel::kMedium;
  } else {
    rust_options.fCompressionLevel = SkPngRustEncoder::CompressionLevel::kHigh;
  }
  rust_options.fComments = options.fComments;

  // TODO(https://crbug.com/379312510): Translate other `options` (e.g.
  // comments and/or ICC profile).

  return rust_options;
}

}  // namespace

BASE_FEATURE(kRustyPngFeature, "RustyPng", base::FEATURE_ENABLED_BY_DEFAULT);

bool EncodePng(SkWStream* dst,
               const SkPixmap& src,
               const SkPngEncoder::Options& options) {
  if (IsRustyPngEnabled()) {
    return SkPngRustEncoder::Encode(dst, src, ConvertToRustOptions(options));
  }

  return SkPngEncoder::Encode(dst, src, options);
}

std::unique_ptr<SkEncoder> MakePngEncoder(
    SkWStream* dst,
    const SkPixmap& src,
    const SkPngEncoder::Options& options) {
  if (IsRustyPngEnabled()) {
    return SkPngRustEncoder::Make(dst, src, ConvertToRustOptions(options));
  }

  return SkPngEncoder::Make(dst, src, options);
}

}  // namespace skia
