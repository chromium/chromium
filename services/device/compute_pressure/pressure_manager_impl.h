// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_PRESSURE_MANAGER_IMPL_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_PRESSURE_MANAGER_IMPL_H_

#include <map>
#include <memory>

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "services/device/public/mojom/pressure_update.mojom.h"

namespace device {

class CpuProbe;

// Handles the communication between the renderer process and services.
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
  static std::unique_ptr<PressureManagerImpl> Create();

  ~PressureManagerImpl() override;

  PressureManagerImpl(const PressureManagerImpl&) = delete;
  PressureManagerImpl& operator=(const PressureManagerImpl&) = delete;

  void Bind(mojo::PendingReceiver<mojom::PressureManager> receiver);

  // device::mojom::PressureManager implementation.
  void AddClient(mojo::PendingRemote<mojom::PressureClient> client,
                 mojom::PressureSource source,
                 AddClientCallback callback) override;

  void SetCpuProbeForTesting(std::unique_ptr<CpuProbe>);

 private:
  friend class PressureManagerImplTest;

  explicit PressureManagerImpl(base::TimeDelta sampling_interval);

  // Called periodically by probe for each PressureSource.
  void UpdateClients(mojom::PressureSource source, mojom::PressureState state);

  // Stop corresponding probe once there is no client.
  void OnClientRemoteDisconnected(mojom::PressureSource source,
                                  mojo::RemoteSetElementId /*id*/);

  SEQUENCE_CHECKER(sequence_checker_);

  // Probe for retrieving the compute pressure state for CPU.
  std::unique_ptr<CpuProbe> cpu_probe_ GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::ReceiverSet<mojom::PressureManager> receivers_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::map<mojom::PressureSource, mojo::RemoteSet<mojom::PressureClient>>
      clients_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_PRESSURE_MANAGER_IMPL_H_
