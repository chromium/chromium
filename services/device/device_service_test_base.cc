// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/device_service_test_base.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "services/device/device_service.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"

#if BUILDFLAG(IS_MAC)
#include "services/device/public/cpp/test/fake_geolocation_manager.h"
#endif

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
    mojo::PendingReceiver<mojom::DeviceService> receiver,
    GeolocationManager* geolocation_manager) {
  auto params = std::make_unique<DeviceServiceParams>();
  params->file_task_runner = std::move(file_task_runner);
  params->io_task_runner = std::move(io_task_runner);
  params->url_loader_factory = std::move(url_loader_factory);
  params->network_connection_tracker =
      network::TestNetworkConnectionTracker::GetInstance();
  params->geolocation_api_key = kTestGeolocationApiKey;
  params->custom_location_provider_callback =
      base::BindRepeating(&GetCustomLocationProviderForTest);
  params->geolocation_manager = geolocation_manager;

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
  GeolocationManager* geolocation_manager = nullptr;
#if BUILDFLAG(IS_MAC)
  fake_geolocation_manager_ = std::make_unique<FakeGeolocationManager>();
  geolocation_manager = fake_geolocation_manager_.get();
#endif
  service_ = CreateTestDeviceService(
      file_task_runner_, io_task_runner_,
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_),
      service_remote_.BindNewPipeAndPassReceiver(), geolocation_manager);
}

void DeviceServiceTestBase::DestroyDeviceService() {
  service_.reset();
}

}  // namespace device
