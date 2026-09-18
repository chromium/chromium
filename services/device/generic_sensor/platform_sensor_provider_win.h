// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_WIN_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_WIN_H_

#include <SensorsApi.h>
#include <wrl/client.h>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "services/device/generic_sensor/platform_sensor_provider.h"
#include "services/device/generic_sensor/platform_sensor_reader_win_base.h"

namespace device {

// Implementation of PlatformSensorProvider for Windows platform.
// PlatformSensorProviderWin is responsible for following tasks:
// - Owns the COM STA task runner that all ISensorManager and ISensor access
//   happens on, and keeps ISensorManager confined to it via ComStaHelper.
// - Creates sensor readers on that COM STA task runner.
// - Constructs PlatformSensorWin on IPC thread and returns it to requester.
class PlatformSensorProviderWin final : public PlatformSensorProvider {
 public:
  PlatformSensorProviderWin();

  PlatformSensorProviderWin(const PlatformSensorProviderWin&) = delete;
  PlatformSensorProviderWin& operator=(const PlatformSensorProviderWin&) =
      delete;

  ~PlatformSensorProviderWin() override;

  base::WeakPtr<PlatformSensorProvider> AsWeakPtr() override;

  // Overrides ISensorManager COM interface provided by the system, used
  // only for testing purposes. The override is applied asynchronously on
  // |com_sta_task_runner_|, but it is ordered ahead of any subsequent
  // CreateSensorReader() call on that same sequence.
  void SetSensorManagerForTesting(
      Microsoft::WRL::ComPtr<ISensorManager> sensor_manager);

  scoped_refptr<base::SingleThreadTaskRunner> GetComStaTaskRunnerForTesting();

 protected:
  // PlatformSensorProvider interface implementation.
  void CreateSensorInternal(mojom::SensorType type,
                            CreateSensorCallback callback) override;

 private:
  class ComStaHelper;

  void SensorReaderCreated(mojom::SensorType type,
                           CreateSensorCallback callback,
                           ScopedPlatformSensorReaderWinBase sensor_reader);

  const scoped_refptr<base::SingleThreadTaskRunner> com_sta_task_runner_;
  base::SequenceBound<ComStaHelper> com_sta_helper_;
  base::WeakPtrFactory<PlatformSensorProviderWin> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_WIN_H_
