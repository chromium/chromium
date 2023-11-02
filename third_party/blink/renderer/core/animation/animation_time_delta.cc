// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_time_delta.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

#if !BUILDFLAG(BLINK_ANIMATION_USE_TIME_DELTA)
// Comparison operators on AnimationTimeDelta.
bool CORE_EXPORT operator==(const AnimationTimeDelta& lhs,
                            const AnimationTimeDelta& rhs) {
  return lhs.InSecondsF() == rhs.InSecondsF();
}
bool CORE_EXPORT operator!=(const AnimationTimeDelta& lhs,
                            const AnimationTimeDelta& rhs) {
  return lhs.InSecondsF() != rhs.InSecondsF();
}
bool CORE_EXPORT operator>(const AnimationTimeDelta& lhs,
                           const AnimationTimeDelta& rhs) {
  return lhs.InSecondsF() > rhs.InSecondsF();
}
bool CORE_EXPORT operator<(const AnimationTimeDelta& lhs,
                           const AnimationTimeDelta& rhs) {
  return !(lhs >= rhs);
}
bool CORE_EXPORT operator>=(const AnimationTimeDelta& lhs,
                            const AnimationTimeDelta& rhs) {
  return lhs.InSecondsF() >= rhs.InSecondsF();
}
bool CORE_EXPORT operator<=(const AnimationTimeDelta& lhs,
                            const AnimationTimeDelta& rhs) {
  return lhs.InSecondsF() <= rhs.InSecondsF();
}

std::ostream& operator<<(std::ostream& os, const AnimationTimeDelta& time) {
  return os << time.InSecondsF() << " s";
}
#endif

}  // namespace blink
