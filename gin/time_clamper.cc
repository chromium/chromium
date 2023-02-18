// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/time_clamper.h"

#include "build/build_config.h"

namespace gin {

// As site isolation is enabled on desktop platforms, we can safely provide
// more timing resolution. Jittering is still enabled everywhere.
#if BUILDFLAG(IS_ANDROID)
// static
const int64_t TimeClamper::kResolutionMicros = 100;
#else
const int64_t TimeClamper::kResolutionMicros = 5;
#endif

}  // namespace gin
