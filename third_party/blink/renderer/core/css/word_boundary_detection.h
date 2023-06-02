// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_WORD_BOUNDARY_DETECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_WORD_BOUNDARY_DETECTION_H_

namespace blink {

// Values of the `word-boundary-detection` property.
// https://drafts.csswg.org/css-text-4/#word-boundary-detection
enum class WordBoundaryDetection {
  kNormal,
  kAuto,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_WORD_BOUNDARY_DETECTION_H_
