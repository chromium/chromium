// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_CHROMEOS_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_CHROMEOS_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/components/sensors/mojom/cros_sensor_service.mojom.h"
#include "chromeos/components/sensors/mojom/sensor.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/generic_sensor/platform_sensor_provider_linux_base.h"

namespace device {

class PlatformSensorProviderChromeOS
    : public PlatformSensorProviderLinuxBase,
      public chromeos::sensors::mojom::SensorHalClient,
      public chromeos::sensors::mojom::SensorServiceNewDevicesObserver {
 public:
  PlatformSensorProviderChromeOS();
  PlatformSensorProviderChromeOS(const PlatformSensorProviderChromeOS&) =
      delete;
  PlatformSensorProviderChromeOS& operator=(
      const PlatformSensorProviderChromeOS&) = delete;
  ~PlatformSensorProviderChromeOS() override;

  // chromeos::sensors::mojom::SensorHalClient overrides:
  void SetUpChannel(mojo::PendingRemote<chromeos::sensors::mojom::SensorService>
                        pending_remote) override;

  // chromeos::sensors::mojom::SensorServiceNewDevicesObserver overrides:
  void OnNewDeviceAdded(
      int32_t iio_device_id,
      const std::vector<chromeos::sensors::mojom::DeviceType>& types) override;

  base::WeakPtr<PlatformSensorProvider> AsWeakPtr() override;

 protected:
  // PlatformSensorProviderLinuxBase overrides:
  void CreateSensorInternal(mojom::SensorType type,
                            CreateSensorCallback callback) override;
  void FreeResources() override;

  bool IsFusionSensorType(mojom::SensorType type) const override;
  bool IsSensorTypeAvailable(mojom::SensorType type) const override;

 private:
  friend class PlatformSensorProviderChromeOSTest;

  enum class SensorLocation {
    kBase = 0,
    kLid,
    kCamera,
    kMax,
  };

  using SensorIdTypesMap =
      base::flat_map<int32_t,
                     std::vector<chromeos::sensors::mojom::DeviceType>>;

  struct SensorData {
    SensorData();
    ~SensorData();

    std::vector<mojom::SensorType> types;
    bool ignored = false;
    std::optional<SensorLocation> location;
    std::optional<double> scale;

    // Temporarily stores the remote, waiting for its attributes information.
    // It'll be passed to PlatformSensorChromeOS' constructor as an argument
    // after all information is collected, if this sensor is needed.
    mojo::Remote<chromeos::sensors::mojom::SensorDevice> remote;
  };

  std::optional<SensorLocation> ParseLocation(
      const std::optional<std::string>& location);

  std::optional<int32_t> GetDeviceId(mojom::SensorType type) const;

  void RegisterSensorClient();
  void OnSensorHalClientFailure(base::TimeDelta reconnection_delay);

  void OnSensorServiceDisconnect();

  void OnNewDevicesObserverDisconnect();

  void ResetSensorService();

  void GetAllDeviceIdsCallback(const SensorIdTypesMap& ids_types);
  void RegisterDevice(
      int32_t id,
      const std::vector<chromeos::sensors::mojom::DeviceType>& types);

  void GetAttributesCallback(
      int32_t id,
      const std::vector<std::optional<std::string>>& values);
  void IgnoreSensor(SensorData& sensor);
  bool AreAllSensorsReady() const;

  void OnSensorDeviceDisconnect(int32_t id,
                                uint32_t custom_reason_code,
                                const std::string& description);

  void ReplaceAndRemoveSensor(mojom::SensorType type);

  void ProcessSensorsIfPossible();

  void DetermineMotionSensors();
  void DetermineLightSensor();
  void UpdateSensorIdMapping(const mojom::SensorType& type, int32_t id);

  // Remove Mojo remotes of the unused devices, as they'll never be used.
  void RemoveUnusedSensorDeviceRemotes();
  void ProcessStoredRequests();

  mojo::Remote<chromeos::sensors::mojom::SensorDevice> GetSensorDeviceRemote(
      int32_t id);

  mojo::Receiver<chromeos::sensors::mojom::SensorHalClient> sensor_hal_client_{
      this};

  // The Mojo remote to query and request for devices.
  mojo::Remote<chromeos::sensors::mojom::SensorService> sensor_service_remote_;

  // The Mojo channel to get notified when new devices are added to IIO Service.
  mojo::Receiver<chromeos::sensors::mojom::SensorServiceNewDevicesObserver>
      new_devices_observer_{this};

  // The flag of sensor ids received or not to help determine if all sensors are
  // ready. It's needed when there is no sensor at all.
  bool sensor_ids_received_ = false;

  // First is the device id, second is the device's types, data and Mojo remote.
  std::map<int32_t, SensorData> sensors_;

  // Stores the selected sensor devices' ids by type.
  std::map<mojom::SensorType, int32_t> sensor_id_by_type_;

  base::WeakPtrFactory<PlatformSensorProviderChromeOS> weak_ptr_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(PlatformSensorProviderChromeOSTest,
                           CheckUnsupportedTypes);
  FRIEND_TEST_ALL_PREFIXES(PlatformSensorProviderChromeOSTest,
                           SensorDeviceDisconnectWithReason);
  FRIEND_TEST_ALL_PREFIXES(PlatformSensorProviderChromeOSTest, ReconnectClient);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_CHROMEOS_H_
