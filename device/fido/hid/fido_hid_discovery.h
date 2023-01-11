// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_HID_FIDO_HID_DISCOVERY_H_
#define DEVICE_FIDO_HID_FIDO_HID_DISCOVERY_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/fido_device_discovery.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/hid/hid_device_filter.h"
#include "services/device/public/mojom/hid.mojom-forward.h"

namespace device {

// VidPid represents an HID vendor and product ID pair.
struct COMPONENT_EXPORT(DEVICE_FIDO) VidPid {
  uint16_t vid;
  uint16_t pid;
};

COMPONENT_EXPORT(DEVICE_FIDO)
bool operator==(const VidPid& lhs, const VidPid& rhs);
COMPONENT_EXPORT(DEVICE_FIDO)
bool operator<(const VidPid& lhs, const VidPid& rhs);

class COMPONENT_EXPORT(DEVICE_FIDO) FidoHidDiscovery
    : public FidoDeviceDiscovery,
      device::mojom::HidManagerClient {
 public:
  explicit FidoHidDiscovery(base::flat_set<VidPid> ignore_list = {});

  FidoHidDiscovery(const FidoHidDiscovery&) = delete;
  FidoHidDiscovery& operator=(const FidoHidDiscovery&) = delete;

  ~FidoHidDiscovery() override;

  // Sets a callback for this class to use when binding a HidManager receiver.
  using HidManagerBinder =
      base::RepeatingCallback<void(mojo::PendingReceiver<mojom::HidManager>)>;
  static void SetHidManagerBinder(HidManagerBinder binder);

 private:
  // FidoDeviceDiscovery:
  void StartInternal() override;

  // device::mojom::HidManagerClient implementation:
  void DeviceAdded(device::mojom::HidDeviceInfoPtr device_info) override;
  void DeviceRemoved(device::mojom::HidDeviceInfoPtr device_info) override;
  void DeviceChanged(device::mojom::HidDeviceInfoPtr device_info) override;

  void OnGetDevices(std::vector<device::mojom::HidDeviceInfoPtr> devices);

  mojo::Remote<device::mojom::HidManager> hid_manager_;
  mojo::AssociatedReceiver<device::mojom::HidManagerClient> receiver_{this};
  HidDeviceFilter filter_;
  base::flat_set<VidPid> ignore_list_;
  base::WeakPtrFactory<FidoHidDiscovery> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_HID_FIDO_HID_DISCOVERY_H_
