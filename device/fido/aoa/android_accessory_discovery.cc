// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/aoa/android_accessory_discovery.h"

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/aoa/android_accessory_device.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"

#include <utility>

#include "base/functional/bind.h"

// See https://source.android.com/devices/accessories/aoa for details on the
// protocol used to talk to apps on the phone here.

namespace device {

namespace {

// AOADiscoveryEvent enumerates several steps that occur during AOA discovery.
// Do not change the assigned values since they are used in histograms, only
// append new values. Keep synced with enums.xml.
enum class AOADiscoveryEvent {
  kStarted = 0,
  kAOADeviceObserved = 1,
  kNonAOADeviceObserved = 2,
  kBadInterface = 3,
  kAOAOpenFailed = 4,
  kAOAConfigurationFailed = 5,
  kAOAInterfaceFailed = 6,
  kAOAWriteFailed = 7,
  kAOAReadFailed = 8,
  kAOADeviceDiscovered = 9,
  kOpenFailed = 10,
  kVersionFailed = 11,
  kBadVersion = 12,
  kConfigurationFailed = 13,
  kAOARequested = 14,
  kPreviousDeviceFound = 15,

  kMaxValue = 15,
};

void RecordEvent(AOADiscoveryEvent event) {
  base::UmaHistogramEnumeration("WebAuthentication.CableV2.AOADiscoveryEvent",
                                event);
}

// KnownAccessories returns a global that stores the GUIDs of USB devices that
// we have previously put into accessory mode and, if still connected, can be
// used immediately. (GUIDs are not a USB concept, the Chromium USB layer
// generates them to identity a specific USB connection.)
base::flat_set<std::string>& KnownAccessories() {
  static base::NoDestructor<base::flat_set<std::string>> set;
  return *set;
}

}  // namespace

AndroidAccessoryDiscovery::AndroidAccessoryDiscovery(
    mojo::Remote<device::mojom::UsbDeviceManager> device_manager,
    std::string request_description)
    : FidoDeviceDiscovery(FidoTransportProtocol::kUsbHumanInterfaceDevice),
      device_manager_(std::move(device_manager)),
      request_description_(std::move(request_description)) {}

AndroidAccessoryDiscovery::~AndroidAccessoryDiscovery() = default;

void AndroidAccessoryDiscovery::StartInternal() {
  FIDO_LOG(DEBUG) << "Android accessory discovery started";
  RecordEvent(AOADiscoveryEvent::kStarted);

  device_manager_->EnumerateDevicesAndSetClient(
      receiver_.BindNewEndpointAndPassRemote(),
      base::BindOnce(&AndroidAccessoryDiscovery::OnGetDevices,
                     weak_factory_.GetWeakPtr()));
}

void AndroidAccessoryDiscovery::OnDeviceAdded(
    device::mojom::UsbDeviceInfoPtr device_info) {
  if (device_info->class_code != 0 || device_info->subclass_code != 0) {
    FIDO_LOG(DEBUG) << "Ignoring new USB device with class: "
                    << device_info->class_code
                    << " subclass: " << device_info->subclass_code;
    return;
  }

  mojo::Remote<device::mojom::UsbDevice> device;
  device_manager_->GetSecurityKeyDevice(device_info->guid,
                                        device.BindNewPipeAndPassReceiver(),
                                        /*device_client=*/mojo::NullRemote());

  auto* device_ptr = device.get();
  if (device_info->vendor_id == 0x18d1 &&
      (device_info->product_id & ~1) == 0x2d00) {
    RecordEvent(AOADiscoveryEvent::kAOADeviceObserved);
    HandleAccessoryDevice(std::move(device), std::move(device_info));
    return;
  }

  RecordEvent(AOADiscoveryEvent::kNonAOADeviceObserved);
  // Attempt to reconfigure the device into accessory mode.
  device_ptr->Open(base::BindOnce(&AndroidAccessoryDiscovery::OnOpen,
                                  weak_factory_.GetWeakPtr(),
                                  std::move(device)));
}

static absl::optional<AndroidAccessoryDiscovery::InterfaceInfo>
FindAccessoryInterface(const device::mojom::UsbDeviceInfoPtr& device_info) {
  for (const device::mojom::UsbConfigurationInfoPtr& config :
       device_info->configurations) {
    for (const device::mojom::UsbInterfaceInfoPtr& interface :
         config->interfaces) {
      if (interface->alternates.empty()) {
        // I don't believe that this is possible in USB.
        continue;
      }

      const device::mojom::UsbAlternateInterfaceInfoPtr& info =
          interface->alternates[0];
      if (info->class_code == 0xff && info->subclass_code == 0xff &&
          info->endpoints.size() == 2) {
        // This is the AOA interface. (ADB, if enabled, has a subclass of 66.)
        absl::optional<uint8_t> in_endpoint_num;
        absl::optional<uint8_t> out_endpoint_num;

        for (const device::mojom::UsbEndpointInfoPtr& endpoint :
             info->endpoints) {
          if (endpoint->direction == mojom::UsbTransferDirection::INBOUND) {
            in_endpoint_num = endpoint->endpoint_number;
          } else {
            out_endpoint_num = endpoint->endpoint_number;
          }
        }

        if (!in_endpoint_num || !out_endpoint_num) {
          continue;
        }

        return AndroidAccessoryDiscovery::InterfaceInfo{
            .configuration = config->configuration_value,
            .interface = interface->interface_number,
            .in_endpoint = *in_endpoint_num,
            .out_endpoint = *out_endpoint_num,
            .guid = device_info->guid,
        };
      }
    }
  }

  return absl::nullopt;
}

void AndroidAccessoryDiscovery::HandleAccessoryDevice(
    mojo::Remote<device::mojom::UsbDevice> device,
    device::mojom::UsbDeviceInfoPtr device_info) {
  auto interface_info = FindAccessoryInterface(device_info);
  if (!interface_info) {
    RecordEvent(AOADiscoveryEvent::kBadInterface);
    FIDO_LOG(ERROR) << "Failed to find accessory interface on device";
    return;
  }

  auto* device_ptr = device.get();
  device_ptr->Open(base::BindOnce(&AndroidAccessoryDiscovery::OnOpenAccessory,
                                  weak_factory_.GetWeakPtr(), std::move(device),
                                  std::move(device_info), *interface_info));
}

enum AccessoryControlRequest : uint8_t {
  kGetProtocol = 51,
  kSendString = 52,
  kStart = 53,
};

static mojom::UsbControlTransferParamsPtr ControlTransferParams(
    AccessoryControlRequest request,
    uint16_t index = 0) {
  auto ret = mojom::UsbControlTransferParams::New();
  ret->type = mojom::UsbControlTransferType::VENDOR;
  ret->recipient = mojom::UsbControlTransferRecipient::DEVICE;
  ret->request = request;
  ret->value = 0;
  ret->index = index;

  return ret;
}

static constexpr unsigned kTimeoutMilliseconds = 1000;
static constexpr unsigned kLongTimeoutMilliseconds = 90 * 1000;

void AndroidAccessoryDiscovery::OnOpenAccessory(
    mojo::Remote<device::mojom::UsbDevice> device,
    device::mojom::UsbDeviceInfoPtr device_info,
    InterfaceInfo interface_info,
    device::mojom::UsbOpenDeviceResultPtr result) {
  if (result->is_error()) {
    switch (result->get_error()) {
      case mojom::UsbOpenDeviceError::ALREADY_OPEN:
        break;
      default:
        FIDO_LOG(DEBUG) << "Failed to open accessory device. Ignoring.";
        RecordEvent(AOADiscoveryEvent::kAOAOpenFailed);
        return;
    }
  }

  FIDO_LOG(DEBUG) << "Accessory USB device opened";

  if (interface_info.configuration != device_info->active_configuration) {
    FIDO_LOG(DEBUG) << "Setting device configuration "
                    << interface_info.configuration;
    auto* device_ptr = device.get();
    device_ptr->SetConfiguration(
        interface_info.configuration,
        base::BindOnce(&AndroidAccessoryDiscovery::OnAccessoryConfigured,
                       weak_factory_.GetWeakPtr(), std::move(device),
                       interface_info));
    return;
  }

  OnAccessoryConfigured(std::move(device), interface_info, /*success=*/true);
}

void AndroidAccessoryDiscovery::OnAccessoryConfigured(
    mojo::Remote<device::mojom::UsbDevice> device,
    InterfaceInfo interface_info,
    bool success) {
  if (!success) {
    FIDO_LOG(DEBUG) << "Failed to set configuration on an accessory device";
    RecordEvent(AOADiscoveryEvent::kAOAConfigurationFailed);
    return;
  }

  auto* device_ptr = device.get();
  device_ptr->ClaimInterface(
      interface_info.interface,
      base::BindOnce(&AndroidAccessoryDiscovery::OnAccessoryInterfaceClaimed,
                     weak_factory_.GetWeakPtr(), std::move(device),
                     interface_info));
}

void AndroidAccessoryDiscovery::OnAccessoryInterfaceClaimed(
    mojo::Remote<device::mojom::UsbDevice> device,
    InterfaceInfo interface_info,
    mojom::UsbClaimInterfaceResult result) {
  if (result != mojom::UsbClaimInterfaceResult::kSuccess) {
    FIDO_LOG(DEBUG) << "Failed to claim interface on an accessory device";
    RecordEvent(AOADiscoveryEvent::kAOAInterfaceFailed);
    return;
  }

  std::array<uint8_t, kSyncNonceLength> nonce;
  base::RandBytes(&nonce[0], kSyncNonceLength);

  std::vector<uint8_t> packet;
  packet.push_back(AndroidAccessoryDevice::kCoaoaSync);
  packet.insert(packet.end(), nonce.begin(), nonce.end());

  auto* device_ptr = device.get();
  const uint8_t out_endpoint = interface_info.out_endpoint;
  device_ptr->GenericTransferOut(
      out_endpoint, std::move(packet), kLongTimeoutMilliseconds,
      base::BindOnce(&AndroidAccessoryDiscovery::OnSyncWritten,
                     weak_factory_.GetWeakPtr(), std::move(device),
                     std::move(interface_info), nonce));
}

void AndroidAccessoryDiscovery::OnSyncWritten(
    mojo::Remote<device::mojom::UsbDevice> device,
    InterfaceInfo interface_info,
    std::array<uint8_t, kSyncNonceLength> nonce,
    mojom::UsbTransferStatus result) {
  if (result != mojom::UsbTransferStatus::COMPLETED) {
    FIDO_LOG(ERROR) << "Failed to write to USB device ("
                    << static_cast<int>(result) << ").";
    RecordEvent(AOADiscoveryEvent::kAOAWriteFailed);
    return;
  }

  FIDO_LOG(DEBUG) << "Awaiting response to sync message";

  auto* device_ptr = device.get();
  const uint8_t in_endpoint = interface_info.in_endpoint;
  device_ptr->GenericTransferIn(
      in_endpoint, kSyncMessageLength, kTimeoutMilliseconds,
      base::BindOnce(&AndroidAccessoryDiscovery::OnReadComplete,
                     weak_factory_.GetWeakPtr(), std::move(device),
                     std::move(interface_info), nonce));
}

void AndroidAccessoryDiscovery::OnReadComplete(
    mojo::Remote<device::mojom::UsbDevice> device,
    InterfaceInfo interface_info,
    std::array<uint8_t, kSyncNonceLength> nonce,
    mojom::UsbTransferStatus result,
    base::span<const uint8_t> payload) {
  // BABBLE results if the message from the USB peer was longer than expected.
  // That's fine because we're expecting potentially discard some messages in
  // order to find the sync message.
  if (result != mojom::UsbTransferStatus::COMPLETED &&
      result != mojom::UsbTransferStatus::BABBLE) {
    FIDO_LOG(ERROR) << "Failed to read from USB device ("
                    << static_cast<int>(result) << ").";
    RecordEvent(AOADiscoveryEvent::kAOAReadFailed);
    return;
  }

  if (result == mojom::UsbTransferStatus::COMPLETED &&
      payload.size() == kSyncMessageLength &&
      payload[0] == AndroidAccessoryDevice::kCoaoaSync &&
      memcmp(&payload[1], nonce.data(), kSyncNonceLength) == 0) {
    FIDO_LOG(DEBUG) << "Accessory device discovered";
    RecordEvent(AOADiscoveryEvent::kAOADeviceDiscovered);
    KnownAccessories().insert(interface_info.guid);
    AddDevice(std::make_unique<AndroidAccessoryDevice>(
        std::move(device), interface_info.in_endpoint,
        interface_info.out_endpoint));
    return;
  }

  auto* device_ptr = device.get();
  const uint8_t in_endpoint = interface_info.in_endpoint;
  device_ptr->GenericTransferIn(
      in_endpoint, kSyncMessageLength, kTimeoutMilliseconds,
      base::BindOnce(&AndroidAccessoryDiscovery::OnReadComplete,
                     weak_factory_.GetWeakPtr(), std::move(device),
                     std::move(interface_info), nonce));
}

void AndroidAccessoryDiscovery::OnOpen(
    mojo::Remote<device::mojom::UsbDevice> device,
    device::mojom::UsbOpenDeviceResultPtr result) {
  if (result->is_error()) {
    switch (result->get_error()) {
      case mojom::UsbOpenDeviceError::ALREADY_OPEN:
        break;
      default:
        FIDO_LOG(DEBUG) << "Failed to open USB device. Ignoring.";
        RecordEvent(AOADiscoveryEvent::kOpenFailed);
        return;
    }
  }

  auto* device_ptr = device.get();
  device_ptr->ControlTransferIn(
      ControlTransferParams(kGetProtocol),
      /* reply length */ 2, kTimeoutMilliseconds,
      base::BindOnce(&AndroidAccessoryDiscovery::OnVersionReply,
                     weak_factory_.GetWeakPtr(), std::move(device)));
}

void AndroidAccessoryDiscovery::OnVersionReply(
    mojo::Remote<device::mojom::UsbDevice> device,
    device::mojom::UsbTransferStatus status,
    base::span<const uint8_t> payload) {
  if (status != mojom::UsbTransferStatus::COMPLETED || payload.size() != 2) {
    RecordEvent(AOADiscoveryEvent::kVersionFailed);
    FIDO_LOG(DEBUG) << "Android AOA version request failed with status: "
                    << static_cast<unsigned>(status)
                    << " payload.size: " << payload.size()
                    << ". Ignoring device.";
    return;
  }

  const uint16_t version = static_cast<uint16_t>(payload[0]) |
                           (static_cast<uint16_t>(payload[1]) << 8);

  if (version == 0) {
    RecordEvent(AOADiscoveryEvent::kBadVersion);
    FIDO_LOG(DEBUG)
        << "Android AOA version one not supported. Ignoring device.";
    return;
  }

  OnConfigurationStepComplete(std::move(device), 0,
                              mojom::UsbTransferStatus::COMPLETED);
}

static std::vector<uint8_t> VectorFromString(const char* str) {
  return std::vector<uint8_t>(
      reinterpret_cast<const uint8_t*>(str),
      reinterpret_cast<const uint8_t*>(str + strlen(str) + 1));
}

void AndroidAccessoryDiscovery::OnConfigurationStepComplete(
    mojo::Remote<device::mojom::UsbDevice> device,
    unsigned step,
    device::mojom::UsbTransferStatus status) {
  if (status != mojom::UsbTransferStatus::COMPLETED) {
    RecordEvent(AOADiscoveryEvent::kConfigurationFailed);
    FIDO_LOG(DEBUG) << "Android AOA configuration failed at step " << step;
    return;
  }

  // The semantics of each step number are defined at
  // https://source.android.com/devices/accessories/aoa#attempt-to-start-in-accessory-mode
  auto* device_ptr = device.get();
  std::vector<uint8_t> encoded_string;
  switch (step) {
    case 0:
      // Manufacturer.
      encoded_string = VectorFromString("Chromium");
      break;

    case 1:
      // Model.
      encoded_string = VectorFromString(
          device::mojom::UsbControlTransferParams::kSecurityKeyAOAModel);
      break;

    case 2:
      encoded_string = VectorFromString(request_description_.c_str());
      break;

    case 3:
      // Version. Always some value as a version in order to avoid a potential
      // Android crash. See https://crbug.com/1174217.
      encoded_string = VectorFromString("1");
      break;

    case 4:
      // Finished sending strings; request switch to AOA mode.
      device_ptr->ControlTransferOut(
          ControlTransferParams(kStart), {}, kTimeoutMilliseconds,
          base::BindOnce(
              &AndroidAccessoryDiscovery::OnConfigurationStepComplete,
              weak_factory_.GetWeakPtr(), std::move(device), step + 1));
      return;

    case 5:
      RecordEvent(AOADiscoveryEvent::kAOARequested);
      FIDO_LOG(DEBUG) << "Device requested to switch to accessory mode";
      return;

    default:
      CHECK(false);
  }

  device_ptr->ControlTransferOut(
      ControlTransferParams(kSendString, step), encoded_string,
      kTimeoutMilliseconds,
      base::BindOnce(&AndroidAccessoryDiscovery::OnConfigurationStepComplete,
                     weak_factory_.GetWeakPtr(), std::move(device), step + 1));
}

void AndroidAccessoryDiscovery::OnDeviceRemoved(
    device::mojom::UsbDeviceInfoPtr device_info) {}

void AndroidAccessoryDiscovery::OnGetDevices(
    std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
  base::flat_set<std::string>& known_guids(KnownAccessories());
  base::flat_set<std::string> still_known_guids;
  bool event_recorded = false;

  for (auto& device_info : devices) {
    const std::string& guid = device_info->guid;
    if (!base::Contains(known_guids, guid)) {
      continue;
    }

    still_known_guids.insert(guid);
    FIDO_LOG(DEBUG) << "Previously opened accessory device found.";
    if (!event_recorded) {
      RecordEvent(AOADiscoveryEvent::kPreviousDeviceFound);
      event_recorded = true;
    }

    mojo::Remote<device::mojom::UsbDevice> device;
    device_manager_->GetSecurityKeyDevice(guid,
                                          device.BindNewPipeAndPassReceiver(),
                                          /*device_client=*/mojo::NullRemote());

    HandleAccessoryDevice(std::move(device), std::move(device_info));
  }

  // The global |known_guids| is updated to remove any GUIDs that have
  // disappeared so that it doesn't grow over time.
  known_guids.swap(still_known_guids);

  // Other devices attached at the time that the discovery is started are
  // ignored because we don't want to send USB vendor commands to random
  // devices.
  NotifyDiscoveryStarted(true);
}

}  // namespace device
