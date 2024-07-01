// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_PROBES_MANAGER_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_PROBES_MANAGER_H_

#include <map>
#include <memory>

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/compute_pressure/probes_manager_base.h"
#include "services/device/public/mojom/pressure_manager.mojom-forward.h"
#include "services/device/public/mojom/pressure_update.mojom-shared.h"

namespace system_cpu {

class CpuProbe;

}

namespace device {

class CpuProbeManager;

class ProbesManager final : public ProbesManagerBase {
 public:
  explicit ProbesManager(base::TimeDelta sampling_interval);
  ~ProbesManager() override;

  ProbesManager(const ProbesManager&) = delete;
  ProbesManager& operator=(const ProbesManager&) = delete;

  mojom::PressureStatus AddClient(
      mojo::PendingRemote<mojom::PressureClient> client,
      mojom::PressureSource source) override;

  void SetCpuProbeForTesting(std::unique_ptr<system_cpu::CpuProbe> cpu_probe);

 private:
  friend class PressureManagerImplTest;

  // Called periodically by probe for each PressureSource.
  void UpdateClients(mojom::PressureSource source, mojom::PressureState state);

  // Stop corresponding probe once there is no client.
  void OnClientRemoteDisconnected(mojom::PressureSource source,
                                  mojo::RemoteSetElementId /*id*/);

  SEQUENCE_CHECKER(sequence_checker_);

  // Probe for retrieving the compute pressure state for CPU.
  std::unique_ptr<CpuProbeManager> cpu_probe_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::map<mojom::PressureSource, mojo::RemoteSet<mojom::PressureClient>>
      clients_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_PROBES_MANAGER_H_
