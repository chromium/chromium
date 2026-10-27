// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_PROGRESS_CLAMPING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_PROGRESS_CLAMPING_H_

#include <stdint.h>

namespace blink {

// If `no-clamp` is present, progress() can resolve to any number; if omitted
// (the default), the result is clamped to the range [0, 1].
// https://drafts.csswg.org/css-values-5/#progress
enum class ProgressClamping : uint8_t { kClamp, kNoClamp };

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_PROGRESS_CLAMPING_H_
