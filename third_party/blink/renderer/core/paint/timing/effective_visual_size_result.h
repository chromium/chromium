// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_EFFECTIVE_VISUAL_SIZE_RESULT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_EFFECTIVE_VISUAL_SIZE_RESULT_H_

namespace blink {

// Result of "determining the effective visual size of an element" in the LCP
// specification. This maps to the "effective visual result" concept, but we
// include different fields required for our implementation.
// https://w3c.github.io/largest-contentful-paint/#effective-visual-size-result
struct EffectiveVisualSizeResult {
  // The actual "effective visual size". This is what the LCP algorithm uses for
  // size.
  uint64_t size = 0;

  // The entropy in bytes-per-pixel of the image. This gets reported to UKM.
  double entropy = 0;

  // Whether the minimum entropy requirement has been met. When false, the image
  // should be ignored by the LCP algorithm.
  bool is_min_entropy_met = true;

  // Whether the entire viewport is covered by the image. When true, the image
  // should be ignored by the LCP algorithm.
  bool is_viewport_covered = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_EFFECTIVE_VISUAL_SIZE_RESULT_H_
