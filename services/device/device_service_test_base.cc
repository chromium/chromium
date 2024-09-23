// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/device_service_test_base.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "services/device/device_service.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"

#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
#include "services/device/public/cpp/geolocation/geolocation_system_permission_manager.h"
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)

namespace device {

namespace {

// Simply return a nullptr which means no CustomLocationProvider from embedder.
std::unique_ptr<LocationProvider> GetCustomLocationProviderForTest() {
  return nullptr;
}

std::unique_ptr<DeviceService> CreateTestDeviceService(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    mojo::PendingReceiver<mojom::DeviceService> receiver) {
  auto params = std::make_unique<DeviceServiceParams>();
  params->file_task_runner = std::move(file_task_runner);
  params->io_task_runner = std::move(io_task_runner);
  params->url_loader_factory = std::move(url_loader_factory);
  params->network_connection_tracker =
      network::TestNetworkConnectionTracker::GetInstance();
  params->geolocation_api_key = kTestGeolocationApiKey;
  params->custom_location_provider_callback =
      base::BindRepeating(&GetCustomLocationProviderForTest);
  params->geolocation_system_permission_manager =
      device::GeolocationSystemPermissionManager::GetInstance();

  return CreateDeviceService(std::move(params), std::move(receiver));
}

}  // namespace

DeviceServiceTestBase::DeviceServiceTestBase()
    : file_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})),
      io_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::USER_VISIBLE})),
      network_connection_tracker_(
          network::TestNetworkConnectionTracker::CreateInstance()) {}

DeviceServiceTestBase::~DeviceServiceTestBase() = default;

void DeviceServiceTestBase::SetUp() {
#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  auto geolocation_system_permission_manager =
      std::make_unique<FakeGeolocationSystemPermissionManager>();
  fake_geolocation_system_permission_manager_ =
      geolocation_system_permission_manager.get();
  device::GeolocationSystemPermissionManager::SetInstance(
      std::move(geolocation_system_permission_manager));
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  service_ = CreateTestDeviceService(
      file_task_runner_, io_task_runner_,
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_),
      service_remote_.BindNewPipeAndPassReceiver());
}

void DeviceServiceTestBase::DestroyDeviceService() {
  service_.reset();
}

}  // namespace device
