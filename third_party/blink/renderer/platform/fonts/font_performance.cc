// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_performance.h"

namespace blink {

base::TimeDelta FontPerformance::primary_font_;
base::TimeDelta FontPerformance::primary_font_in_style_;
base::TimeDelta FontPerformance::system_fallback_;
unsigned FontPerformance::in_style_ = 0;

}  // namespace blink
