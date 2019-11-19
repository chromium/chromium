// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_cable_discovery.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/random.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/fido/ble/fido_ble_uuids.h"
#include "device/fido/cable/fido_cable_device.h"
#include "device/fido/cable/fido_cable_handshake_handler.h"
#include "device/fido/features.h"
#include "device/fido/fido_parsing_utils.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace device {

namespace {

// Construct advertisement data with different formats depending on client's
// operating system. Ideally, we advertise EIDs as part of Service Data, but
// this isn't available on all platforms. On Windows we use Manufacturer Data
// instead, and on Mac our only option is to advertise an additional service
// with the EID as its UUID.
std::unique_ptr<BluetoothAdvertisement::Data> ConstructAdvertisementData(
    base::span<const uint8_t, kCableEphemeralIdSize> client_eid) {
  auto advertisement_data = std::make_unique<BluetoothAdvertisement::Data>(
      BluetoothAdvertisement::AdvertisementType::ADVERTISEMENT_TYPE_BROADCAST);

#if defined(OS_MACOSX)
  auto list = std::make_unique<BluetoothAdvertisement::UUIDList>();
  list->emplace_back(kCableAdvertisementUUID16);
  list->emplace_back(fido_parsing_utils::ConvertBytesToUuid(client_eid));
  advertisement_data->set_service_uuids(std::move(list));

#elif defined(OS_WIN)
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

  auto manufacturer_data =
      std::make_unique<BluetoothAdvertisement::ManufacturerData>();
  std::vector<uint8_t> manufacturer_data_value;
  fido_parsing_utils::Append(&manufacturer_data_value,
                             kCableGoogleManufacturerDataHeader);
  fido_parsing_utils::Append(&manufacturer_data_value, client_eid);
  manufacturer_data->emplace(kGoogleManufacturerId,
                             std::move(manufacturer_data_value));
  advertisement_data->set_manufacturer_data(std::move(manufacturer_data));

#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
  // Reference:
  // https://github.com/arnar/fido-2-specs/blob/fido-client-to-authenticator-protocol.bs#L4314
  static constexpr uint8_t kCableFlags = 0x20;

  // Service data for ChromeOS and Linux is 1 byte corresponding to Cable flags,
  // followed by 1 byte corresponding to Cable version number, followed by 16
  // bytes corresponding to client EID.
  auto service_data = std::make_unique<BluetoothAdvertisement::ServiceData>();
  std::vector<uint8_t> service_data_value(18, 0);
  // Since the remainder of this service data field is a Cable EID, set the 5th
  // bit of the flag byte.
  service_data_value[0] = kCableFlags;
  service_data_value[1] = 1 /* version */;
  std::copy(client_eid.begin(), client_eid.end(),
            service_data_value.begin() + 2);
  service_data->emplace(kCableAdvertisementUUID128,
                        std::move(service_data_value));
  advertisement_data->set_service_data(std::move(service_data));
#endif

  return advertisement_data;
}

}  // namespace

// CableDiscoveryData -------------------------------------

CableDiscoveryData::CableDiscoveryData() = default;

CableDiscoveryData::CableDiscoveryData(
    CableDiscoveryData::Version version,
    const CableEidArray& client_eid,
    const CableEidArray& authenticator_eid,
    const CableSessionPreKeyArray& session_pre_key)
    : version(version) {
  CHECK_EQ(Version::V1, version);
  v1.emplace();
  v1->client_eid = client_eid;
  v1->authenticator_eid = authenticator_eid;
  v1->session_pre_key = session_pre_key;
}

CableDiscoveryData::CableDiscoveryData(
    base::span<const uint8_t, kCableQRSecretSize> qr_secret) {
  version = Version::V2;
  v2.emplace();

  static const char kEIDGen[] = "caBLE QR to EID generator key";
  bool ok =
      HKDF(v2->eid_gen_key.data(), v2->eid_gen_key.size(), EVP_sha256(),
           qr_secret.data(), qr_secret.size(), /*salt=*/nullptr, 0,
           reinterpret_cast<const uint8_t*>(kEIDGen), sizeof(kEIDGen) - 1);
  DCHECK(ok);

  static const char kPSKGen[] = "caBLE QR to PSK generator key";
  ok = HKDF(v2->psk_gen_key.data(), v2->psk_gen_key.size(), EVP_sha256(),
            qr_secret.data(), qr_secret.size(), /*salt=*/nullptr, 0,
            reinterpret_cast<const uint8_t*>(kPSKGen), sizeof(kPSKGen) - 1);
  DCHECK(ok);
}

CableDiscoveryData::CableDiscoveryData(const CableDiscoveryData& data) =
    default;

CableDiscoveryData& CableDiscoveryData::operator=(
    const CableDiscoveryData& other) = default;

CableDiscoveryData::~CableDiscoveryData() = default;

bool CableDiscoveryData::operator==(const CableDiscoveryData& other) const {
  if (version != other.version) {
    return false;
  }

  switch (version) {
    case CableDiscoveryData::Version::V1:
      return v1->client_eid == other.v1->client_eid &&
             v1->authenticator_eid == other.v1->authenticator_eid &&
             v1->session_pre_key == other.v1->session_pre_key;

    case CableDiscoveryData::Version::V2:
      return v2->eid_gen_key == other.v2->eid_gen_key &&
             v2->psk_gen_key == other.v2->psk_gen_key &&
             v2->peer_identity == other.v2->peer_identity &&
             v2->peer_name == other.v2->peer_name;

    case CableDiscoveryData::Version::INVALID:
      CHECK(false);
      return false;
  }
}

base::Optional<CableNonce> CableDiscoveryData::Match(
    const CableEidArray& eid) const {
  switch (version) {
    case Version::V1: {
      if (eid != v1->authenticator_eid) {
        return base::nullopt;
      }

      // The nonce is the first eight bytes of the EID.
      CableNonce nonce;
      const bool ok =
          fido_parsing_utils::ExtractArray(v1->client_eid, 0, &nonce);
      DCHECK(ok);
      return nonce;
    }

    case Version::V2: {
      // Attempt to decrypt the EID with the EID generator key and check whether
      // it has a valid structure.
      AES_KEY key;
      CHECK(AES_set_decrypt_key(v2->eid_gen_key.data(),
                                /*bits=*/8 * v2->eid_gen_key.size(),
                                &key) == 0);
      static_assert(kCableEphemeralIdSize == AES_BLOCK_SIZE,
                    "EIDs are not AES blocks");
      CableEidArray decrypted;
      AES_decrypt(/*in=*/eid.data(), /*out=*/decrypted.data(), &key);
      const uint8_t kZeroTrailer[8] = {0};
      static_assert(8 + sizeof(kZeroTrailer) ==
                        std::tuple_size<decltype(decrypted)>::value,
                    "Trailer is wrong size");
      if (CRYPTO_memcmp(kZeroTrailer, decrypted.data() + 8,
                        sizeof(kZeroTrailer)) != 0) {
        return base::nullopt;
      }

      CableNonce nonce;
      static_assert(
          sizeof(nonce) <= std::tuple_size<decltype(decrypted)>::value,
          "nonce too large");
      memcpy(nonce.data(), decrypted.data(), sizeof(nonce));
      return nonce;
    }

    case Version::INVALID:
      DCHECK(false);
      return base::nullopt;
  }
}

// static
QRGeneratorKey CableDiscoveryData::NewQRKey() {
  QRGeneratorKey key;
  crypto::RandBytes(key.data(), key.size());
  return key;
}

// static
int64_t CableDiscoveryData::CurrentTimeTick() {
  // The ticks are currently 256ms.
  return base::TimeTicks::Now().since_origin().InMilliseconds() >> 8;
}

// static
std::array<uint8_t, kCableQRSecretSize> CableDiscoveryData::DeriveQRSecret(
    base::span<const uint8_t, 32> qr_generator_key,
    const int64_t tick) {
  union {
    int64_t i;
    uint8_t bytes[8];
  } current_tick;
  current_tick.i = tick;

  std::array<uint8_t, kCableQRSecretSize> ret;
  bool ok = HKDF(ret.data(), ret.size(), EVP_sha256(), qr_generator_key.data(),
                 qr_generator_key.size(),
                 /*salt=*/nullptr, 0, current_tick.bytes, sizeof(current_tick));
  DCHECK(ok);
  return ret;
}

CableDiscoveryData::V2Data::V2Data() = default;
CableDiscoveryData::V2Data::V2Data(const V2Data&) = default;
CableDiscoveryData::V2Data::~V2Data() = default;

// FidoCableDiscovery::Result -------------------------------------------------

FidoCableDiscovery::Result::Result() = default;

FidoCableDiscovery::Result::Result(const CableDiscoveryData& in_discovery_data,
                                   const CableNonce& in_nonce,
                                   const CableEidArray& in_eid,
                                   base::Optional<int> in_ticks_back)
    : discovery_data(in_discovery_data),
      nonce(in_nonce),
      eid(in_eid),
      ticks_back(in_ticks_back) {}

FidoCableDiscovery::Result::Result(const Result& other) = default;

FidoCableDiscovery::Result::~Result() = default;

// FidoCableDiscovery::ObservedDeviceData -------------------------------------

FidoCableDiscovery::ObservedDeviceData::ObservedDeviceData() = default;
FidoCableDiscovery::ObservedDeviceData::~ObservedDeviceData() = default;

// FidoCableDiscovery ---------------------------------------------------------

FidoCableDiscovery::FidoCableDiscovery(
    std::vector<CableDiscoveryData> discovery_data,
    base::Optional<QRGeneratorKey> qr_generator_key,
    base::Optional<
        base::RepeatingCallback<void(std::unique_ptr<CableDiscoveryData>)>>
        pairing_callback)
    : FidoBleDiscoveryBase(
          FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy),
      discovery_data_(std::move(discovery_data)),
      qr_generator_key_(std::move(qr_generator_key)),
      pairing_callback_(std::move(pairing_callback)) {
// Windows currently does not support multiple EIDs, thus we ignore any extra
// discovery data.
// TODO(https://crbug.com/837088): Add support for multiple EIDs on Windows.
#if defined(OS_WIN)
  if (discovery_data_.size() > 1u)
    discovery_data_.erase(discovery_data_.begin() + 1, discovery_data_.end());
#endif
}

// This is a workaround for https://crbug.com/846522
FidoCableDiscovery::~FidoCableDiscovery() {
  for (auto advertisement : advertisements_)
    advertisement.second->Unregister(base::DoNothing(), base::DoNothing());
}

base::Optional<std::unique_ptr<FidoCableHandshakeHandler>>
FidoCableDiscovery::CreateHandshakeHandler(
    FidoCableDevice* device,
    const CableDiscoveryData& discovery_data,
    const CableNonce& nonce,
    const CableEidArray& eid) {
  std::unique_ptr<FidoCableHandshakeHandler> handler;
  switch (discovery_data.version) {
    case CableDiscoveryData::Version::V1: {
      // Nonce is embedded as first 8 bytes of client EID.
      std::array<uint8_t, 8> nonce;
      const bool ok = fido_parsing_utils::ExtractArray(
          discovery_data.v1->client_eid, 0, &nonce);
      DCHECK(ok);

      handler.reset(new FidoCableV1HandshakeHandler(
          device, nonce, discovery_data.v1->session_pre_key));
      break;
    }

    case CableDiscoveryData::Version::V2: {
      if (!base::FeatureList::IsEnabled(device::kWebAuthPhoneSupport)) {
        return base::nullopt;
      }
      if (!pairing_callback_) {
        FIDO_LOG(DEBUG) << "Discarding caBLE v2 handshake because of missing "
                           "pairing callback";
        return base::nullopt;
      }

      handler.reset(new FidoCableV2HandshakeHandler(
          device, discovery_data.v2->psk_gen_key, nonce, eid,
          discovery_data.v2->peer_identity, *pairing_callback_));
      break;
    }

    case CableDiscoveryData::Version::INVALID:
      CHECK(false);
      return base::nullopt;
  }

  return handler;
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
  if (IsCableDevice(device) && GetCableDiscoveryData(device)) {
    const auto& device_address = device->GetAddress();
    FIDO_LOG(DEBUG) << "caBLE device removed: " << device_address;
    RemoveDevice(FidoBleDevice::GetIdForAddress(device_address));
  }
}

void FidoCableDiscovery::AdapterPoweredChanged(BluetoothAdapter* adapter,
                                               bool powered) {
  // If Bluetooth adapter is powered on, resume scanning for nearby Cable
  // devices and start advertising client EIDs.
  if (powered) {
    StartCableDiscovery();
  } else {
    // In order to prevent duplicate client EIDs from being advertised when
    // BluetoothAdapter is powered back on, unregister all existing client
    // EIDs.
    StopAdvertisements(base::DoNothing());
  }
}

void FidoCableDiscovery::OnSetPowered() {
  DCHECK(adapter());

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FidoCableDiscovery::StartCableDiscovery,
                                weak_factory_.GetWeakPtr()));
}

void FidoCableDiscovery::StartCableDiscovery() {
  // Error callback OnStartDiscoverySessionError() is defined in the base class
  // FidoBleDiscoveryBase.
  adapter()->StartDiscoverySessionWithFilter(
      std::make_unique<BluetoothDiscoveryFilter>(
          BluetoothTransport::BLUETOOTH_TRANSPORT_LE),
      base::AdaptCallbackForRepeating(
          base::BindOnce(&FidoCableDiscovery::OnStartDiscoverySessionWithFilter,
                         weak_factory_.GetWeakPtr())),
      base::AdaptCallbackForRepeating(
          base::BindOnce(&FidoCableDiscovery::OnStartDiscoverySessionError,
                         weak_factory_.GetWeakPtr())));
}

void FidoCableDiscovery::OnStartDiscoverySessionWithFilter(
    std::unique_ptr<BluetoothDiscoverySession> session) {
  SetDiscoverySession(std::move(session));
  FIDO_LOG(DEBUG) << "Discovery session started.";
  // Advertising is delayed by 500ms to ensure that any UI has a chance to
  // appear as we don't want to start broadcasting without the user being
  // aware.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FidoCableDiscovery::StartAdvertisement,
                     weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(500));
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
        base::AdaptCallbackForRepeating(
            base::BindOnce(&FidoCableDiscovery::OnAdvertisementRegistered,
                           weak_factory_.GetWeakPtr(), data.v1->client_eid)),
        base::AdaptCallbackForRepeating(
            base::BindOnce(&FidoCableDiscovery::OnAdvertisementRegisterError,
                           weak_factory_.GetWeakPtr())));
  }
}

void FidoCableDiscovery::StopAdvertisements(base::OnceClosure callback) {
  auto barrier_closure =
      base::BarrierClosure(advertisement_success_counter_, std::move(callback));
  for (auto advertisement : advertisements_) {
    advertisement.second->Unregister(barrier_closure, base::DoNothing());
    FIDO_LOG(DEBUG) << "Stopped caBLE advertisement.";
  }

#if !defined(OS_WIN)
  // On Windows the discovery is the only owner of the advertisements, meaning
  // the advertisements would be destroyed before |barrier_closure| could be
  // invoked.
  advertisements_.clear();
#endif  // !defined(OS_WIN)
}

void FidoCableDiscovery::OnAdvertisementRegistered(
    const CableEidArray& client_eid,
    scoped_refptr<BluetoothAdvertisement> advertisement) {
  FIDO_LOG(DEBUG) << "Advertisement registered.";
  advertisements_.emplace(client_eid, std::move(advertisement));
  RecordAdvertisementResult(true /* is_success */);
}

void FidoCableDiscovery::OnAdvertisementRegisterError(
    BluetoothAdvertisement::ErrorCode error_code) {
  FIDO_LOG(ERROR) << "Failed to register advertisement: " << error_code;
  RecordAdvertisementResult(false /* is_success */);
}

void FidoCableDiscovery::RecordAdvertisementResult(bool is_success) {
  // If at least one advertisement succeeds, then notify discovery start.
  if (is_success) {
    advertisement_success_counter_++;
  } else {
    advertisement_failure_counter_++;
  }
}

void FidoCableDiscovery::CableDeviceFound(BluetoothAdapter* adapter,
                                          BluetoothDevice* device) {
  const std::string device_address = device->GetAddress();
  if (base::Contains(active_devices_, device_address)) {
    return;
  }

  base::Optional<Result> maybe_result = GetCableDiscoveryData(device);
  if (!maybe_result ||
      base::Contains(active_authenticator_eids_, maybe_result->eid)) {
    return;
  }

  FIDO_LOG(EVENT) << "Found new caBLE device.";
  active_devices_.insert(device_address);
  active_authenticator_eids_.insert(maybe_result->eid);

  auto cable_device =
      std::make_unique<FidoCableDevice>(adapter, device->GetAddress());
  StopAdvertisements(
      base::BindOnce(&FidoCableDiscovery::ConductEncryptionHandshake,
                     weak_factory_.GetWeakPtr(), std::move(cable_device),
                     std::move(*maybe_result)));
}

void FidoCableDiscovery::ConductEncryptionHandshake(
    std::unique_ptr<FidoCableDevice> cable_device,
    FidoCableDiscovery::Result result) {
  base::Optional<std::unique_ptr<FidoCableHandshakeHandler>> handshake_handler =
      CreateHandshakeHandler(cable_device.get(), result.discovery_data,
                             result.nonce, result.eid);
  if (!handshake_handler) {
    return;
  }
  auto* const handshake_handler_ptr = handshake_handler->get();
  cable_handshake_handlers_.emplace_back(std::move(*handshake_handler));

  handshake_handler_ptr->InitiateCableHandshake(
      base::BindOnce(&FidoCableDiscovery::ValidateAuthenticatorHandshakeMessage,
                     weak_factory_.GetWeakPtr(), std::move(cable_device),
                     handshake_handler_ptr));
}

void FidoCableDiscovery::ValidateAuthenticatorHandshakeMessage(
    std::unique_ptr<FidoCableDevice> cable_device,
    FidoCableHandshakeHandler* handshake_handler,
    base::Optional<std::vector<uint8_t>> handshake_response) {
  if (!handshake_response)
    return;

  if (handshake_handler->ValidateAuthenticatorHandshakeMessage(
          *handshake_response)) {
    FIDO_LOG(DEBUG) << "Authenticator handshake validated";
    AddDevice(std::move(cable_device));
  } else {
    FIDO_LOG(DEBUG) << "Authenticator handshake invalid";
  }
}

base::Optional<FidoCableDiscovery::Result>
FidoCableDiscovery::GetCableDiscoveryData(const BluetoothDevice* device) const {
  base::Optional<CableEidArray> maybe_eid_from_service_data =
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
      return base::nullopt;
    }
  }

  auto data = std::make_unique<ObservedDeviceData>();
  data->service_data = maybe_eid_from_service_data;
  data->uuids = uuids;
  observed_devices_.insert_or_assign(address, std::move(data));

  // New or updated device information.
  if (known) {
    FIDO_LOG(DEBUG) << "Updated information for caBLE device " << address
                    << ":";
  } else {
    FIDO_LOG(DEBUG) << "New caBLE device " << address << ":";
  }

  base::Optional<FidoCableDiscovery::Result> ret;
  if (maybe_eid_from_service_data.has_value()) {
    ret =
        GetCableDiscoveryDataFromAuthenticatorEid(*maybe_eid_from_service_data);
    FIDO_LOG(DEBUG) << "  Service data: "
                    << ResultDebugString(*maybe_eid_from_service_data, ret);

  } else {
    FIDO_LOG(DEBUG) << "  Service data: <none>";
  }

  if (!uuids.empty()) {
    FIDO_LOG(DEBUG) << "  UUIDs:";
    for (const auto& uuid : uuids) {
      auto result = GetCableDiscoveryDataFromAuthenticatorEid(uuid);
      FIDO_LOG(DEBUG) << "    " << ResultDebugString(uuid, result);
      if (!ret.has_value() && result.has_value()) {
        ret = result;
      }
    }
  }

  return ret;
}

// static
base::Optional<CableEidArray> FidoCableDiscovery::MaybeGetEidFromServiceData(
    const BluetoothDevice* device) {
  const auto* service_data =
      device->GetServiceDataForUUID(CableAdvertisementUUID());
  if (!service_data) {
    return base::nullopt;
  }

  // Received service data from authenticator must have a flag that signals that
  // the service data includes Cable EID.
  if (service_data->empty() || !(service_data->at(0) >> 5 & 1u))
    return base::nullopt;

  CableEidArray received_authenticator_eid;
  bool extract_success = fido_parsing_utils::ExtractArray(
      *service_data, 2, &received_authenticator_eid);
  if (!extract_success)
    return base::nullopt;
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

base::Optional<FidoCableDiscovery::Result>
FidoCableDiscovery::GetCableDiscoveryDataFromAuthenticatorEid(
    CableEidArray authenticator_eid) const {
  for (const auto& candidate : discovery_data_) {
    auto maybe_nonce = candidate.Match(authenticator_eid);
    if (maybe_nonce) {
      return Result(candidate, *maybe_nonce, authenticator_eid, base::nullopt);
    }
  }

  if (qr_generator_key_) {
    // Attempt to match |authenticator_eid| as the result of scanning a QR code.
    const int64_t current_tick = CableDiscoveryData::CurrentTimeTick();
    // kNumPreviousTicks is the number of previous ticks that will be accepted
    // as valid. Ticks are currently 256ms so the value of sixteen translates to
    // about four seconds.
    constexpr int kNumPreviousTicks = 16;

    for (int i = 0; i < kNumPreviousTicks; i++) {
      auto qr_secret = CableDiscoveryData::DeriveQRSecret(*qr_generator_key_,
                                                          current_tick - i);
      CableDiscoveryData candidate(qr_secret);
      auto maybe_nonce = candidate.Match(authenticator_eid);
      if (maybe_nonce) {
        return Result(candidate, *maybe_nonce, authenticator_eid, i);
      }
    }

    if (base::Contains(noted_obsolete_eids_, authenticator_eid)) {
      for (int i = kNumPreviousTicks; i < 2 * kNumPreviousTicks; i++) {
        auto qr_secret = CableDiscoveryData::DeriveQRSecret(*qr_generator_key_,
                                                            current_tick - i);
        CableDiscoveryData candidate(qr_secret);
        if (candidate.Match(authenticator_eid)) {
          noted_obsolete_eids_.insert(authenticator_eid);
          FIDO_LOG(DEBUG)
              << "(EID " << base::HexEncode(authenticator_eid) << " is " << i
              << " ticks old and would be valid but for the cutoff)";
          break;
        }
      }
    }
  }

  return base::nullopt;
}

// static
std::string FidoCableDiscovery::ResultDebugString(
    const CableEidArray& eid,
    const base::Optional<FidoCableDiscovery::Result>& result) {
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

  switch (result->discovery_data.version) {
    case CableDiscoveryData::Version::V1:
      ret += " (version one match";
      break;
    case CableDiscoveryData::Version::V2:
      ret += " (version two match";
      break;
    case CableDiscoveryData::Version::INVALID:
      NOTREACHED();
  }

  if (!result->ticks_back) {
    ret += " against pairing data)";
  } else {
    ret += " from QR, " + base::NumberToString(*result->ticks_back) +
           " tick(s) ago)";
  }

  return ret;
}

}  // namespace device
