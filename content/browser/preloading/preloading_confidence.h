// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOADING_CONFIDENCE_H_
#define CONTENT_BROWSER_PRELOADING_PRELOADING_CONFIDENCE_H_

#include "base/check_op.h"
#include "base/types/strong_alias.h"

namespace content {

// The confidence percentage of a predictor's preloading prediction. The range
// is [0, 100].
class PreloadingConfidence
    : public base::StrongAlias<class PreloadingConfidenceTag, int> {
 public:
  constexpr explicit PreloadingConfidence(int confidence)
      : base::StrongAlias<class PreloadingConfidenceTag, int>(confidence) {
    CHECK_GE(confidence, 0);
    CHECK_LE(confidence, 100);
  }
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRELOADING_CONFIDENCE_H_
