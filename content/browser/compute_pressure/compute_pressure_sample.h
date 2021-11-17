// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPUTE_PRESSURE_COMPUTE_PRESSURE_SAMPLE_H_
#define CONTENT_BROWSER_COMPUTE_PRESSURE_COMPUTE_PRESSURE_SAMPLE_H_

namespace content {

// Represents availability of compute resources measured over a period of time.
struct ComputePressureSample {
  // Average utilization of all CPU cores.
  //
  // Values use a scale from 0.0 (no utilization) to 1.0 (100% utilization).
  double cpu_utilization;

  // Average normalized clock speed over all CPU cores.
  //
  // The normalized clock speed for each CPU core uses a piecewise-linear scale
  // from 0.0 (minimum clock speed) to 0.5 (base clock speed) to 1.0 (maximum
  // clock-speed).
  double cpu_speed;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPUTE_PRESSURE_COMPUTE_PRESSURE_SAMPLE_H_
