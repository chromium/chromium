// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_CRITICALLY_DAMPED_SPRING_H_
#define IOS_WEB_COMMON_CRITICALLY_DAMPED_SPRING_H_

namespace web {

// Evaluates the unit step response of a critically damped spring at `elapsed`
// seconds, returning the fraction of the total distance travelled.
//
// The curve is calibrated to match UIKit's
// `-[UIView animateWithDuration:delay:usingSpringWithDamping:1.0
//     initialSpringVelocity:options:animations:completion:]`, so callers
// driving their own per-frame interpolation stay visually in lockstep with
// concurrent UIKit spring animations of the same duration and velocity.
//
// `initial_velocity` is normalized: the fraction of the remaining distance
// covered per second at t = 0, matching UIKit's `initialSpringVelocity`.
//
// Returns exactly 0.0 for a non-positive `elapsed`, and exactly 1.0 once
// `elapsed` reaches `duration`. The analytical curve only approaches 1.0
// asymptotically, so callers must drive termination from `duration` rather
// than waiting for the return value to settle.
//
// A large enough `initial_velocity` makes the spring overshoot, so the result
// is deliberately not clamped to [0, 1]. Clamp at the call site when the
// consuming value requires it.
double CriticallyDampedSpringProgress(double elapsed,
                                      double duration,
                                      double initial_velocity);

}  // namespace web

#endif  // IOS_WEB_COMMON_CRITICALLY_DAMPED_SPRING_H_
