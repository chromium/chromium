// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_H_

#include <memory>

#include "services/device/compute_pressure/pressure_sample.h"
#include "services/device/public/mojom/pressure_update.mojom-shared.h"

namespace device {

// Interface for retrieving the compute pressure state from the underlying OS.
//
// Operating systems differ in how they summarize the info needed to derive the
// compute pressure state. For example, the Linux kernel exposes CPU utilization
// as a summary over the device's entire uptime, while the Windows WMI exposes
// CPU utilization over the last second.
//
// This interface abstracts over the differences with a unified model where the
// implementation is responsible for integrating over the time between two
// Update() calls.
//
// This interface has rather strict requirements. This is because operating
// systems differ in requirements for accessing compute pressure information,
// and this interface expresses the union of all requirements.
//
// Instances are not thread-safe. All methods except for the constructor must be
// created on the same sequence. The sequence must allow blocking I/O
// operations.
class CpuProbe {
 public:
  // LastSample() return value when the implementation fails to get a result.
  static constexpr PressureSample kUnsupportedValue = {.cpu_utilization = 0.0};

  // Instantiates the CpuProbe subclass most suitable for the current platform.
  //
  // Returns nullptr if no suitable implementation exists.
  static std::unique_ptr<CpuProbe> Create();

  CpuProbe(const CpuProbe&) = delete;
  CpuProbe& operator=(const CpuProbe&) = delete;

  virtual ~CpuProbe();

  // Collects new CPU compute resource availability.
  //
  // The return value of LastSample() will reflect resource availability between
  // the current state and the last Update() call.
  virtual void Update() = 0;

  // CPU compute resource availability between the last two Update() calls.
  virtual PressureSample LastSample() = 0;

 protected:
  // The constructor is intentionally only exposed to subclasses. Production
  // code must use the Create() factory method.
  CpuProbe();
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_H_
