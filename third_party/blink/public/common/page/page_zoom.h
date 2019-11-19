// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_PAGE_ZOOM_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_PAGE_ZOOM_H_

#include "third_party/blink/public/common/common_export.h"

namespace blink {

// The minimum and maximum page zoom factors that are allowed.
BLINK_COMMON_EXPORT extern const double kMinimumPageZoomFactor;
BLINK_COMMON_EXPORT extern const double kMaximumPageZoomFactor;

// Convert between page zoom factors and levels.
BLINK_COMMON_EXPORT double PageZoomLevelToZoomFactor(double zoom_level);
BLINK_COMMON_EXPORT double PageZoomFactorToZoomLevel(double factor);

// Use this to compare page zoom factors and levels. It accounts for precision
// loss due to conversions back and forth.
BLINK_COMMON_EXPORT bool PageZoomValuesEqual(double value_a, double value_b);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_PAGE_ZOOM_H_
