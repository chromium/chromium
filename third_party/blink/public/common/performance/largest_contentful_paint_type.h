// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_LARGEST_CONTENTFUL_PAINT_TYPE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_LARGEST_CONTENTFUL_PAINT_TYPE_H_

namespace blink {
// This enum contains the various types a potential LargestContentfulPaint
// candidate entry may have.
// These values are set in PaintTimingDetector, packed into
// page_load_metrics.mojom's LargestContentfulPaintTiming.type and finally
// reported to UKM through UKMPageLoadMetricsObserver.
enum LargestContentfulPaintType : uint32_t {
  // kImage and KText are not yet supported and will be added later.
  kLCPTypeImage = 1 << 0,
  kLCPTypeText = 1 << 1,

  kLCPTypeAnimatedImage = 1 << 2,
  // The enum values below are not yet used and will be added later.
  kLCPTypeVideo = 1 << 3,
  kLCPTypeDataURI = 1 << 4,
  kLCPTypePNG = 1 << 5,
  kLCPTypeJPG = 1 << 6,
  kLCPTypeWebP = 1 << 7,
  kLCPTypeSVG = 1 << 8,
  kLCPTypeGIF = 1 << 9,
  kLCPTypeAVIF = 1 << 10,
  kLCPTypeFullViewport = 1 << 11,
  kLCPTypeAfterMouseover = 1 << 12,
};

using LargestContentfulPaintTypeMask = uint32_t;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_LARGEST_CONTENTFUL_PAINT_TYPE_H_
