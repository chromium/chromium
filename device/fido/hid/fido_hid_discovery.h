// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_HID_FIDO_HID_DISCOVERY_H_
#define DEVICE_FIDO_HID_FIDO_HID_DISCOVERY_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/fido_device_discovery.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/hid/hid_device_filter.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace service_manager {
class Connector;
}

namespace device {

// TODO(crbug/769631): Now the U2F is talking to HID via mojo, once the U2F
// servicification is unblocked, we'll move U2F back to //service/device/.
// Then it will talk to HID via C++ as part of servicifying U2F.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoHidDiscovery
    : public FidoDeviceDiscovery,
      device::mojom::HidManagerClient {
 public:
  explicit FidoHidDiscovery(::service_manager::Connector* connector);
  ~FidoHidDiscovery() override;

 private:
  // FidoDeviceDiscovery:
  void StartInternal() override;

  // device::mojom::HidManagerClient implementation:
  void DeviceAdded(device::mojom::HidDeviceInfoPtr device_info) override;
  void DeviceRemoved(device::mojom::HidDeviceInfoPtr device_info) override;

  void OnGetDevices(std::vector<device::mojom::HidDeviceInfoPtr> devices);

  service_manager::Connector* connector_;
  mojo::Remote<device::mojom::HidManager> hid_manager_;
  mojo::AssociatedReceiver<device::mojom::HidManagerClient> receiver_{this};
  HidDeviceFilter filter_;
  base::WeakPtrFactory<FidoHidDiscovery> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FidoHidDiscovery);
};

}  // namespace device

#endif  // DEVICE_FIDO_HID_FIDO_HID_DISCOVERY_H_
