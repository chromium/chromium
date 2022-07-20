// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_PRESSURE_MANAGER_OVERRIDER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_PRESSURE_MANAGER_OVERRIDER_H_

#include "base/time/time.h"
#include "services/device/public/mojom/pressure_state.mojom.h"

namespace device {

class ScopedPressureManagerOverrider {
 public:
  ScopedPressureManagerOverrider();
  ~ScopedPressureManagerOverrider();

  ScopedPressureManagerOverrider(const ScopedPressureManagerOverrider&) =
      delete;
  ScopedPressureManagerOverrider& operator=(
      const ScopedPressureManagerOverrider&) = delete;

  void UpdateClients(const mojom::PressureState& state, base::Time timestamp);

  void set_is_supported(bool is_supported);

 private:
  class FakePressureManager;
  std::unique_ptr<FakePressureManager> pressure_manager_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_SCOPED_PRESSURE_MANAGER_OVERRIDER_H_
