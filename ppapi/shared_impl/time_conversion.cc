// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/time_conversion.h"

namespace ppapi {

PP_Time TimeToPPTime(base::Time t) {
  return t.InSecondsFSinceUnixEpoch();
}

base::Time PPTimeToTime(PP_Time t) {
  // The time code handles exact "0" values as special, and produces
  // a "null" Time object. But calling code would expect t==0 to represent the
  // epoch (according to the description of PP_Time). Hence we just return the
  // epoch in this case.
  if (t == 0.0)
    return base::Time::UnixEpoch();
  return base::Time::FromSecondsSinceUnixEpoch(t);
}

PP_TimeTicks TimeTicksToPPTimeTicks(base::TimeTicks t) {
  return static_cast<double>(t.ToInternalValue()) /
         base::Time::kMicrosecondsPerSecond;
}

double PPGetLocalTimeZoneOffset(const base::Time& time) {
  // Explode it to local time and then unexplode it as if it were UTC. Also
  // explode it to UTC and unexplode it (this avoids mismatching rounding or
  // lack thereof). The time zone offset is their difference.
  base::Time::Exploded exploded = {0};
  base::Time::Exploded utc_exploded = {0};
  time.LocalExplode(&exploded);
  time.UTCExplode(&utc_exploded);
  if (exploded.HasValidValues() && utc_exploded.HasValidValues()) {
    base::Time adj_time;
    if (base::Time::FromUTCExploded(exploded, &adj_time)) {
      base::Time cur;
      if (base::Time::FromUTCExploded(utc_exploded, &cur))
        return (adj_time - cur).InSecondsF();
    }
  }
  return 0.0;
}

}  // namespace ppapi
