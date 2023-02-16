// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth_low_energy/bluetooth_api_advertisement.h"

#include "base/lazy_instance.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "net/base/io_buffer.h"

namespace extensions {

// static
static base::LazyInstance<BrowserContextKeyedAPIFactory<
    ApiResourceManager<BluetoothApiAdvertisement>>>::DestructorAtExit
    g_server_factory = LAZY_INSTANCE_INITIALIZER;

// static
template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<BluetoothApiAdvertisement>>*
ApiResourceManager<BluetoothApiAdvertisement>::GetFactoryInstance() {
  return g_server_factory.Pointer();
}

BluetoothApiAdvertisement::BluetoothApiAdvertisement(
    const std::string& owner_extension_id,
    scoped_refptr<device::BluetoothAdvertisement> advertisement)
    : ApiResource(owner_extension_id), advertisement_(advertisement) {
  DCHECK_CURRENTLY_ON(kThreadId);
}

BluetoothApiAdvertisement::~BluetoothApiAdvertisement() = default;

}  // namespace extensions
