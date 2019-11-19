// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/device_service_test_base.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/device/device_service.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/service_manager/public/cpp/binder_registry.h"

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
    service_manager::mojom::ServiceRequest request) {
#if defined(OS_ANDROID)
  return CreateDeviceService(
      file_task_runner, io_task_runner, url_loader_factory,
      network::TestNetworkConnectionTracker::GetInstance(),
      kTestGeolocationApiKey, false, WakeLockContextCallback(),
      base::BindRepeating(&GetCustomLocationProviderForTest), nullptr,
      std::move(request));
#else
  return CreateDeviceService(
      file_task_runner, io_task_runner, url_loader_factory,
      network::TestNetworkConnectionTracker::GetInstance(),
      kTestGeolocationApiKey,
      base::BindRepeating(&GetCustomLocationProviderForTest),
      std::move(request));
#endif
}

}  // namespace

DeviceServiceTestBase::DeviceServiceTestBase()
    : file_task_runner_(base::CreateSingleThreadTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT})),
      io_task_runner_(base::CreateSingleThreadTaskRunner(
          {base::ThreadPool(), base::TaskPriority::USER_VISIBLE})),
      network_connection_tracker_(
          network::TestNetworkConnectionTracker::CreateInstance()),
      connector_(test_connector_factory_.CreateConnector()) {}

DeviceServiceTestBase::~DeviceServiceTestBase() = default;

void DeviceServiceTestBase::SetUp() {
  service_ = CreateTestDeviceService(
      file_task_runner_, io_task_runner_,
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_),
      test_connector_factory_.RegisterInstance(mojom::kServiceName));
}

void DeviceServiceTestBase::DestroyDeviceService() {
  service_.reset();
}

}  // namespace device
