// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/raster_dark_mode_filter_impl.h"

#include <memory>

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings_builder.h"

namespace blink {

// static
RasterDarkModeFilterImpl& RasterDarkModeFilterImpl::Instance() {
  static base::NoDestructor<RasterDarkModeFilterImpl> dark_mode_filter(
      GetCurrentDarkModeSettings());
  return *dark_mode_filter;
}

RasterDarkModeFilterImpl::RasterDarkModeFilterImpl(
    const DarkModeSettings& settings)
    : settings_(settings) {}

sk_sp<cc::ColorFilter> RasterDarkModeFilterImpl::ApplyToImage(
    const SkPixmap& pixmap,
    const SkIRect& src) const {
  return GetDarkModeFilter().GenerateImageFilter(pixmap, src);
}

DarkModeFilter& RasterDarkModeFilterImpl::GetDarkModeFilter() const {
  base::AutoLock lock(lock_);
  if (!dark_mode_filter_) {
    dark_mode_filter_ = std::make_unique<DarkModeFilter>(settings_);
  }
  return *dark_mode_filter_;
}

}  // namespace blink
