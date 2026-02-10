// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_COMMON_HEADLESS_FEATURES_H_
#define HEADLESS_LIB_COMMON_HEADLESS_FEATURES_H_

#include "base/feature_list.h"
#include "headless/public/headless_export.h"

namespace headless::features {

// Enables virtual time, which allows for deterministic time control.
// In addition to the switches below, this feature also suppresses audio
// decoding and rendering. Audio plays in real time and does not respect virtual
// time, and video tracks are kept in sync with audio. For virtual time to work
// with video playback, audio must be suppressed.
HEADLESS_EXPORT BASE_DECLARE_FEATURE(kVirtualTime);

}  // namespace headless::features

#endif  // HEADLESS_LIB_COMMON_HEADLESS_FEATURES_H_
