// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_NAVIGATION_TIMINGS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_NAVIGATION_TIMINGS_H_

#include "base/time/time.h"

namespace blink {

struct WebNavigationTimings {
  base::TimeTicks input_start;
  base::TimeTicks navigation_start;
  base::TimeTicks redirect_start;
  base::TimeTicks redirect_end;
  base::TimeTicks fetch_start;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_NAVIGATION_TIMINGS_H_
