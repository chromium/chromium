// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/core_times.h"

namespace device {

double CoreTimes::TimeUtilization(const CoreTimes& baseline) const {
  // Each of the blocks below consists of a check and a subtraction. The check
  // is used to bail on invalid input (/proc/stat counters should never
  // decrease over time).
  //
  // The check is also essential for the correctness of the subtraction -- the
  // result of the subtraction is stored in a temporary `uint64_t` before being
  // accumulated in `active_delta`, and this intermediate result must not be
  // negative.

  if (user() < baseline.user())
    return -1;
  double active_delta = user() - baseline.user();

  if (nice() < baseline.nice())
    return -1;
  active_delta += nice() - baseline.nice();

  if (system() < baseline.system())
    return -1;
  active_delta += system() - baseline.system();

  if (idle() < baseline.idle())
    return -1;
  uint64_t idle_delta = idle() - baseline.idle();

  // iowait() is unreliable, according to the Linux kernel documentation at
  // https://www.kernel.org/doc/Documentation/filesystems/proc.txt.

  if (irq() < baseline.irq())
    return -1;
  active_delta += irq() - baseline.irq();

  if (softirq() < baseline.softirq())
    return -1;
  active_delta += softirq() - baseline.softirq();

  if (steal() < baseline.steal())
    return -1;
  active_delta += steal() - baseline.steal();

  // guest() and guest_nice() are included in user(). Full analysis in
  // https://unix.stackexchange.com/a/303224/

  double total_delta = active_delta + idle_delta;
  if (total_delta == 0) {
    // The two snapshots represent the same point in time, so the time interval
    // between the two snapshots is empty.
    return -1;
  }

  return active_delta / total_delta;
}

}  // namespace device
