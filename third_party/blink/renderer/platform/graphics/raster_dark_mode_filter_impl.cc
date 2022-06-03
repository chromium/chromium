// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/raster_dark_mode_filter_impl.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"

namespace blink {

RasterDarkModeFilterImpl::RasterDarkModeFilterImpl(
    const DarkModeSettings& settings) {
  dark_mode_filter_ = std::make_unique<DarkModeFilter>(settings);
}

sk_sp<SkColorFilter> RasterDarkModeFilterImpl::ApplyToImage(
    const SkPixmap& pixmap,
    const SkIRect& src) const {
  return dark_mode_filter_->ApplyToImage(pixmap, src);
}

}  // namespace blink
