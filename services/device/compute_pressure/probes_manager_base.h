// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_PROBES_MANAGER_BASE_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_PROBES_MANAGER_BASE_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/pressure_manager.mojom-forward.h"
#include "services/device/public/mojom/pressure_update.mojom-shared.h"

namespace device {

class ProbesManagerBase {
 public:
  ProbesManagerBase();
  virtual ~ProbesManagerBase();

  ProbesManagerBase(const ProbesManagerBase&) = delete;
  ProbesManagerBase& operator=(const ProbesManagerBase&) = delete;

  virtual mojom::PressureStatus AddClient(
      mojo::PendingRemote<mojom::PressureClient> client,
      mojom::PressureSource source) = 0;
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_PROBES_MANAGER_BASE_H_
