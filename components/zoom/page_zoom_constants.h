// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZOOM_PAGE_ZOOM_CONSTANTS_H_
#define COMPONENTS_ZOOM_PAGE_ZOOM_CONSTANTS_H_

#include <stddef.h>
#include <string>

namespace zoom {

// Default zoom factors supported by ui_zoom.
extern const double kPresetZoomFactors[];
// Size of |kPresetZoomFactors|.
extern const std::size_t kPresetZoomFactorsSize;

std::string GetPresetZoomFactorsAsJSON();

}  // namespace zoom

#endif  // COMPONENTS_ZOOM_PAGE_ZOOM_CONSTANTS_H_
