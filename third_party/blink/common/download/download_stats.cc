// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/download/download_stats.h"

#include "base/metrics/histogram_macros.h"

namespace blink {

// static
DownloadStats::FrameGesture DownloadStats::GetMetricsEnum(FrameType frame,
                                                          GestureType gesture) {
  switch (frame) {
    case FrameType::kMainFrame:
      return gesture == GestureType::kWithGesture
                 ? FrameGesture::kMainFrameGesture
                 : FrameGesture::kMainFrameNoGesture;
    case FrameType::kSameOriginAdSubframe:
      return gesture == GestureType::kWithGesture
                 ? FrameGesture::kSameOriginAdSubframeGesture
                 : FrameGesture::kSameOriginAdSubframeNoGesture;
    case FrameType::kSameOriginNonAdSubframe:
      return gesture == GestureType::kWithGesture
                 ? FrameGesture::kSameOriginNonAdSubframeGesture
                 : FrameGesture::kSameOriginNonAdSubframeNoGesture;
    case FrameType::kCrossOriginAdSubframe:
      return gesture == GestureType::kWithGesture
                 ? FrameGesture::kCrossOriginAdSubframeGesture
                 : FrameGesture::kCrossOriginAdSubframeNoGesture;
    case FrameType::kCrossOriginNonAdSubframe:
      return gesture == GestureType::kWithGesture
                 ? FrameGesture::kCrossOriginNonAdSubframeGesture
                 : FrameGesture::kCrossOriginNonAdSubframeNoGesture;
  }
}

// static
void DownloadStats::Record(FrameType frame, GestureType gesture) {
  UMA_HISTOGRAM_ENUMERATION("Download.FrameGesture",
                            GetMetricsEnum(frame, gesture));
}

}  // namespace blink
