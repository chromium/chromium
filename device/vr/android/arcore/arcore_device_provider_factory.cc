// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/arcore/arcore_device_provider_factory.h"

#include "base/logging.h"
#include "device/vr/vr_device_provider.h"

namespace device {

namespace {
ArCoreDeviceProviderFactory* g_arcore_device_provider_factory = nullptr;
}  // namespace

// static
std::unique_ptr<device::VRDeviceProvider>
ArCoreDeviceProviderFactory::Create() {
  if (!g_arcore_device_provider_factory)
    return nullptr;
  return g_arcore_device_provider_factory->CreateDeviceProvider();
}

// static
void ArCoreDeviceProviderFactory::Install(
    std::unique_ptr<ArCoreDeviceProviderFactory> factory) {
  DCHECK_NE(g_arcore_device_provider_factory, factory.get());
  if (g_arcore_device_provider_factory)
    delete g_arcore_device_provider_factory;
  g_arcore_device_provider_factory = factory.release();
}

}  // namespace device
