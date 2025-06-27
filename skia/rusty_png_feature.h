// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_RUSTY_PNG_FEATURE_H_
#define SKIA_RUSTY_PNG_FEATURE_H_

#include <memory>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "third_party/skia/include/encode/SkEncoder.h"

class SkPixmap;
class SkWStream;

namespace SkPngEncoder {
struct Options;
}  // namespace SkPngEncoder

namespace skia {

// Exposing the feature so that chrome://flags and tests can inspect it and turn
// it on/off, but product code should instead just call `IsRustyPngEnabled`.
SK_API BASE_DECLARE_FEATURE(kRustyPngFeature);

// Returns true if Rust should be used for PNG decoding.
//
// See also  https://crbug.com/40278281 and the "Rollout plan" in
// https://docs.google.com/document/d/1glx5ue5JDlCld5WzWgTOGK3wsMErQFnkY5N5Dsbi91Y
inline bool IsRustyPngEnabled() {
  return base::FeatureList::IsEnabled(kRustyPngFeature);
}

// A helper that will encode a PNG image using either the `libpng`-based
// `SkPngEncoder::Encode` API, or (if `kRustyPngFeature` is built and enabled)
// the Rust-based `SkPngRustEncoder::Encode` API.
SK_API bool EncodePng(SkWStream* dst,
                      const SkPixmap& src,
                      const SkPngEncoder::Options& options);

// A helper that will create either a `libpng`-based, or a Rust-based PNG
// encoder (depending on whether the `kRustyPngFeature` is built and enabled).
SK_API std::unique_ptr<SkEncoder> MakePngEncoder(
    SkWStream* dst,
    const SkPixmap& src,
    const SkPngEncoder::Options& options);

}  // namespace skia

#endif  // SKIA_RUSTY_PNG_FEATURE_H_
