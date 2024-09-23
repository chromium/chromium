// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_cable_discovery.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/fido/cable/fido_ble_connection.h"
#include "device/fido/cable/fido_ble_uuids.h"
#include "device/fido/cable/fido_cable_device.h"
#include "device/fido/cable/fido_cable_handshake_handler.h"
#include "device/fido/cable/fido_tunnel_device.h"
#include "device/fido/features.h"
#include "device/fido/fido_parsing_utils.h"

#if BUILDFLAG(IS_MAC)
#include "device/fido/mac/util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#include "device/bluetooth/floss/floss_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace device {

namespace {

// Client name for logging in BLE scanning.
constexpr char kScanClientName[] = "FIDO";

// Construct advertisement data with different formats depending on client's
// operating system. Ideally, we advertise EIDs as part of Service Data, but
// this isn't available on all platforms. On Windows we use Manufacturer Data
// instead, and on Mac our only option is to advertise an additional service
// with the EID as its UUID.
std::unique_ptr<BluetoothAdvertisement::Data> ConstructAdvertisementData(
    base::span<const uint8_t, kCableEphemeralIdSize> client_eid) {
  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::AdvertisementType::ADVERTISEMENT_TYPE_BROADCAST);

#if BUILDFLAG(IS_MAC)
  BluetoothAdvertisement::UUIDList list;
  list.emplace_back(kGoogleCableUUID16);
  list.emplace_back(fido_parsing_utils::ConvertBytesToUuid(client_eid));
  advertisement_data->set_service_uuids(std::move(list));

#elif BUILDFLAG(IS_WIN)
  // References:
  // https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers
  // go/google-ble-manufacturer-data-format
  static constexpr uint16_t kGoogleManufacturerId = 0x00E0;
  static constexpr uint8_t kCableGoogleManufacturerDataType = 0x15;

  // Reference:
  // https://github.com/arnar/fido-2-specs/blob/fido-client-to-authenticator-protocol.bs#L4314
  static constexpr uint8_t kCableFlags = 0x20;

  static constexpr uint8_t kCableGoogleManufacturerDataLength =
      3u + kCableEphemeralIdSize;
  std::array<uint8_t, 4> kCableGoogleManufacturerDataHeader = {
      kCableGoogleManufacturerDataLength, kCableGoogleManufacturerDataType,
      kCableFlags, /*version=*/1};

  BluetoothAdvertisement::ManufacturerData manufacturer_data;
  std::vector<uint8_t> manufacturer_data_value;
  fido_parsing_utils::Append(&manufacturer_data_value,
                             kCableGoogleManufacturerDataHeader);
  fido_parsing_utils::Append(&manufacturer_data_value, client_eid);
  manufacturer_data.emplace(kGoogleManufacturerId,
                            std::move(manufacturer_data_value));
  advertisement_data->set_manufacturer_data(std::move(manufacturer_data));

#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Reference:
  // https://github.com/arnar/fido-2-specs/blob/fido-client-to-authenticator-protocol.bs#L4314
  static constexpr uint8_t kCableFlags = 0x20;

  // Service data for ChromeOS and Linux is 1 byte corresponding to Cable flags,
  // followed by 1 byte corresponding to Cable version number, followed by 16
  // bytes corresponding to client EID.
  BluetoothAdvertisement::ServiceData service_data;
  std::vector<uint8_t> service_data_value(18, 0);
  // Since the remainder of this service data field is a Cable EID, set the 5th
  // bit of the flag byte.
  service_data_value[0] = kCableFlags;
  service_data_value[1] = 1 /* version */;
  base::ranges::copy(client_eid, service_data_value.begin() + 2);
  service_data.emplace(kGoogleCableUUID128, std::move(service_data_value));
  advertisement_data->set_service_data(std::move(service_data));
#endif

  return advertisement_data;
}

}  // namespace

// FidoCableDiscovery::ObservedDeviceData -------------------------------------

FidoCableDiscovery::ObservedDeviceData::ObservedDeviceData() = default;
FidoCableDiscovery::ObservedDeviceData::~ObservedDeviceData() = default;

// FidoCableDiscovery ---------------------------------------------------------

FidoCableDiscovery::FidoCableDiscovery(
    std::vector<CableDiscoveryData> discovery_data)
    : FidoDeviceDiscovery(FidoTransportProtocol::kHybrid),
      discovery_data_(std::move(discovery_data)) {
// Windows currently does not support multiple EIDs, thus we ignore any extra
// discovery data.
// TODO(crbug.com/40573698): Add support for multiple EIDs on Windows.
#if BUILDFLAG(IS_WIN)
  if (discovery_data_.size() > 1u) {
    FIDO_LOG(ERROR) << "discovery_data_.size()=" << discovery_data_.size()
                    << ", trimming to 1.";
    discovery_data_.erase(discovery_data_.begin() + 1, discovery_data_.end());
  }
#endif
  for (const CableDiscoveryData& data : discovery_data_) {
    if (data.version != CableDiscoveryData::Version::V1) {
      continue;
    }
    has_v1_discovery_data_ = true;
    break;
  }
}

FidoCableDiscovery::~FidoCableDiscovery() {
  // Work around dangling advertisement references. (crbug/846522)
  for (auto advertisement : advertisements_) {
    advertisement.second->Unregister(base::DoNothing(), base::DoNothing());
  }

  if (adapter_)
    adapter_->RemoveObserver(this);
}

std::unique_ptr<FidoDiscoveryBase::EventStream<base::span<const uint8_t, 20>>>
FidoCableDiscovery::GetV2AdvertStream() {
  DCHECK(!advert_callback_);

  std::unique_ptr<EventStream<base::span<const uint8_t, 20>>> ret;
  std::tie(advert_callback_, ret) =
      EventStream<base::span<const uint8_t, 20>>::New();
  return ret;
}

std::unique_ptr<FidoCableHandshakeHandler>
FidoCableDiscovery::CreateV1HandshakeHandler(
    FidoCableDevice* device,
    const CableDiscoveryData& discovery_data,
    const CableEidArray& authenticator_eid) {
  std::unique_ptr<FidoCableHandshakeHandler> handler;
  switch (discovery_data.version) {
    case CableDiscoveryData::Version::V1: {
      // Nonce is embedded as first 8 bytes of client EID.
      std::array<uint8_t, 8> nonce;
      const bool ok = fido_parsing_utils::ExtractArray(
          discovery_data.v1->client_eid, 0, &nonce);
      DCHECK(ok);

      return std::make_unique<FidoCableV1HandshakeHandler>(
          device, nonce, discovery_data.v1->session_pre_key);
    }

    case CableDiscoveryData::Version::V2:
    case CableDiscoveryData::Version::INVALID:
      CHECK(false);
      return nullptr;
  }
}

// static
const BluetoothUUID& FidoCableDiscovery::GoogleCableUUID() {
  static const base::NoDestructor<BluetoothUUID> kUUID(kGoogleCableUUID128);
  return *kUUID;
}

const BluetoothUUID& FidoCableDiscovery::FIDOCableUUID() {
  static const base::NoDestructor<BluetoothUUID> kUUID(kFIDOCableUUID128);
  return *kUUID;
}

// static
bool FidoCableDiscovery::IsCableDevice(const BluetoothDevice* device) {
  const auto& uuid1 = GoogleCableUUID();
  const auto& uuid2 = FIDOCableUUID();
  return base::Contains(device->GetServiceData(), uuid1) ||
         base::Contains(device->GetUUIDs(), uuid1) ||
         base::Contains(device->GetServiceData(), uuid2) ||
         base::Contains(device->GetUUIDs(), uuid2);
}

void FidoCableDiscovery::OnGetAdapter(scoped_refptr<BluetoothAdapter> adapter) {
  if (!adapter->IsPresent()) {
    FIDO_LOG(DEBUG) << "No BLE adapter present";
    NotifyDiscoveryStarted(false);
    return;
  }

  DCHECK(!adapter_);
  adapter_ = std::move(adapter);
  DCHECK(adapter_);

  adapter_->AddObserver(this);
  BluetoothAdapter::PermissionStatus bluetooth_permission =
      BluetoothAdapter::PermissionStatus::kAllowed;
#if BUILDFLAG(IS_MAC)
  switch (fido::mac::ProcessIsSigned()) {
    case fido::mac::CodeSigningState::kSigned:
      bluetooth_permission = adapter_->GetOsPermissionStatus();
      FIDO_LOG(DEBUG) << "Bluetooth authorized: "
                      << static_cast<int>(bluetooth_permission);
      break;
    case fido::mac::CodeSigningState::kNotSigned:
      FIDO_LOG(DEBUG)
          << "Build not signed. Assuming Bluetooth permission is granted.";
      break;
  }
#endif

  if (bluetooth_permission == BluetoothAdapter::PermissionStatus::kAllowed) {
    FIDO_LOG(DEBUG) << "BLE adapter address " << adapter_->GetAddress();
    if (adapter_->IsPowered()) {
      OnSetPowered();
    }
  }

  // FidoCableDiscovery blocks its transport availability callback on the
  // DiscoveryStarted() calls of all instantiated discoveries. Hence, this call
  // must not be put behind the BLE adapter getting powered on (which is
  // dependent on the UI), or else the UI and this discovery will wait on each
  // other indefinitely (see crbug.com/1018416).
  NotifyDiscoveryStarted(true);
}

void FidoCableDiscovery::OnSetPowered() {
  DCHECK(adapter());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FidoCableDiscovery::StartCableDiscovery,
                                weak_factory_.GetWeakPtr()));
}

void FidoCableDiscovery::SetDiscoverySession(
    std::unique_ptr<BluetoothDiscoverySession> discovery_session) {
  discovery_session_ = std::move(discovery_session);
}

void FidoCableDiscovery::DeviceAdded(BluetoothAdapter* adapter,
                                     BluetoothDevice* device) {
  if (!IsCableDevice(device))
    return;

  CableDeviceFound(adapter, device);
}

void FidoCableDiscovery::DeviceChanged(BluetoothAdapter* adapter,
                                       BluetoothDevice* device) {
  if (!IsCableDevice(device))
    return;

  CableDeviceFound(adapter, device);
}

void FidoCableDiscovery::DeviceRemoved(BluetoothAdapter* adapter,
                                       BluetoothDevice* device) {
  const auto& device_address = device->GetAddress();
  if (IsCableDevice(device) &&
      // It only matters if V1 devices are "removed" because V2 devices do not
      // transport data over BLE.
      base::Contains(active_devices_, device_address)) {
    FIDO_LOG(DEBUG) << "caBLE device removed: " << device_address;
    RemoveDevice(FidoCableDevice::GetIdForAddress(device_address));
  }
}

void FidoCableDiscovery::AdapterPoweredChanged(BluetoothAdapter* adapter,
                                               bool powered) {
  if (!powered) {
    // In order to prevent duplicate client EIDs from being advertised when
    // BluetoothAdapter is powered back on, unregister all existing client
    // EIDs.
    StopAdvertisements(base::DoNothing());
    return;
  }

#if BUILDFLAG(IS_WIN)
  // On Windows, the power-on event appears to race against initialization of
  // the adapter, such that one of the WinRT API calls inside
  // BluetoothAdapter::StartDiscoverySessionWithFilter() can fail with "Device
  // not ready for use". So wait for things to actually be ready.
  // TODO(crbug.com/40670639): Remove this delay once the Bluetooth layer
  // handles the spurious failure.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FidoCableDiscovery::StartCableDiscovery,
                     weak_factory_.GetWeakPtr()),
      base::Milliseconds(500));
#else
  StartCableDiscovery();
#endif  // BUILDFLAG(IS_WIN)
}

void FidoCableDiscovery::AdapterDiscoveringChanged(BluetoothAdapter* adapter,
                                                   bool is_scanning) {
  FIDO_LOG(DEBUG) << "AdapterDiscoveringChanged() is_scanning=" << is_scanning;

  // Ignore updates while we're not scanning for caBLE devices ourselves. Other
  // things in Chrome may start or stop scans at any time.
  if (!discovery_session_) {
    return;
  }
}

#if BUILDFLAG(IS_CHROMEOS)
void FidoCableDiscovery::OnDeviceFound(
    device::BluetoothLowEnergyScanSession* scan_session,
    device::BluetoothDevice* device) {
  DeviceAdded(adapter_.get(), device);
}

void FidoCableDiscovery::OnDeviceLost(
    device::BluetoothLowEnergyScanSession* scan_session,
    device::BluetoothDevice* device) {
  DeviceRemoved(adapter_.get(), device);
}

void FidoCableDiscovery::OnSessionStarted(
    device::BluetoothLowEnergyScanSession* scan_session,
    std::optional<device::BluetoothLowEnergyScanSession::ErrorCode>
        error_code) {
  if (error_code) {
    FIDO_LOG(ERROR) << "Failed to start caBLE LE scan session, error_code = "
                    << static_cast<int>(error_code.value());
    le_scan_session_.reset();
    return;
  }

  FIDO_LOG(DEBUG) << "LE scan session started.";

  // Advertising is delayed by 500ms to ensure that any UI has a chance to
  // appear as we don't want to start broadcasting without the user being
  // aware.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FidoCableDiscovery::StartAdvertisement,
                     weak_factory_.GetWeakPtr()),
      base::Milliseconds(500));
}

void FidoCableDiscovery::OnSessionInvalidated(
    device::BluetoothLowEnergyScanSession* scan_session) {
  FIDO_LOG(EVENT) << "LE scan session invalidated";
  le_scan_session_.reset();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void FidoCableDiscovery::StartCableDiscovery() {
#if BUILDFLAG(IS_CHROMEOS)
  if (floss::features::IsFlossEnabled()) {
    device::BluetoothLowEnergyScanFilter::Pattern google_pattern(
        /*start_position=*/0,
        device::BluetoothLowEnergyScanFilter::AdvertisementDataType::
            kServiceData,
        /* kServiceData takes the 16-bit UUID as a little endian byte vector. */
        std::vector<uint8_t>{kGoogleCableUUID[3], kGoogleCableUUID[2]});
    device::BluetoothLowEnergyScanFilter::Pattern fido_pattern(
        /*start_position=*/0,
        device::BluetoothLowEnergyScanFilter::AdvertisementDataType::
            kServiceData,
        std::vector<uint8_t>{kFIDOCableUUID[3], kFIDOCableUUID[2]});
    auto filter = device::BluetoothLowEnergyScanFilter::Create(
        device::BluetoothLowEnergyScanFilter::Range::kFar,
        /*device_found_timeout=*/base::Seconds(1),
        /*device_lost_timeout=*/base::Seconds(7),
        {google_pattern, fido_pattern},
        /*rssi_sampling_period=*/base::Seconds(1));
    if (!filter) {
      FIDO_LOG(ERROR)
          << "Failed to start LE scanning due to failure to create filter.";
      return;
    }

    le_scan_session_ = adapter_->StartLowEnergyScanSession(
        std::move(filter), weak_factory_.GetWeakPtr());
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  adapter()->StartDiscoverySessionWithFilter(
      std::make_unique<BluetoothDiscoveryFilter>(
          BluetoothTransport::BLUETOOTH_TRANSPORT_LE),
      kScanClientName,
      base::BindOnce(&FidoCableDiscovery::OnStartDiscoverySession,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&FidoCableDiscovery::OnStartDiscoverySessionError,
                     weak_factory_.GetWeakPtr()));
}

void FidoCableDiscovery::OnStartDiscoverySession(
    std::unique_ptr<BluetoothDiscoverySession> session) {
  FIDO_LOG(DEBUG) << "Discovery session started.";
  SetDiscoverySession(std::move(session));
  // Advertising is delayed by 500ms to ensure that any UI has a chance to
  // appear as we don't want to start broadcasting without the user being
  // aware.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FidoCableDiscovery::StartAdvertisement,
                     weak_factory_.GetWeakPtr()),
      base::Milliseconds(500));
}

void FidoCableDiscovery::OnStartDiscoverySessionError() {
  FIDO_LOG(ERROR) << "Failed to start caBLE discovery";
}

void FidoCableDiscovery::StartAdvertisement() {
  DCHECK(adapter());
  bool advertisements_pending = false;
  for (const auto& data : discovery_data_) {
    if (data.version != CableDiscoveryData::Version::V1) {
      continue;
    }

    if (!advertisements_pending) {
      FIDO_LOG(DEBUG) << "Starting to advertise clientEIDs.";
      advertisements_pending = true;
    }
    adapter()->RegisterAdvertisement(
        ConstructAdvertisementData(data.v1->client_eid),
        base::BindOnce(&FidoCableDiscovery::OnAdvertisementRegistered,
                       weak_factory_.GetWeakPtr(), data.v1->client_eid),
        base::BindOnce([](BluetoothAdvertisement::ErrorCode error_code) {
          FIDO_LOG(ERROR) << "Failed to register advertisement: " << error_code;
        }));
  }
}

void FidoCableDiscovery::StopAdvertisements(base::OnceClosure callback) {
  // Destructing a BluetoothAdvertisement invokes its Unregister() method, but
  // there may be references to the advertisement outside this
  // FidoCableDiscovery (see e.g. crbug/846522). Hence, merely clearing
  // |advertisements_| is not sufficient; we need to manually invoke
  // Unregister() for every advertisement in order to stop them. On the other
  // hand, |advertisements_| must not be cleared before the Unregister()
  // callbacks return either, in case we do hold the only reference to a
  // BluetoothAdvertisement.
  FIDO_LOG(DEBUG) << "Stopping " << advertisements_.size()
                  << " caBLE advertisements";
  auto barrier_closure = base::BarrierClosure(
      advertisements_.size(),
      base::BindOnce(&FidoCableDiscovery::OnAdvertisementsStopped,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  auto error_closure = base::BindRepeating(
      [](base::RepeatingClosure cb, BluetoothAdvertisement::ErrorCode code) {
        FIDO_LOG(ERROR) << "BluetoothAdvertisement::Unregister() failed: "
                        << code;
        cb.Run();
      },
      barrier_closure);
  for (auto advertisement : advertisements_) {
    advertisement.second->Unregister(barrier_closure, error_closure);
  }
}

void FidoCableDiscovery::OnAdvertisementsStopped(base::OnceClosure callback) {
  FIDO_LOG(DEBUG) << "Advertisements stopped";
  advertisements_.clear();
  std::move(callback).Run();
}

void FidoCableDiscovery::OnAdvertisementRegistered(
    const CableEidArray& client_eid,
    scoped_refptr<BluetoothAdvertisement> advertisement) {
  FIDO_LOG(DEBUG) << "Advertisement registered";
  advertisements_.emplace(client_eid, std::move(advertisement));
}

void FidoCableDiscovery::CableDeviceFound(BluetoothAdapter* adapter,
                                          BluetoothDevice* device) {
  const std::string device_address = device->GetAddress();
  if (base::Contains(active_devices_, device_address)) {
    return;
  }

  std::optional<V1DiscoveryDataAndEID> v1_match = GetCableDiscoveryData(device);
  if (!v1_match) {
    return;
  }

  if (base::Contains(active_authenticator_eids_, v1_match->second)) {
    return;
  }
  active_authenticator_eids_.insert(v1_match->second);
  active_devices_.insert(device_address);

  FIDO_LOG(EVENT) << "Found new caBLEv1 device.";

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  // Speed up GATT service discovery on ChromeOS/BlueZ.
  // SetConnectionLatency() is NOTIMPLEMENTED() on other platforms.
  device->SetConnectionLatency(BluetoothDevice::CONNECTION_LATENCY_LOW,
                               base::DoNothing(), base::BindOnce([]() {
                                 FIDO_LOG(ERROR)
                                     << "SetConnectionLatency() failed";
                               }));
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

  auto cable_device =
      std::make_unique<FidoCableDevice>(adapter, device_address);

  std::unique_ptr<FidoCableHandshakeHandler> handshake_handler =
      CreateV1HandshakeHandler(cable_device.get(), v1_match->first,
                               v1_match->second);
  auto* const handshake_handler_ptr = handshake_handler.get();
  active_handshakes_.emplace_back(std::move(cable_device),
                                  std::move(handshake_handler));

  StopAdvertisements(
      base::BindOnce(&FidoCableDiscovery::ConductEncryptionHandshake,
                     weak_factory_.GetWeakPtr(), handshake_handler_ptr,
                     v1_match->first.version));
}

void FidoCableDiscovery::ConductEncryptionHandshake(
    FidoCableHandshakeHandler* handshake_handler,
    CableDiscoveryData::Version cable_version) {
  handshake_handler->InitiateCableHandshake(base::BindOnce(
      &FidoCableDiscovery::ValidateAuthenticatorHandshakeMessage,
      weak_factory_.GetWeakPtr(), cable_version, handshake_handler));
}

void FidoCableDiscovery::ValidateAuthenticatorHandshakeMessage(
    CableDiscoveryData::Version cable_version,
    FidoCableHandshakeHandler* handshake_handler,
    std::optional<std::vector<uint8_t>> handshake_response) {
  const bool ok = handshake_response.has_value() &&
                  handshake_handler->ValidateAuthenticatorHandshakeMessage(
                      *handshake_response);

  bool found = false;
  for (auto it = active_handshakes_.begin(); it != active_handshakes_.end();
       it++) {
    if (it->second.get() != handshake_handler) {
      continue;
    }

    found = true;
    if (ok) {
      AddDevice(std::move(it->first));
    }
    active_handshakes_.erase(it);
    break;
  }
  DCHECK(found);

  if (ok) {
    FIDO_LOG(DEBUG) << "Authenticator handshake validated";
  } else {
    FIDO_LOG(DEBUG) << "Authenticator handshake invalid";
  }
}

std::optional<FidoCableDiscovery::V1DiscoveryDataAndEID>
FidoCableDiscovery::GetCableDiscoveryData(const BluetoothDevice* device) {
  const std::vector<uint8_t>* service_data =
      device->GetServiceDataForUUID(GoogleCableUUID());
  if (!service_data) {
    service_data = device->GetServiceDataForUUID(FIDOCableUUID());
  }
  std::optional<CableEidArray> maybe_eid_from_service_data =
      MaybeGetEidFromServiceData(device);
  std::vector<CableEidArray> uuids = GetUUIDs(device);

  const std::string address = device->GetAddress();
  const auto it = observed_devices_.find(address);
  const bool known = it != observed_devices_.end();
  if (known) {
    std::unique_ptr<ObservedDeviceData>& data = it->second;
    if (maybe_eid_from_service_data == data->service_data &&
        uuids == data->uuids) {
      // Duplicate data. Ignore.
      return std::nullopt;
    }
  }

  // New or updated device information.
  if (known) {
    FIDO_LOG(DEBUG) << "Updated information for caBLE device " << address
                    << ":";
  } else {
    FIDO_LOG(DEBUG) << "New caBLE device " << address << ":";
  }

  std::optional<FidoCableDiscovery::V1DiscoveryDataAndEID> result;
  if (maybe_eid_from_service_data.has_value()) {
    result =
        GetCableDiscoveryDataFromAuthenticatorEid(*maybe_eid_from_service_data);
    FIDO_LOG(DEBUG) << "  Service data: "
                    << ResultDebugString(*maybe_eid_from_service_data, result);
  } else if (service_data) {
    FIDO_LOG(DEBUG) << "  Service data: " << base::HexEncode(*service_data);
  } else {
    FIDO_LOG(DEBUG) << "  Service data: <none>";
  }

  if (!uuids.empty()) {
    FIDO_LOG(DEBUG) << "  UUIDs:";
    for (const auto& uuid : uuids) {
      auto eid_result = GetCableDiscoveryDataFromAuthenticatorEid(uuid);
      FIDO_LOG(DEBUG) << "    " << ResultDebugString(uuid, eid_result);
      if (!result && eid_result) {
        result = std::move(eid_result);
      }
    }
  }

  std::array<uint8_t, 16 + 4> v2_advert;
  if (advert_callback_ && service_data &&
      service_data->size() == v2_advert.size()) {
    memcpy(v2_advert.data(), service_data->data(), v2_advert.size());
    advert_callback_.Run(v2_advert);
  }

  auto observed_data = std::make_unique<ObservedDeviceData>();
  observed_data->service_data = maybe_eid_from_service_data;
  observed_data->uuids = uuids;
  observed_devices_.insert_or_assign(address, std::move(observed_data));

  return result;
}

// static
std::optional<CableEidArray> FidoCableDiscovery::MaybeGetEidFromServiceData(
    const BluetoothDevice* device) {
  const std::vector<uint8_t>* service_data =
      device->GetServiceDataForUUID(GoogleCableUUID());
  if (!service_data) {
    return std::nullopt;
  }

  // Received service data from authenticator must have a flag that signals that
  // the service data includes Cable EID.
  if (service_data->empty() || !(service_data->at(0) >> 5 & 1u))
    return std::nullopt;

  CableEidArray received_authenticator_eid;
  bool extract_success = fido_parsing_utils::ExtractArray(
      *service_data, 2, &received_authenticator_eid);
  if (!extract_success)
    return std::nullopt;
  return received_authenticator_eid;
}

// static
std::vector<CableEidArray> FidoCableDiscovery::GetUUIDs(
    const BluetoothDevice* device) {
  std::vector<CableEidArray> ret;

  const auto service_uuids = device->GetUUIDs();
  for (const auto& uuid : service_uuids) {
    std::vector<uint8_t> uuid_binary = uuid.GetBytes();
    CableEidArray authenticator_eid;
    DCHECK_EQ(authenticator_eid.size(), uuid_binary.size());
    memcpy(authenticator_eid.data(), uuid_binary.data(),
           std::min(uuid_binary.size(), authenticator_eid.size()));

    ret.emplace_back(std::move(authenticator_eid));
  }

  return ret;
}

std::optional<FidoCableDiscovery::V1DiscoveryDataAndEID>
FidoCableDiscovery::GetCableDiscoveryDataFromAuthenticatorEid(
    CableEidArray authenticator_eid) {
  for (const auto& candidate : discovery_data_) {
    if (candidate.version == CableDiscoveryData::Version::V1 &&
        candidate.MatchV1(authenticator_eid)) {
      return V1DiscoveryDataAndEID(candidate, authenticator_eid);
    }
  }

  return std::nullopt;
}

void FidoCableDiscovery::StartInternal() {
  BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
      &FidoCableDiscovery::OnGetAdapter, weak_factory_.GetWeakPtr()));
}

// static
std::string FidoCableDiscovery::ResultDebugString(
    const CableEidArray& eid,
    const std::optional<FidoCableDiscovery::V1DiscoveryDataAndEID>& result) {
  static const uint8_t kAppleContinuity[16] = {
      0xd0, 0x61, 0x1e, 0x78, 0xbb, 0xb4, 0x45, 0x91,
      0xa5, 0xf8, 0x48, 0x79, 0x10, 0xae, 0x43, 0x66,
  };
  static const uint8_t kAppleUnknown[16] = {
      0x9f, 0xa4, 0x80, 0xe0, 0x49, 0x67, 0x45, 0x42,
      0x93, 0x90, 0xd3, 0x43, 0xdc, 0x5d, 0x04, 0xae,
  };
  static const uint8_t kAppleMedia[16] = {
      0x89, 0xd3, 0x50, 0x2b, 0x0f, 0x36, 0x43, 0x3a,
      0x8e, 0xf4, 0xc5, 0x02, 0xad, 0x55, 0xf8, 0xdc,
  };
  static const uint8_t kAppleNotificationCenter[16] = {
      0x79, 0x05, 0xf4, 0x31, 0xb5, 0xce, 0x4e, 0x99,
      0xa4, 0x0f, 0x4b, 0x1e, 0x12, 0x2d, 0x00, 0xd0,
  };
  static const uint8_t kCable[16] = {
      0x00, 0x00, 0xfd, 0xe2, 0x00, 0x00, 0x10, 0x00,
      0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb,
  };

  std::string ret = base::HexEncode(eid) + "";

  if (!result) {
    // Try to identify some common UUIDs that are random and thus otherwise look
    // like potential EIDs.
    if (memcmp(eid.data(), kAppleContinuity, eid.size()) == 0) {
      ret += " (Apple Continuity service)";
    } else if (memcmp(eid.data(), kAppleUnknown, eid.size()) == 0) {
      ret += " (Apple service)";
    } else if (memcmp(eid.data(), kAppleMedia, eid.size()) == 0) {
      ret += " (Apple Media service)";
    } else if (memcmp(eid.data(), kAppleNotificationCenter, eid.size()) == 0) {
      ret += " (Apple Notification service)";
    } else if (memcmp(eid.data(), kCable, eid.size()) == 0) {
      ret += " (caBLE indicator)";
    }
    return ret;
  }

  if (result) {
    ret += " (version one match)";
  }

  return ret;
}

void FidoCableDiscovery::Stop() {
  FidoDeviceDiscovery::Stop();
  StopAdvertisements(base::DoNothing());
}

}  // namespace device
