// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/raster_dark_mode_filter_impl.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings_builder.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

// static
RasterDarkModeFilterImpl& RasterDarkModeFilterImpl::Instance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(RasterDarkModeFilterImpl, dark_mode_filter,
                                  (GetCurrentDarkModeSettings()));
  return dark_mode_filter;
}

RasterDarkModeFilterImpl::RasterDarkModeFilterImpl(
    const DarkModeSettings& settings)
    : dark_mode_filter_(std::make_unique<DarkModeFilter>(settings)) {}

sk_sp<cc::ColorFilter> RasterDarkModeFilterImpl::ApplyToImage(
    const SkPixmap& pixmap,
    const SkIRect& src) const {
  return dark_mode_filter_->GenerateImageFilter(pixmap, src);
}

}  // namespace blink
