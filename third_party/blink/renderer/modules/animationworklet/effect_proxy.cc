// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/effect_proxy.h"

namespace blink {

EffectProxy::EffectProxy(base::Optional<base::TimeDelta> local_time)
    : local_time_(local_time) {}

void EffectProxy::setLocalTime(double time_ms, bool is_null) {
  if (is_null) {
    local_time_.reset();
    return;
  }
  DCHECK(!std::isnan(time_ms));
  // Convert double to base::TimeDelta because cc/animation expects
  // base::TimeDelta.
  //
  // Note on precision loss: base::TimeDelta has microseconds precision which is
  // also the precision recommended by the web animation specification as well
  // [1]. If the input time value has a bigger precision then the conversion
  // causes precision loss. Doing the conversion here ensures that reading the
  // value back provides the actual value we use in further computation which
  // is the least surprising path.
  // [1] https://drafts.csswg.org/web-animations/#precision-of-time-values
  local_time_ = base::TimeDelta::FromMillisecondsD(time_ms);
}

double EffectProxy::localTime(bool& is_null) const {
  is_null = !local_time_.has_value();
  return local_time_.value_or(base::TimeDelta()).InMillisecondsF();
}

base::Optional<base::TimeDelta> EffectProxy::local_time() const {
  return local_time_;
}

}  // namespace blink
