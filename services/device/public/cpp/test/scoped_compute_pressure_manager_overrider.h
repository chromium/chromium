// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_COMPUTE_PRESSURE_MANAGER_OVERRIDER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_COMPUTE_PRESSURE_MANAGER_OVERRIDER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "services/device/public/mojom/compute_pressure_state.mojom.h"

namespace device {

class ScopedComputePressureManagerOverrider {
 public:
  ScopedComputePressureManagerOverrider();
  ~ScopedComputePressureManagerOverrider();

  ScopedComputePressureManagerOverrider(
      const ScopedComputePressureManagerOverrider&) = delete;
  ScopedComputePressureManagerOverrider& operator=(
      const ScopedComputePressureManagerOverrider&) = delete;

  void UpdateClients(const mojom::ComputePressureState& state,
                     base::Time timestamp);

  void set_is_supported(bool is_supported);

 private:
  class FakeComputePressureManager;
  std::unique_ptr<FakeComputePressureManager> compute_pressure_manager_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_COMPUTE_PRESSURE_MANAGER_OVERRIDER_H_
