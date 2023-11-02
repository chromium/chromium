// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_TIME_SMOOTHER_H_
#define IOS_WEB_NAVIGATION_TIME_SMOOTHER_H_

#include "base/time/time.h"

namespace web {

// Helper class to smooth out runs of duplicate timestamps while still
// allowing time to jump backwards.
//
// Duplicated from NavigationControllerImpl (until we have a better
// idea how to handle NavigationController implementation overlap
// in general).
class TimeSmoother {
 public:
  // Returns `t` with possibly some time added on.
  base::Time GetSmoothedTime(base::Time t);

 private:
  // `low_water_mark_` is the first time in a sequence of adjusted
  // times and `high_water_mark_` is the last.
  base::Time low_water_mark_;
  base::Time high_water_mark_;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_TIME_SMOOTHER_H_
