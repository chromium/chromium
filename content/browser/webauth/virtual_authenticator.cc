// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/virtual_authenticator.h"

#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "crypto/ec_private_key.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_u2f_device.h"

namespace content {

VirtualAuthenticator::VirtualAuthenticator(
    device::ProtocolVersion protocol,
    device::Ctap2Version ctap2_version,
    device::FidoTransportProtocol transport,
    device::AuthenticatorAttachment attachment,
    bool has_resident_key,
    bool has_user_verification)
    : protocol_(protocol),
      ctap2_version_(ctap2_version),
      attachment_(attachment),
      has_resident_key_(has_resident_key),
      has_user_verification_(has_user_verification),
      unique_id_(base::GenerateGUID()),
      state_(base::MakeRefCounted<device::VirtualFidoDevice::State>()) {
  state_->transport = transport;
  // If the authenticator has user verification, simulate having set it up
  // already.
  state_->fingerprints_enrolled = has_user_verification_;
  SetUserPresence(true);
}

VirtualAuthenticator::~VirtualAuthenticator() = default;

void VirtualAuthenticator::AddReceiver(
    mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticator> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

bool VirtualAuthenticator::AddRegistration(
    std::vector<uint8_t> key_handle,
    const std::string& rp_id,
    const std::vector<uint8_t>& private_key,
    int32_t counter) {
  base::Optional<std::unique_ptr<device::VirtualFidoDevice::PrivateKey>>
      fido_private_key =
          device::VirtualFidoDevice::PrivateKey::FromPKCS8(private_key);
  if (!fido_private_key)
    return false;

  return state_->registrations
      .emplace(
          std::move(key_handle),
          device::VirtualFidoDevice::RegistrationData(
              std::move(*fido_private_key),
              device::fido_parsing_utils::CreateSHA256Hash(rp_id), counter))
      .second;
}

bool VirtualAuthenticator::AddResidentRegistration(
    std::vector<uint8_t> key_handle,
    std::string rp_id,
    const std::vector<uint8_t>& private_key,
    int32_t counter,
    std::vector<uint8_t> user_handle) {
  base::Optional<std::unique_ptr<device::VirtualFidoDevice::PrivateKey>>
      fido_private_key =
          device::VirtualFidoDevice::PrivateKey::FromPKCS8(private_key);
  if (!fido_private_key)
    return false;

  return state_->InjectResidentKey(
      std::move(key_handle),
      device::PublicKeyCredentialRpEntity(std::move(rp_id)),
      device::PublicKeyCredentialUserEntity(std::move(user_handle)), counter,
      std::move(*fido_private_key));
}

void VirtualAuthenticator::ClearRegistrations() {
  state_->registrations.clear();
}

bool VirtualAuthenticator::RemoveRegistration(
    const std::vector<uint8_t>& key_handle) {
  return state_->registrations.erase(key_handle) != 0;
}

void VirtualAuthenticator::SetUserPresence(bool is_user_present) {
  is_user_present_ = is_user_present;
  state_->simulate_press_callback = base::BindRepeating(
      [](bool is_user_present, device::VirtualFidoDevice* device) {
        return is_user_present;
      },
      is_user_present);
}

std::unique_ptr<device::FidoDevice> VirtualAuthenticator::ConstructDevice() {
  switch (protocol_) {
    case device::ProtocolVersion::kU2f:
      return std::make_unique<device::VirtualU2fDevice>(state_);
    case device::ProtocolVersion::kCtap2: {
      device::VirtualCtap2Device::Config config;
      switch (ctap2_version_) {
        case device::Ctap2Version::kCtap2_0:
          config.ctap2_versions = {std::begin(device::kCtap2Versions2_0),
                                   std::end(device::kCtap2Versions2_0)};
          break;
        case device::Ctap2Version::kCtap2_1:
          config.ctap2_versions = {std::begin(device::kCtap2Versions2_1),
                                   std::end(device::kCtap2Versions2_1)};
          break;
      }
      config.resident_key_support = has_resident_key_;
      config.internal_uv_support = has_user_verification_;
      config.is_platform_authenticator =
          attachment_ == device::AuthenticatorAttachment::kPlatform;
      config.user_verification_succeeds = is_user_verified_;
      return std::make_unique<device::VirtualCtap2Device>(state_, config);
    }
    default:
      NOTREACHED();
      return std::make_unique<device::VirtualU2fDevice>(state_);
  }
}

void VirtualAuthenticator::GetUniqueId(GetUniqueIdCallback callback) {
  std::move(callback).Run(unique_id_);
}

void VirtualAuthenticator::GetRegistrations(GetRegistrationsCallback callback) {
  std::vector<blink::test::mojom::RegisteredKeyPtr> mojo_registered_keys;
  for (const auto& registration : state_->registrations) {
    auto mojo_registered_key = blink::test::mojom::RegisteredKey::New();
    mojo_registered_key->key_handle = registration.first;
    mojo_registered_key->counter = registration.second.counter;
    mojo_registered_key->rp_id =
        registration.second.rp ? registration.second.rp->id : "";
    mojo_registered_key->private_key =
        registration.second.private_key->GetPKCS8PrivateKey();
    mojo_registered_keys.push_back(std::move(mojo_registered_key));
  }
  std::move(callback).Run(std::move(mojo_registered_keys));
}

void VirtualAuthenticator::AddRegistration(
    blink::test::mojom::RegisteredKeyPtr registration,
    AddRegistrationCallback callback) {
  std::move(callback).Run(AddRegistration(
      std::move(registration->key_handle), std::move(registration->rp_id),
      registration->private_key, registration->counter));
}

void VirtualAuthenticator::ClearRegistrations(
    ClearRegistrationsCallback callback) {
  ClearRegistrations();
  std::move(callback).Run();
}

void VirtualAuthenticator::RemoveRegistration(
    const std::vector<uint8_t>& key_handle,
    RemoveRegistrationCallback callback) {
  std::move(callback).Run(RemoveRegistration(std::move(key_handle)));
}

void VirtualAuthenticator::SetUserVerified(bool verified,
                                           SetUserVerifiedCallback callback) {
  is_user_verified_ = verified;
  std::move(callback).Run();
}

}  // namespace content
