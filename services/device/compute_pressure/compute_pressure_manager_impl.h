// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_COMPUTE_PRESSURE_MANAGER_IMPL_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_COMPUTE_PRESSURE_MANAGER_IMPL_H_

#include <memory>

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/compute_pressure/compute_pressure_sample.h"
#include "services/device/compute_pressure/compute_pressure_sampler.h"
#include "services/device/public/mojom/compute_pressure_manager.mojom.h"

namespace device {

class CpuProbe;

// Handles the communication between the browser process and services.
//
// This class owns one instance of ComputePressureSampler. The
// ComputePressureSampler instance keeps collecting compute pressure
// information from the underlying operating system when `clients_` is
// not empty and stops collecting when `clients_` becomes empty.
//
// DeviceService owns one instance of this class.
//
// Instances are not thread-safe and should be used on the same sequence.
class ComputePressureManagerImpl : public mojom::ComputePressureManager {
 public:
  // The sampling interval must be smaller or equal to the rate-limit for
  // observer updates.
  static constexpr base::TimeDelta kDefaultSamplingInterval = base::Seconds(1);

  // Factory method for production instances.
  static std::unique_ptr<ComputePressureManagerImpl> Create();

  // Factory method with dependency injection support for testing.
  static std::unique_ptr<ComputePressureManagerImpl> CreateForTesting(
      std::unique_ptr<CpuProbe> cpu_probe,
      base::TimeDelta sampling_interval);

  ~ComputePressureManagerImpl() override;

  ComputePressureManagerImpl(const ComputePressureManagerImpl&) = delete;
  ComputePressureManagerImpl& operator=(const ComputePressureManagerImpl&) =
      delete;

  void Bind(mojo::PendingReceiver<mojom::ComputePressureManager> receiver);

  // device::mojom::ComputePressureManager implementation.
  void AddClient(mojo::PendingRemote<mojom::ComputePressureClient> client,
                 AddClientCallback callback) override;

 private:
  ComputePressureManagerImpl(std::unique_ptr<CpuProbe> cpu_probe,
                             base::TimeDelta sampling_interval);

  // Called periodically by ComputePressureSampler.
  void UpdateClients(ComputePressureSample sample);

  // Stop `sampler_` once there is no client.
  void OnClientRemoteDisconnected(mojo::RemoteSetElementId /*id*/);

  SEQUENCE_CHECKER(sequence_checker_);

  ComputePressureSampler sampler_ GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::ReceiverSet<mojom::ComputePressureManager> receivers_
      GUARDED_BY_CONTEXT(sequence_checker_);
  mojo::RemoteSet<mojom::ComputePressureClient> clients_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_COMPUTE_PRESSURE_MANAGER_IMPL_H_
