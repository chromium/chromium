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

// A helper that will encode a PNG image.
SK_API bool EncodePng(SkWStream* dst,
                      const SkPixmap& src,
                      const SkPngEncoder::Options& options);

// A helper that will create a PNG encoder.
SK_API std::unique_ptr<SkEncoder> MakePngEncoder(
    SkWStream* dst,
    const SkPixmap& src,
    const SkPngEncoder::Options& options);

}  // namespace skia

#endif  // SKIA_RUSTY_PNG_FEATURE_H_
