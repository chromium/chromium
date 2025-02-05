// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RANDOMIZED_CONFIDENCE_UTILS_H_
#define CONTENT_BROWSER_RENDERER_HOST_RANDOMIZED_CONFIDENCE_UTILS_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/confidence_level.mojom.h"

namespace content {

// Returns the randomized trigger rate with 7 digits of precision.
// The trigger rate returned will be between 0 and 1.
CONTENT_EXPORT double GetConfidenceRandomizedTriggerRate();

// Returns a local differentially private confidence value.
// The `randomizedTriggerRate` determines how much noise is added.
CONTENT_EXPORT blink::mojom::ConfidenceLevel GenerateRandomizedConfidenceLevel(
    double randomizedTriggerRate,
    blink::mojom::ConfidenceLevel confidence);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RANDOMIZED_CONFIDENCE_UTILS_H_
