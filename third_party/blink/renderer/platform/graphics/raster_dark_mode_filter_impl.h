// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_RASTER_DARK_MODE_FILTER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_RASTER_DARK_MODE_FILTER_IMPL_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "cc/tiles/raster_dark_mode_filter.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// This class wraps DarkModeFilter to be used in compositor and creates the dark
// mode filter using dark mode settings passed. The DarkModeFilter is created on
// first use because building it compiles SkSL, and most renderers never
// rasterize anything in dark mode.
class PLATFORM_EXPORT RasterDarkModeFilterImpl
    : public cc::RasterDarkModeFilter {
 public:
  RasterDarkModeFilterImpl(const RasterDarkModeFilterImpl&) = delete;
  RasterDarkModeFilterImpl& operator=(const RasterDarkModeFilterImpl&) = delete;

  static RasterDarkModeFilterImpl& Instance();

  // RasterDarkModeFilter API.
  sk_sp<cc::ColorFilter> ApplyToImage(const SkPixmap& pixmap,
                                      const SkIRect& src) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(RasterDarkModeFilterImplTest, ApplyToImageAPI);

  explicit RasterDarkModeFilterImpl(const DarkModeSettings& settings);

  DarkModeFilter& GetDarkModeFilter() const;

  const DarkModeSettings settings_;
  mutable base::Lock lock_;
  mutable std::unique_ptr<DarkModeFilter> dark_mode_filter_ GUARDED_BY(lock_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_RASTER_DARK_MODE_FILTER_IMPL_H_
