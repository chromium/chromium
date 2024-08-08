// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/v2_discovery.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/fido_tunnel_device.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/features.h"
#include "device/fido/fido_parsing_utils.h"
#include "third_party/boringssl/src/include/openssl/aes.h"

namespace device::cablev2 {

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
    FidoRequestType request_type,
    NetworkContextFactory network_context_factory,
    std::optional<base::span<const uint8_t, kQRKeySize>> qr_generator_key,
    std::unique_ptr<AdvertEventStream> advert_stream,
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
      advert_stream_(std::move(advert_stream)),
      contact_device_stream_(std::move(contact_device_stream)),
      pairing_callback_(std::move(pairing_callback)),
      invalidated_pairing_callback_(std::move(invalidated_pairing_callback)),
      event_callback_(std::move(event_callback)),
      must_support_ctap_(must_support_ctap) {
  static_assert(EXTENT(*qr_generator_key) == kQRSecretSize + kQRSeedSize, "");
  advert_stream_->Connect(
      base::BindRepeating(&Discovery::OnBLEAdvertSeen, base::Unretained(this)));

  if (contact_device_stream_) {
    contact_device_stream_->Connect(base::BindRepeating(
        &Discovery::OnContactDevice, base::Unretained(this)));
  }
}

Discovery::~Discovery() = default;

void Discovery::StartInternal() {
  DCHECK(!started_);

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
  NotifyDiscoveryStarted(true);

  std::vector<std::array<uint8_t, kAdvertSize>> pending_adverts(
      std::move(pending_adverts_));
  for (const auto& advert : pending_adverts) {
    OnBLEAdvertSeen(advert);
  }
}

void Discovery::OnBLEAdvertSeen(base::span<const uint8_t, kAdvertSize> advert) {
  const std::array<uint8_t, kAdvertSize> advert_array =
      fido_parsing_utils::Materialize<kAdvertSize>(advert);

  if (!started_) {
    // Server-linked devices may have started advertising already.
    pending_adverts_.push_back(advert_array);
    return;
  }

  if (device_committed_) {
    // A device has already been accepted. Ignore other adverts.
    return;
  }

  if (base::Contains(observed_adverts_, advert_array)) {
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
      AddDevice(std::make_unique<cablev2::FidoTunnelDevice>(
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
    const std::optional<base::span<const uint8_t, kQRKeySize>>
        qr_generator_key) {
  if (!qr_generator_key) {
    return std::nullopt;
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

    std::optional<Discovery::UnpairedKeys> keys =
        KeysFromQRGeneratorKey(sized_server_link_data_span.value());
    if (keys.has_value()) {
      ret.emplace_back(std::move(keys.value()));
    }
  }

  return ret;
}

}  // namespace device::cablev2
