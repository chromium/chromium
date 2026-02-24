// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/v2_discovery.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_discovery_filter.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/fido/cable/fido_ble_uuids.h"
#include "device/fido/cable/fido_tunnel_device.h"
#include "device/fido/cable/pairing.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/public/cable_discovery_data.h"
#include "device/fido/public/features.h"
#include "third_party/boringssl/src/include/openssl/aes.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#include "device/bluetooth/floss/floss_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
#include "device/fido/mac/util.h"
#endif

namespace device::cablev2 {

namespace {

// Client name for logging in BLE scanning.
constexpr char kScanClientName[] = "FIDO";

// CableV2DiscoveryEvent enumerates several steps that occur while listening for
// BLE adverts. Do not change the assigned values since they are used in
// histograms, only append new values. Keep synced with enums.xml.
enum class CableV2DiscoveryEvent {
  kStarted = 0,
  kHavePairings = 1,
  kHaveQRKeys = 2,
  kHaveExtensionKeys = 3,
  kTunnelMatch = 4,
  kQRMatch = 5,
  kExtensionMatch = 6,
  kNoMatch = 7,

  kMaxValue = 7,
};

void RecordEvent(CableV2DiscoveryEvent event) {
  base::UmaHistogramEnumeration("WebAuthentication.CableV2.DiscoveryEvent",
                                event);
}

}  // namespace

Discovery::Discovery(
    FidoRequestType request_type,
    NetworkContextFactory network_context_factory,
    std::optional<base::span<const uint8_t, kQRKeySize>> qr_generator_key,
    std::unique_ptr<EventStream<std::unique_ptr<Pairing>>>
        contact_device_stream,
    const std::vector<CableDiscoveryData>& extension_contents,
    std::optional<base::RepeatingCallback<void(std::unique_ptr<Pairing>)>>
        pairing_callback,
    std::optional<base::RepeatingCallback<void(std::unique_ptr<Pairing>)>>
        invalidated_pairing_callback,
    std::optional<base::RepeatingCallback<void(Event)>> event_callback,
    bool must_support_ctap)
    : FidoDeviceDiscovery(FidoTransportProtocol::kHybrid),
      request_type_(request_type),
      network_context_factory_(std::move(network_context_factory)),
      qr_keys_(KeysFromQRGeneratorKey(qr_generator_key)),
      extension_keys_(KeysFromExtension(extension_contents)),
      contact_device_stream_(std::move(contact_device_stream)),
      pairing_callback_(std::move(pairing_callback)),
      invalidated_pairing_callback_(std::move(invalidated_pairing_callback)),
      event_callback_(std::move(event_callback)),
      must_support_ctap_(must_support_ctap) {
  static_assert(kQRKeySize == kQRSecretSize + kQRSeedSize);

  if (contact_device_stream_) {
    contact_device_stream_->Connect(base::BindRepeating(
        &Discovery::OnContactDevice, base::Unretained(this)));
  }
}

Discovery::~Discovery() {
  if (adapter_) {
    adapter_->RemoveObserver(this);
  }
}

void Discovery::StartInternal() {
  CHECK(!started_);

  BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&Discovery::OnGetAdapter, weak_factory_.GetWeakPtr()));

  RecordEvent(CableV2DiscoveryEvent::kStarted);
  if (pairing_callback_) {
    // The pairing callback is null if there are no pairings.
    RecordEvent(CableV2DiscoveryEvent::kHavePairings);
  }
  if (qr_keys_) {
    RecordEvent(CableV2DiscoveryEvent::kHaveQRKeys);
  }
  if (!extension_keys_.empty()) {
    RecordEvent(CableV2DiscoveryEvent::kHaveExtensionKeys);
  }

  started_ = true;
}

void Discovery::GetDiscoveryData(const BluetoothDevice* device) {
  const std::vector<uint8_t>* service_data =
      device->GetServiceDataForUUID(GoogleCableUUID());
  if (!service_data) {
    service_data = device->GetServiceDataForUUID(FIDOCableUUID());
  }

  if (service_data && service_data->size() == kAdvertSize) {
    OnBLEAdvertSeen(*(base::span<const uint8_t>(*service_data)
                          .to_fixed_extent<kAdvertSize>()));
  }
}

void Discovery::OnBLEAdvertSeen(base::span<const uint8_t, kAdvertSize> advert) {
  const std::array<uint8_t, kAdvertSize> advert_array =
      fido_parsing_utils::Materialize<kAdvertSize>(advert);

  if (device_committed_) {
    // A device has already been accepted. Ignore other adverts.
    return;
  }

  if (observed_adverts_.contains(advert_array)) {
    return;
  }
  observed_adverts_.insert(advert_array);

  // Check whether the EID satisfies any pending tunnels.
  for (std::vector<std::unique_ptr<FidoTunnelDevice>>::iterator i =
           tunnels_pending_advert_.begin();
       i != tunnels_pending_advert_.end(); i++) {
    if (!(*i)->MatchAdvert(advert_array)) {
      continue;
    }

    RecordEvent(CableV2DiscoveryEvent::kTunnelMatch);
    FIDO_LOG(DEBUG) << "  (" << base::HexEncode(advert)
                    << " matches pending tunnel)";
    std::unique_ptr<FidoTunnelDevice> device(std::move(*i));
    tunnels_pending_advert_.erase(i);
    device_committed_ = true;
    if (event_callback_) {
      event_callback_->Run(Event::kBLEAdvertReceived);
    }
    AddDevice(std::move(device));
    return;
  }

  if (qr_keys_) {
    // Check whether the EID matches a QR code.
    std::optional<CableEidArray> plaintext =
        eid::Decrypt(advert_array, qr_keys_->eid_key);
    if (plaintext) {
      FIDO_LOG(DEBUG) << "  (" << base::HexEncode(advert)
                      << " matches QR code)";
      RecordEvent(CableV2DiscoveryEvent::kQRMatch);
      device_committed_ = true;
      if (event_callback_) {
        event_callback_->Run(Event::kBLEAdvertReceived);
      }
      AddDevice(std::make_unique<FidoTunnelDevice>(
          network_context_factory_, pairing_callback_, event_callback_,
          qr_keys_->qr_secret, qr_keys_->local_identity_seed, *plaintext,
          must_support_ctap_));
      return;
    }
  }
  // Check whether the EID matches the extension.
  for (const auto& extension : extension_keys_) {
    std::optional<CableEidArray> plaintext =
        eid::Decrypt(advert_array, extension.eid_key);
    if (plaintext) {
      FIDO_LOG(DEBUG) << "  (" << base::HexEncode(advert)
                      << " matches extension)";
      RecordEvent(CableV2DiscoveryEvent::kExtensionMatch);
      device_committed_ = true;
      AddDevice(std::make_unique<cablev2::FidoTunnelDevice>(
          network_context_factory_, base::DoNothing(), event_callback_,
          extension.qr_secret, extension.local_identity_seed, *plaintext,
          must_support_ctap_));
      return;
    }
  }

  RecordEvent(CableV2DiscoveryEvent::kNoMatch);
  FIDO_LOG(DEBUG) << "  (" << base::HexEncode(advert) << ": no v2 match)";
}

void Discovery::OnContactDevice(std::unique_ptr<Pairing> pairing) {
  auto pairing_copy = std::make_unique<Pairing>(*pairing);
  tunnels_pending_advert_.emplace_back(std::make_unique<FidoTunnelDevice>(
      request_type_, network_context_factory_, std::move(pairing),
      base::BindOnce(&Discovery::PairingIsInvalid, weak_factory_.GetWeakPtr(),
                     std::move(pairing_copy)),
      event_callback_));
}

void Discovery::PairingIsInvalid(std::unique_ptr<Pairing> pairing) {
  if (!invalidated_pairing_callback_) {
    return;
  }

  invalidated_pairing_callback_->Run(std::move(pairing));
}

// static
std::optional<Discovery::UnpairedKeys> Discovery::KeysFromQRGeneratorKey(
    std::optional<base::span<const uint8_t, kQRKeySize>> qr_generator_key) {
  if (!qr_generator_key) {
    return std::nullopt;
  }

  UnpairedKeys ret;
  static_assert(kQRKeySize == kQRSeedSize + kQRSecretSize);
  ret.local_identity_seed = fido_parsing_utils::Materialize(
      qr_generator_key->subspan<0, kQRSeedSize>());
  ret.qr_secret = fido_parsing_utils::Materialize(
      qr_generator_key->subspan<kQRSeedSize, kQRSecretSize>());
  ret.eid_key = Derive<ret.eid_key.size()>(
      ret.qr_secret, base::span<const uint8_t>(), DerivedValueType::kEIDKey);
  return ret;
}

// static
std::vector<Discovery::UnpairedKeys> Discovery::KeysFromExtension(
    const std::vector<CableDiscoveryData>& extension_contents) {
  std::vector<Discovery::UnpairedKeys> ret;

  for (auto const& data : extension_contents) {
    if (data.version != CableDiscoveryData::Version::V2) {
      continue;
    }

    auto sized_server_link_data_span =
        base::span(data.v2->server_link_data).to_fixed_extent<kQRKeySize>();
    if (!sized_server_link_data_span.has_value()) {
      FIDO_LOG(ERROR) << "caBLEv2 extension has incorrect length ("
                      << data.v2->server_link_data.size() << ")";
      continue;
    }

    if (std::optional<Discovery::UnpairedKeys> keys =
            KeysFromQRGeneratorKey(sized_server_link_data_span.value())) {
      ret.emplace_back(*std::move(keys));
    }
  }

  return ret;
}

// static
const BluetoothUUID& Discovery::GoogleCableUUID() {
  static const base::NoDestructor<BluetoothUUID> kUUID(kGoogleCableUUID128);
  return *kUUID;
}

const BluetoothUUID& Discovery::FIDOCableUUID() {
  static const base::NoDestructor<BluetoothUUID> kUUID(kFIDOCableUUID128);
  return *kUUID;
}

// static
bool Discovery::IsCableDevice(const BluetoothDevice* device) {
  const auto& uuid1 = GoogleCableUUID();
  const auto& uuid2 = FIDOCableUUID();
  return device->GetServiceData().contains(uuid1) ||
         device->GetUUIDs().contains(uuid1) ||
         device->GetServiceData().contains(uuid2) ||
         device->GetUUIDs().contains(uuid2);
}

// static
std::vector<CableEidArray> Discovery::GetUUIDs(const BluetoothDevice* device) {
  std::vector<CableEidArray> ret;

  const auto service_uuids = device->GetUUIDs();
  for (const auto& uuid : service_uuids) {
    std::vector<uint8_t> uuid_binary = uuid.GetBytes();
    CableEidArray authenticator_eid;
    CHECK_EQ(authenticator_eid.size(), uuid_binary.size());
    base::span(authenticator_eid).copy_from(uuid_binary);
    ret.emplace_back(std::move(authenticator_eid));
  }

  return ret;
}

void Discovery::OnGetAdapter(scoped_refptr<BluetoothAdapter> bt_adapter) {
  if (!bt_adapter->IsPresent()) {
    FIDO_LOG(DEBUG) << "No BLE adapter present";
    NotifyDiscoveryStarted(false);
    return;
  }

  CHECK(!adapter());
  adapter_ = std::move(bt_adapter);
  CHECK(adapter());

  adapter()->AddObserver(this);
  BluetoothAdapter::PermissionStatus bluetooth_permission =
      BluetoothAdapter::PermissionStatus::kAllowed;
#if BUILDFLAG(IS_MAC)
  switch (fido::mac::ProcessIsSigned()) {
    case fido::mac::CodeSigningState::kSigned:
      bluetooth_permission = adapter()->GetOsPermissionStatus();
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
    FIDO_LOG(DEBUG) << "BLE adapter address " << adapter()->GetAddress();
    if (adapter()->IsPowered()) {
      OnSetPowered();
    }
  }

  // Discovery blocks its transport availability callback on the
  // DiscoveryStarted() calls of all instantiated discoveries. Hence, this call
  // must not be put behind the BLE adapter getting powered on (which is
  // dependent on the UI), or else the UI and this discovery will wait on each
  // other indefinitely (see crbug.com/1018416).
  NotifyDiscoveryStarted(true);
}

void Discovery::OnSetPowered() {
  CHECK(adapter());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&Discovery::StartCableDiscovery,
                                weak_factory_.GetWeakPtr()));
}

void Discovery::SetDiscoverySession(
    std::unique_ptr<BluetoothDiscoverySession> discovery_session) {
  discovery_session_ = std::move(discovery_session);
}

void Discovery::DeviceAdded(BluetoothAdapter* adapter,
                            BluetoothDevice* device) {
  if (!IsCableDevice(device)) {
    return;
  }

  GetDiscoveryData(device);
}

void Discovery::DeviceChanged(BluetoothAdapter* adapter,
                              BluetoothDevice* device) {
  if (!IsCableDevice(device)) {
    return;
  }

  GetDiscoveryData(device);
}

void Discovery::AdapterPoweredChanged(BluetoothAdapter* adapter, bool powered) {
#if BUILDFLAG(IS_WIN)
  // On Windows, the power-on event appears to race against initialization of
  // the adapter, such that one of the WinRT API calls inside
  // BluetoothAdapter::StartDiscoverySessionWithFilter() can fail with "Device
  // not ready for use". So wait for things to actually be ready.
  // TODO(crbug.com/40670639): Remove this delay once the Bluetooth layer
  // handles the spurious failure.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&Discovery::StartCableDiscovery,
                     weak_factory_.GetWeakPtr()),
      base::Milliseconds(500));
#else
  StartCableDiscovery();
#endif  // BUILDFLAG(IS_WIN)
}

#if BUILDFLAG(IS_CHROMEOS)
void Discovery::OnDeviceFound(
    device::BluetoothLowEnergyScanSession* scan_session,
    device::BluetoothDevice* device) {
  DeviceAdded(adapter(), device);
}

void Discovery::OnDeviceLost(
    device::BluetoothLowEnergyScanSession* scan_session,
    device::BluetoothDevice* device) {
  DeviceRemoved(adapter(), device);
}

void Discovery::OnSessionStarted(
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
}

void Discovery::OnSessionInvalidated(
    device::BluetoothLowEnergyScanSession* scan_session) {
  FIDO_LOG(EVENT) << "LE scan session invalidated";
  le_scan_session_.reset();
}

#endif  // BUILDFLAG(IS_CHROMEOS)

void Discovery::StartCableDiscovery() {
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

    le_scan_session_ = adapter()->StartLowEnergyScanSession(
        std::move(filter), weak_factory_.GetWeakPtr());
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  adapter()->StartDiscoverySessionWithFilter(
      std::make_unique<BluetoothDiscoveryFilter>(
          BluetoothTransport::BLUETOOTH_TRANSPORT_LE),
      kScanClientName,
      base::BindOnce(&Discovery::OnStartDiscoverySession,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&Discovery::OnStartDiscoverySessionError,
                     weak_factory_.GetWeakPtr()));
}

void Discovery::OnStartDiscoverySession(
    std::unique_ptr<BluetoothDiscoverySession> session) {
  FIDO_LOG(DEBUG) << "Discovery session started.";
  SetDiscoverySession(std::move(session));
}

void Discovery::OnStartDiscoverySessionError() {
  FIDO_LOG(ERROR) << "Failed to start caBLE discovery";
}

}  // namespace device::cablev2
