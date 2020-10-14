// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/raster_dark_mode_filter_impl.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"

namespace blink {

RasterDarkModeFilterImpl::RasterDarkModeFilterImpl(
    const DarkModeSettings& settings) {
  dark_mode_filter_ = std::make_unique<DarkModeFilter>();
  dark_mode_filter_->UpdateSettings(settings);
}

cc::RasterDarkModeFilter::Result
RasterDarkModeFilterImpl::AnalyzeShouldApplyToImage(const SkIRect& src,
                                                    const SkIRect& dst) const {
  DarkModeResult dark_mode_result =
      dark_mode_filter_->AnalyzeShouldApplyToImage(src, dst);
  switch (dark_mode_result) {
    case DarkModeResult::kDoNotApplyFilter:
      return cc::RasterDarkModeFilter::Result::kDoNotApplyFilter;
    case DarkModeResult::kApplyFilter:
      return cc::RasterDarkModeFilter::Result::kApplyFilter;
    case DarkModeResult::kNotClassified:
      return cc::RasterDarkModeFilter::Result::kNotClassified;
  }
  NOTREACHED();
}

sk_sp<SkColorFilter> RasterDarkModeFilterImpl::ApplyToImage(
    const SkPixmap& pixmap,
    const SkIRect& src,
    const SkIRect& dst) const {
  return dark_mode_filter_->ApplyToImage(pixmap, src, dst);
}

sk_sp<SkColorFilter> RasterDarkModeFilterImpl::GetImageFilter() const {
  return dark_mode_filter_->GetImageFilter();
}

}  // namespace blink
