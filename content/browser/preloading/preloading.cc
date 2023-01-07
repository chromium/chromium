// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preloading.h"

namespace content {

PreloadingPredictor ToPreloadingPredictor(
    ContentPreloadingPredictor predictor) {
  return static_cast<PreloadingPredictor>(predictor);
}

}  // namespace content
