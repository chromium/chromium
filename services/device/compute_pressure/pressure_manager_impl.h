// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_PRESSURE_MANAGER_IMPL_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_PRESSURE_MANAGER_IMPL_H_

#include <memory>

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/compute_pressure/platform_collector.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"

namespace device {

class CpuProbe;

// Handles the communication between the browser process and services.
//
// This class owns one instance of PlatformCollector. The PlatformCollector
// instance keeps collecting compute pressure information from the
// underlying operating system when `clients_` is not empty and stops
// collecting when `clients_` becomes empty.
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

  // Factory method with dependency injection support for testing.
  static std::unique_ptr<PressureManagerImpl> CreateForTesting(
      std::unique_ptr<CpuProbe> cpu_probe,
      base::TimeDelta sampling_interval);

  ~PressureManagerImpl() override;

  PressureManagerImpl(const PressureManagerImpl&) = delete;
  PressureManagerImpl& operator=(const PressureManagerImpl&) = delete;

  void Bind(mojo::PendingReceiver<mojom::PressureManager> receiver);

  // device::mojom::PressureManager implementation.
  void AddClient(mojo::PendingRemote<mojom::PressureClient> client,
                 AddClientCallback callback) override;

 private:
  PressureManagerImpl(std::unique_ptr<CpuProbe> cpu_probe,
                      base::TimeDelta sampling_interval);

  // Called periodically by PlatformCollector.
  void UpdateClients(mojom::PressureState state);

  // Stop `collector_` once there is no client.
  void OnClientRemoteDisconnected(mojo::RemoteSetElementId /*id*/);

  SEQUENCE_CHECKER(sequence_checker_);

  PlatformCollector collector_ GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::ReceiverSet<mojom::PressureManager> receivers_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::RemoteSet<mojom::PressureClient> clients_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_PRESSURE_MANAGER_IMPL_H_
