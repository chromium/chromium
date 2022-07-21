// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_CORE_TIMES_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_CORE_TIMES_H_

#include <stdint.h>

namespace device {

// CPU core utilization statistics.
//
// Linux:
// Quantities are expressed in "user hertz", which is a Linux kernel
// configuration knob (USER_HZ). Typical values range between 1/100 seconds
// and 1/1000 seconds. The denominator can be obtained from
// sysconf(_SC_CLK_TCK).
//
// Mac:
// Quantities are expressed in "CPU Ticks", which is an arbitrary unit of time
// recording how many intervals of time elapsed, typically 1/100 of a second.
struct CoreTimes {
  // Normal processes executing in user mode.
  uint64_t user() const { return times[0]; }
  // Niced processes executing in user mode.
  uint64_t nice() const { return times[1]; }
  // Processes executing in kernel mode.
  uint64_t system() const { return times[2]; }
  // Twiddling thumbs.
  uint64_t idle() const { return times[3]; }
  // Waiting for I/O to complete. Unreliable.
  uint64_t iowait() const { return times[4]; }
  // Servicing interrupts.
  uint64_t irq() const { return times[5]; }
  // Servicing softirqs.
  uint64_t softirq() const { return times[6]; }
  // Involuntary wait.
  uint64_t steal() const { return times[7]; }
  // Running a normal guest.
  uint64_t guest() const { return times[8]; }
  // Running a niced guest.
  uint64_t guest_nice() const { return times[9]; }

  uint64_t times[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  // Computes a CPU's utilization over the time between two stat snapshots.
  //
  // Returns a value between 0.0 and 1.0 on success, and -1.0 when given
  // invalid data, such as a `baseline` that does not represent a stat
  // snapshot collected before `this` snapshot.
  double TimeUtilization(const CoreTimes& baseline) const;
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_CORE_TIMES_H_
