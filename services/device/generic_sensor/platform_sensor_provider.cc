// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"

#if BUILDFLAG(IS_MAC)
#include "services/device/generic_sensor/platform_sensor_provider_mac.h"
#elif BUILDFLAG(IS_ANDROID)
#include "services/device/generic_sensor/platform_sensor_provider_android.h"
#elif BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "services/device/generic_sensor/platform_sensor_provider_win.h"
#include "services/device/generic_sensor/platform_sensor_provider_winrt.h"
#elif BUILDFLAG(IS_CHROMEOS)
#include "services/device/generic_sensor/platform_sensor_provider_chromeos.h"
#elif BUILDFLAG(IS_LINUX) && defined(USE_UDEV)
#include "services/device/generic_sensor/platform_sensor_provider_linux.h"
#endif

namespace device {

namespace {

constexpr uint64_t kReadingBufferSize = sizeof(SensorReadingSharedBuffer);
constexpr uint64_t kSharedBufferSizeInBytes =
    kReadingBufferSize *
    (static_cast<uint64_t>(mojom::SensorType::kMaxValue) + 1);

}  // namespace

PlatformSensorProvider::PlatformSensorProvider() = default;

PlatformSensorProvider::~PlatformSensorProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

// static
std::unique_ptr<PlatformSensorProvider> PlatformSensorProvider::Create() {
#if BUILDFLAG(IS_MAC)
  return std::make_unique<PlatformSensorProviderMac>();
#elif BUILDFLAG(IS_ANDROID)
  return std::make_unique<PlatformSensorProviderAndroid>();
#elif BUILDFLAG(IS_WIN)
  if (PlatformSensorProvider::UseWindowsWinrt()) {
    return std::make_unique<PlatformSensorProviderWinrt>();
  } else {
    return std::make_unique<PlatformSensorProviderWin>();
  }
#elif BUILDFLAG(IS_CHROMEOS)
  return std::make_unique<PlatformSensorProviderChromeOS>();
#elif BUILDFLAG(IS_LINUX) && defined(USE_UDEV)
  return std::make_unique<PlatformSensorProviderLinux>();
#else
  return nullptr;
#endif
}

void PlatformSensorProvider::CreateSensor(mojom::SensorType type,
                                          CreateSensorCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!CreateSharedBufferIfNeeded()) {
    std::move(callback).Run(nullptr);
    return;
  }

  SensorReadingSharedBuffer* reading_buffer =
      GetSensorReadingSharedBufferForType(type);
  if (!reading_buffer) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto& requests = requests_map_[type];
  const bool callback_queue_was_empty = requests.empty();
  requests.push_back(std::move(callback));
  if (callback_queue_was_empty) {
    // This is the first CreateSensor call.
    CreateSensorInternal(
        type, reading_buffer,
        base::BindOnce(&PlatformSensorProvider::NotifySensorCreated,
                       base::Unretained(this), type));
  }
}

scoped_refptr<PlatformSensor> PlatformSensorProvider::GetSensor(
    mojom::SensorType type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = sensor_map_.find(type);
  if (it != sensor_map_.end()) {
    return it->second;
  }
  return nullptr;
}

bool PlatformSensorProvider::CreateSharedBufferIfNeeded() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (mapped_region_.IsValid()) {
    return true;
  }

  mapped_region_ =
      base::ReadOnlySharedMemoryRegion::Create(kSharedBufferSizeInBytes);

  return mapped_region_.IsValid();
}

void PlatformSensorProvider::FreeResourcesIfNeeded() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (sensor_map_.empty() && requests_map_.empty()) {
    FreeResources();
    mapped_region_ = {};
  }
}

void PlatformSensorProvider::RemoveSensor(mojom::SensorType type,
                                          PlatformSensor* sensor) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = sensor_map_.find(type);
  if (it == sensor_map_.end()) {
    // It is possible on PlatformSensorFusion creation failure since the
    // PlatformSensorFusion object is not added to the |sensor_map_|, but
    // its base class destructor PlatformSensor::~PlatformSensor() calls this
    // RemoveSensor() function with the PlatformSensorFusion type.
    // It is also possible on PlatformSensorProviderChromeOS as late present
    // sensors makes the previous sensor calls this RemoveSensor() function
    // twice.
    return;
  }

  if (sensor != it->second) {
    // It is possible on PlatformSensorProviderChromeOS as late present sensors
    // may change the devices chosen on specific types.
    return;
  }

  sensor_map_.erase(type);
  FreeResourcesIfNeeded();
}

base::ReadOnlySharedMemoryRegion
PlatformSensorProvider::CloneSharedMemoryRegion() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CreateSharedBufferIfNeeded();
  return mapped_region_.region.Duplicate();
}

void PlatformSensorProvider::NotifySensorCreated(
    mojom::SensorType type,
    scoped_refptr<PlatformSensor> sensor) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!base::Contains(sensor_map_, type));
  DCHECK(base::Contains(requests_map_, type));

  if (sensor) {
    sensor_map_[type] = sensor.get();
  }

  auto it = requests_map_.find(type);
  CallbackQueue callback_queue = std::move(it->second);
  requests_map_.erase(it);

  FreeResourcesIfNeeded();

  // Inform subscribers about the sensor.
  // |sensor| can be nullptr here.
  for (auto& callback : callback_queue) {
    std::move(callback).Run(sensor);
  }
}

std::vector<mojom::SensorType>
PlatformSensorProvider::GetPendingRequestTypes() {
  std::vector<mojom::SensorType> request_types;
  for (auto const& entry : requests_map_) {
    request_types.push_back(entry.first);
  }
  return request_types;
}

SensorReadingSharedBuffer*
PlatformSensorProvider::GetSensorReadingSharedBufferForType(
    mojom::SensorType type) {
  auto* ptr = static_cast<char*>(mapped_region_.mapping.memory());
  if (!ptr) {
    return nullptr;
  }

  ptr += SensorReadingSharedBuffer::GetOffset(type);
  memset(ptr, 0, kReadingBufferSize);
  return reinterpret_cast<SensorReadingSharedBuffer*>(ptr);
}

#if BUILDFLAG(IS_WIN)
// static
bool PlatformSensorProvider::UseWindowsWinrt() {
  // TODO: Windows version dependency should eventually be updated to
  // a future version which supports WinRT sensor thresholding. Since
  // this Windows version has yet to be released, Win10 is being
  // provisionally used for testing. This also means sensors will
  // stream if this implementation path is enabled.

  // Note the fork occurs specifically on the 19H1 build of Win10
  // because a previous version (RS5) contains an access violation
  // issue in the WinRT APIs which causes the client code to crash.
  // See http://crbug.com/1063124
  return base::win::GetVersion() >= base::win::Version::WIN10_19H1;
}
#endif

}  // namespace device
