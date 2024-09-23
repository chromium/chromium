// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_PROBES_MANAGER_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_PROBES_MANAGER_H_

#include <map>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/pressure_manager.mojom-forward.h"
#include "services/device/public/mojom/pressure_update.mojom-shared.h"

namespace device {

class CpuProbeManager;

class ProbesManager {
 public:
  explicit ProbesManager(base::TimeDelta sampling_interval);
  virtual ~ProbesManager();

  ProbesManager(const ProbesManager&) = delete;
  ProbesManager& operator=(const ProbesManager&) = delete;

  bool is_supported(mojom::PressureSource source) const;

  // Adds |client| to the list of mojom::PressureClient remotes for the given
  // |source| type and starts the probe if necessary.
  void RegisterClientRemote(mojo::Remote<mojom::PressureClient> client,
                            mojom::PressureSource source);

  base::TimeDelta sampling_interval_for_testing() const;

 protected:
  CpuProbeManager* cpu_probe_manager() const;

  void set_cpu_probe_manager(
      std::unique_ptr<CpuProbeManager> cpu_probe_manager);

  const base::RepeatingCallback<void(mojom::PressureState)>&
  cpu_probe_sampling_callback() const;

 private:
  friend class PressureManagerImplTest;
  FRIEND_TEST_ALL_PREFIXES(PressureManagerImplTest, AddClientNoProbe);

  // Called periodically by probe for each PressureSource.
  void UpdateClients(mojom::PressureSource source, mojom::PressureState state);

  // Stop corresponding probe once there is no client.
  void OnClientRemoteDisconnected(mojom::PressureSource source,
                                  mojo::RemoteSetElementId /*id*/);

  SEQUENCE_CHECKER(sequence_checker_);

  const base::TimeDelta sampling_interval_;

  const base::RepeatingCallback<void(mojom::PressureState)>
      cpu_probe_sampling_callback_;

  // Probe for retrieving the compute pressure state for CPU.
  std::unique_ptr<CpuProbeManager> cpu_probe_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::map<mojom::PressureSource, mojo::RemoteSet<mojom::PressureClient>>
      clients_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_PROBES_MANAGER_H_
