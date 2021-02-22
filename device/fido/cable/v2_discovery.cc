// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/v2_discovery.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/cable/fido_tunnel_device.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/fido_parsing_utils.h"
#include "third_party/boringssl/src/include/openssl/aes.h"

namespace device {
namespace cablev2 {

namespace {

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
    network::mojom::NetworkContext* network_context,
    base::Optional<base::span<const uint8_t, kQRKeySize>> qr_generator_key,
    std::vector<std::unique_ptr<Pairing>> pairings,
    const std::vector<CableDiscoveryData>& extension_contents,
    base::Optional<base::RepeatingCallback<void(PairingEvent)>>
        pairing_callback)
    : FidoDeviceDiscovery(
          FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy),
      network_context_(network_context),
      qr_keys_(KeysFromQRGeneratorKey(qr_generator_key)),
      extension_keys_(KeysFromExtension(extension_contents)),
      pairings_(std::move(pairings)),
      pairing_callback_(std::move(pairing_callback)) {
  static_assert(EXTENT(*qr_generator_key) == kQRSecretSize + kQRSeedSize, "");
}

Discovery::~Discovery() = default;

void Discovery::StartInternal() {
  DCHECK(!started_);

  RecordEvent(CableV2DiscoveryEvent::kStarted);
  if (!pairings_.empty()) {
    RecordEvent(CableV2DiscoveryEvent::kHavePairings);
  }
  if (qr_keys_) {
    RecordEvent(CableV2DiscoveryEvent::kHaveQRKeys);
  }
  if (extension_keys_) {
    RecordEvent(CableV2DiscoveryEvent::kHaveExtensionKeys);
  }

  for (auto& pairing : pairings_) {
    std::array<uint8_t, kP256X962Length> peer_public_key_x962 =
        pairing->peer_public_key_x962;
    tunnels_pending_advert_.emplace_back(std::make_unique<FidoTunnelDevice>(
        network_context_, std::move(pairing),
        base::BindOnce(&Discovery::PairingIsInvalid, weak_factory_.GetWeakPtr(),
                       peer_public_key_x962)));
  }
  pairings_.clear();

  started_ = true;
  NotifyDiscoveryStarted(true);

  std::vector<std::array<uint8_t, kAdvertSize>> pending_adverts(
      std::move(pending_adverts_));
  for (const auto& advert : pending_adverts) {
    OnBLEAdvertSeen(advert);
  }
}

void Discovery::OnBLEAdvertSeen(
    const std::array<uint8_t, kAdvertSize>& advert) {
  if (!started_) {
    pending_adverts_.push_back(advert);
    return;
  }

  if (base::Contains(observed_adverts_, advert)) {
    return;
  }
  observed_adverts_.insert(advert);

  // Check whether the EID satisfies any pending tunnels.
  for (std::vector<std::unique_ptr<FidoTunnelDevice>>::iterator i =
           tunnels_pending_advert_.begin();
       i != tunnels_pending_advert_.end(); i++) {
    if (!(*i)->MatchAdvert(advert)) {
      continue;
    }

    RecordEvent(CableV2DiscoveryEvent::kTunnelMatch);
    FIDO_LOG(DEBUG) << "  (" << base::HexEncode(advert)
                    << " matches pending tunnel)";
    std::unique_ptr<FidoTunnelDevice> device(std::move(*i));
    tunnels_pending_advert_.erase(i);
    AddDevice(std::move(device));
    return;
  }

  if (qr_keys_) {
    // Check whether the EID matches a QR code.
    base::Optional<CableEidArray> plaintext =
        eid::Decrypt(advert, qr_keys_->eid_key);
    if (plaintext) {
      FIDO_LOG(DEBUG) << "  (" << base::HexEncode(advert)
                      << " matches QR code)";
      RecordEvent(CableV2DiscoveryEvent::kQRMatch);
      AddDevice(std::make_unique<cablev2::FidoTunnelDevice>(
          network_context_,
          base::BindOnce(&Discovery::AddPairing, weak_factory_.GetWeakPtr()),
          qr_keys_->qr_secret, qr_keys_->local_identity_seed, *plaintext));
      return;
    }
  }

  // Check whether the EID matches the extension.
  if (extension_keys_) {
    base::Optional<CableEidArray> plaintext =
        eid::Decrypt(advert, extension_keys_->eid_key);
    if (plaintext) {
      FIDO_LOG(DEBUG) << "  (" << base::HexEncode(advert)
                      << " matches extension)";
      RecordEvent(CableV2DiscoveryEvent::kExtensionMatch);
      AddDevice(std::make_unique<cablev2::FidoTunnelDevice>(
          network_context_, base::DoNothing(), extension_keys_->qr_secret,
          extension_keys_->local_identity_seed, *plaintext));
      return;
    }
  }

  RecordEvent(CableV2DiscoveryEvent::kNoMatch);
  FIDO_LOG(DEBUG) << "  (" << base::HexEncode(advert) << ": no v2 match)";
}

void Discovery::AddPairing(std::unique_ptr<Pairing> pairing) {
  if (!pairing_callback_) {
    return;
  }

  pairing_callback_->Run(std::move(pairing));
}

void Discovery::PairingIsInvalid(
    std::array<uint8_t, kP256X962Length> peer_public_key_x962) {
  if (!pairing_callback_) {
    return;
  }

  pairing_callback_->Run(std::move(peer_public_key_x962));
}

// static
base::Optional<Discovery::UnpairedKeys> Discovery::KeysFromQRGeneratorKey(
    const base::Optional<base::span<const uint8_t, kQRKeySize>>
        qr_generator_key) {
  if (!qr_generator_key) {
    return base::nullopt;
  }

  UnpairedKeys ret;
  static_assert(EXTENT(*qr_generator_key) == kQRSeedSize + kQRSecretSize, "");
  ret.local_identity_seed = fido_parsing_utils::Materialize(
      qr_generator_key->subspan<0, kQRSeedSize>());
  ret.qr_secret = fido_parsing_utils::Materialize(
      qr_generator_key->subspan<kQRSeedSize, kQRSecretSize>());
  ret.eid_key = Derive<EXTENT(ret.eid_key)>(
      ret.qr_secret, base::span<const uint8_t>(), DerivedValueType::kEIDKey);
  return ret;
}

// static
base::Optional<Discovery::UnpairedKeys> Discovery::KeysFromExtension(
    const std::vector<CableDiscoveryData>& extension_contents) {
  for (auto const& data : extension_contents) {
    if (data.version != CableDiscoveryData::Version::V2) {
      continue;
    }

    if (data.v2->size() != kQRKeySize) {
      FIDO_LOG(ERROR) << "caBLEv2 extension has incorrect length ("
                      << data.v2->size() << ")";
      continue;
    }

    return KeysFromQRGeneratorKey(base::make_span<kQRKeySize>(*data.v2));
  }

  return base::nullopt;
}

}  // namespace cablev2
}  // namespace device
