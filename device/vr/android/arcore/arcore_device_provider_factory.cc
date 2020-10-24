// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/arcore/arcore_device_provider_factory.h"

#include "base/logging.h"
#include "device/vr/public/cpp/vr_device_provider.h"

namespace device {

namespace {
ArCoreDeviceProviderFactory* g_arcore_device_provider_factory = nullptr;
bool create_called = false;
}  // namespace

// static
std::unique_ptr<device::VRDeviceProvider>
ArCoreDeviceProviderFactory::Create() {
  DVLOG(2) << __func__;

  create_called = true;
  if (!g_arcore_device_provider_factory)
    return nullptr;
  return g_arcore_device_provider_factory->CreateDeviceProvider();
}

// static
void ArCoreDeviceProviderFactory::Install(
    std::unique_ptr<ArCoreDeviceProviderFactory> factory) {
  DVLOG(2) << __func__;

  DCHECK_NE(g_arcore_device_provider_factory, factory.get());
  if (g_arcore_device_provider_factory) {
    delete g_arcore_device_provider_factory;
  } else if (create_called) {
    // TODO(crbug.com/1050470): Remove this logging after investigation.
    // We only hit this code path if "Install" is called for the first time
    // after a call to ArCoreDeviceProviderFactory::Create. This indicates that
    // we previously did not return an ARCoreDeviceProvider, but the system we
    // are on actually *can* support an ARCoreDeviceProvider.
    LOG(ERROR) << "AR provider factory installed after create";
  }

  g_arcore_device_provider_factory = factory.release();
}

}  // namespace device
