// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_PRESSURE_MANAGER_IMPL_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_PRESSURE_MANAGER_IMPL_H_

#include <map>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "services/device/public/mojom/pressure_update.mojom.h"

namespace device {

class ProbesManager;
class VirtualProbesManager;

// Handles the communication between content/browser and services.
//
// This class owns one instance of probe for each PressureSource. The probe
// instance keeps collecting compute pressure information from the
// underlying operating system when its `clients_` is not empty and stops
// collecting when its `clients_` becomes empty.
//
// DeviceService owns one instance of this class.
//
// Instances are not thread-safe and should be used on the same sequence.
class PressureManagerImpl : public mojom::PressureManager {
 public:
  // The sampling interval must be smaller or equal to the rate-limit for
  // observer updates.
  static constexpr base::TimeDelta kDefaultSamplingInterval = base::Seconds(1);

  // Factory method for production instances.
  static std::unique_ptr<PressureManagerImpl> Create(
      base::TimeDelta sampling_interval = kDefaultSamplingInterval);

  ~PressureManagerImpl() override;

  PressureManagerImpl(const PressureManagerImpl&) = delete;
  PressureManagerImpl& operator=(const PressureManagerImpl&) = delete;

  void Bind(mojo::PendingReceiver<mojom::PressureManager> receiver);

  // device::mojom::PressureManager implementation.
  void AddClient(mojom::PressureSource source,
                 const std::optional<base::UnguessableToken>& token,
                 AddClientCallback callback) override;

  ProbesManager* GetProbesManagerForTesting() const;

 private:
  friend class PressureManagerImplTest;

  explicit PressureManagerImpl(base::TimeDelta sampling_interval);

  void AddVirtualPressureSource(
      const base::UnguessableToken& token,
      mojom::PressureSource source,
      mojom::VirtualPressureSourceMetadataPtr metadata,
      AddVirtualPressureSourceCallback callback) override;
  void RemoveVirtualPressureSource(
      const base::UnguessableToken& token,
      mojom::PressureSource source,
      RemoveVirtualPressureSourceCallback callback) override;
  void UpdateVirtualPressureSourceState(
      const ::base::UnguessableToken& token,
      mojom::PressureSource source,
      mojom::PressureState state,
      UpdateVirtualPressureSourceStateCallback callback) override;

  SEQUENCE_CHECKER(sequence_checker_);

  base::TimeDelta sampling_interval_;

  mojo::ReceiverSet<mojom::PressureManager> receivers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<ProbesManager> probes_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::flat_map<base::UnguessableToken, std::unique_ptr<VirtualProbesManager>>
      virtual_probes_managers_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_PRESSURE_MANAGER_IMPL_H_
