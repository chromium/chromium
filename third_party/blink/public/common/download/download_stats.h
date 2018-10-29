// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_DOWNLOAD_DOWNLOAD_STATS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_DOWNLOAD_DOWNLOAD_STATS_H_

#include "base/macros.h"
#include "third_party/blink/common/common_export.h"

namespace blink {

class BLINK_COMMON_EXPORT DownloadStats {
 public:
  enum class FrameType {
    kMainFrame,
    kSameOriginAdSubframe,
    kSameOriginNonAdSubframe,
    kCrossOriginAdSubframe,
    kCrossOriginNonAdSubframe,
  };

  enum class GestureType {
    kWithoutGesture,
    kWithGesture,
  };

  // Note that these values are reported in UMA. So entries should never be
  // renumbered, and numeric values should never be reused.
  enum class FrameGesture {
    kMainFrameNoGesture = 0,
    kMainFrameGesture = 1,
    kSameOriginAdSubframeNoGesture = 2,
    kSameOriginAdSubframeGesture = 3,
    kSameOriginNonAdSubframeNoGesture = 4,
    kSameOriginNonAdSubframeGesture = 5,
    kCrossOriginAdSubframeNoGesture = 6,
    kCrossOriginAdSubframeGesture = 7,
    kCrossOriginNonAdSubframeNoGesture = 8,
    kCrossOriginNonAdSubframeGesture = 9,
    kMaxValue = kCrossOriginNonAdSubframeGesture,
  };

  static FrameGesture GetMetricsEnum(FrameType frame, GestureType gesture);

  static void Record(FrameType frame, GestureType gesture);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(DownloadStats);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_DOWNLOAD_DOWNLOAD_STATS_H_
