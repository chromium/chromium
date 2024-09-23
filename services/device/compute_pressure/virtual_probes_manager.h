// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_VIRTUAL_PROBES_MANAGER_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_VIRTUAL_PROBES_MANAGER_H_

#include "base/containers/enum_set.h"
#include "services/device/compute_pressure/probes_manager.h"
#include "services/device/public/mojom/pressure_manager.mojom-forward.h"
#include "services/device/public/mojom/pressure_update.mojom-shared.h"

namespace device {

class VirtualProbesManager final : public ProbesManager {
 public:
  explicit VirtualProbesManager(base::TimeDelta sampling_interval);
  ~VirtualProbesManager() override;

  VirtualProbesManager(const VirtualProbesManager&) = delete;
  VirtualProbesManager& operator=(const VirtualProbesManager&) = delete;

  // Creates a new VirtualProbe for |source| and adds it to the overrides
  // handled by this VirtualProbesManager instance. Returns false if |source|
  // is already being overridden.
  bool AddOverrideForSource(mojom::PressureSource source,
                            mojom::VirtualPressureSourceMetadataPtr metadata);

  // Removes the VirtualProbe override for |sources|. Does nothing if |source|
  // is not being overridden.
  void RemoveOverrideForSource(mojom::PressureSource source);

  // Adds a new sample for the given |source| and updates any
  // mojom::PressureClient instances waiting for updates.
  // Does nothing if |source| is not being overridden.
  void AddUpdate(mojom::PressureSource source, mojom::PressureState state);

  // Returns true if |source| has a corresponding VirtualProbe instance, and
  // false otherwise.
  bool IsOverriding(mojom::PressureSource source) const;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  base::EnumSet<mojom::PressureSource,
                mojom::PressureSource::kMinValue,
                mojom::PressureSource::kMaxValue>
      overridden_sources_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_VIRTUAL_PROBES_MANAGER_H_
