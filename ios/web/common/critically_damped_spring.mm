// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/critically_damped_spring.h"

#import <cmath>

namespace web {

namespace {

// Empirically derived factor relating a UIKit spring's nominal duration to its
// angular frequency. UIKit treats the duration as the point at which the
// spring has visually settled rather than as a hard stop, so this converts
// between the two conventions.
constexpr double kSpringSettlingFactor = 9.233414;

}  // namespace

double CriticallyDampedSpringProgress(double elapsed,
                                      double duration,
                                      double initial_velocity) {
  if (duration <= 0.0 || elapsed >= duration) {
    return 1.0;
  }
  if (elapsed <= 0.0) {
    return 0.0;
  }

  // Unit step response of a critically damped second order system:
  //   x(t) = 1 - (1 + (w - v0) * t) * e^(-w * t)
  const double omega = kSpringSettlingFactor / duration;
  return 1.0 - (1.0 + (omega - initial_velocity) * elapsed) *
                   std::exp(-omega * elapsed);
}

}  // namespace web
