// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/virtual_authenticator.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/uuid.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_u2f_device.h"
#include "mojo/public/cpp/base/big_buffer.h"

namespace content {

VirtualAuthenticator::VirtualAuthenticator(
    const blink::test::mojom::VirtualAuthenticatorOptions& options)
    : protocol_(options.protocol),
      ctap2_version_(options.ctap2_version),
      attachment_(options.attachment),
      has_resident_key_(options.has_resident_key),
      has_user_verification_(options.has_user_verification),
      has_large_blob_(options.has_large_blob),
      has_cred_blob_(options.has_cred_blob),
      has_min_pin_length_(options.has_min_pin_length),
      has_prf_(options.has_prf),
      unique_id_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      state_(base::MakeRefCounted<device::VirtualFidoDevice::State>()) {
  state_->transport = options.transport;
  // If the authenticator has user verification, simulate having set it up
  // already.
  state_->fingerprints_enrolled = has_user_verification_;
  state_->default_backup_eligibility = options.default_backup_eligibility;
  state_->default_backup_state = options.default_backup_state;
  observation_.Observe(state_.get());
  SetUserPresence(true);
}

VirtualAuthenticator::~VirtualAuthenticator() {
  for (Observer& observer : observers_) {
    observer.OnAuthenticatorWillBeDestroyed(this);
  }
}

void VirtualAuthenticator::AddReceiver(
    mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticator> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

bool VirtualAuthenticator::AddRegistration(
    std::vector<uint8_t> key_handle,
    const std::string& rp_id,
    base::span<const uint8_t> private_key,
    int32_t counter) {
  std::optional<std::unique_ptr<device::VirtualFidoDevice::PrivateKey>>
      fido_private_key =
          device::VirtualFidoDevice::PrivateKey::FromPKCS8(private_key);
  if (!fido_private_key) {
    return false;
  }

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
    base::span<const uint8_t> private_key,
    int32_t counter,
    std::vector<uint8_t> user_handle,
    std::optional<std::string> user_name,
    std::optional<std::string> user_display_name) {
  std::optional<std::unique_ptr<device::VirtualFidoDevice::PrivateKey>>
      fido_private_key =
          device::VirtualFidoDevice::PrivateKey::FromPKCS8(private_key);
  if (!fido_private_key) {
    return false;
  }

  return state_->InjectResidentKey(
      std::move(key_handle),
      device::PublicKeyCredentialRpEntity(std::move(rp_id)),
      device::PublicKeyCredentialUserEntity(std::move(user_handle),
                                            std::move(user_name),
                                            std::move(user_display_name)),
      counter, std::move(*fido_private_key));
}

void VirtualAuthenticator::ClearRegistrations() {
  device::VirtualFidoDevice::State::RegistrationsMap erased;
  state_->registrations.swap(erased);
  for (const auto& registration : erased) {
    state_->NotifyCredentialDeleted(registration.first);
  }
}

bool VirtualAuthenticator::RemoveRegistration(
    const std::vector<uint8_t>& key_handle) {
  bool removed = state_->registrations.erase(key_handle) != 0;
  if (removed) {
    state_->NotifyCredentialDeleted(key_handle);
  }
  return removed;
}

void VirtualAuthenticator::UpdateUserDetails(std::string_view relying_party_id,
                                             base::span<const uint8_t> user_id,
                                             std::string_view name,
                                             std::string_view display_name) {
  for (auto& registration : state_->registrations) {
    if (registration.second.user && registration.second.rp &&
        registration.second.rp->id == relying_party_id &&
        registration.second.user->id == user_id) {
      registration.second.user->name = name;
      registration.second.user->display_name = display_name;
      state_->NotifyCredentialUpdated(
          std::make_pair(registration.first, &registration.second));
    }
  }
}

void VirtualAuthenticator::SetUserPresence(bool is_user_present) {
  is_user_present_ = is_user_present;
  state_->simulate_press_callback = base::BindRepeating(
      [](bool is_user_present, device::VirtualFidoDevice* device) {
        return is_user_present;
      },
      is_user_present);
}

std::unique_ptr<device::VirtualFidoDevice>
VirtualAuthenticator::ConstructDevice() {
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
      config.large_blob_support = has_large_blob_;
      config.cred_protect_support = config.cred_blob_support = has_cred_blob_;
      config.min_pin_length_extension_support = has_min_pin_length_;
      if (has_prf_) {
        config.prf_support = true;
        // This is required when `prf_support` is set.
        config.internal_account_chooser = true;
      }

      if (
          // Writing a large blob requires obtaining a PinUvAuthToken with
          // permissions if the authenticator is protected by user verification.
          (has_large_blob_ && has_user_verification_) ||
          // PRF support always requires PIN support because the exchange is
          // encrypted.
          has_prf_) {
        config.pin_uv_auth_token_support = true;
      }
      config.internal_uv_support = has_user_verification_;
      config.is_platform_authenticator =
          attachment_ == device::AuthenticatorAttachment::kPlatform;
      config.user_verification_succeeds = is_user_verified_;
      config.advertised_algorithms = {
          device::CoseAlgorithmIdentifier::kEdDSA,
          device::CoseAlgorithmIdentifier::kEs256,
          device::CoseAlgorithmIdentifier::kRs256,
      };
      return std::make_unique<device::VirtualCtap2Device>(state_, config);
    }
    default:
      NOTREACHED_IN_MIGRATION();
      return std::make_unique<device::VirtualU2fDevice>(state_);
  }
}

void VirtualAuthenticator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void VirtualAuthenticator::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool VirtualAuthenticator::HasObserversForTest() {
  return !observers_.empty();
}

void VirtualAuthenticator::SetBackupEligibility(
    const std::vector<uint8_t>& key_handle,
    bool backup_eligibility) {
  state_->registrations.at(key_handle).backup_eligible = backup_eligibility;
}

void VirtualAuthenticator::SetBackupState(
    const std::vector<uint8_t>& key_handle,
    bool backup_state) {
  state_->registrations.at(key_handle).backup_state = backup_state;
}

void VirtualAuthenticator::GetLargeBlob(const std::vector<uint8_t>& key_handle,
                                        GetLargeBlobCallback callback) {
  auto registration = state_->registrations.find(key_handle);
  if (registration == state_->registrations.end()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::optional<device::LargeBlob> blob =
      state_->GetLargeBlob(registration->second);
  if (!blob) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  data_decoder_.Inflate(
      std::move(blob->compressed_data), blob->original_size,
      base::BindOnce(&VirtualAuthenticator::OnLargeBlobUncompressed,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void VirtualAuthenticator::SetLargeBlob(const std::vector<uint8_t>& key_handle,
                                        const std::vector<uint8_t>& blob,
                                        SetLargeBlobCallback callback) {
  data_decoder_.Deflate(
      blob, base::BindOnce(&VirtualAuthenticator::OnLargeBlobCompressed,
                           weak_factory_.GetWeakPtr(), key_handle, blob.size(),
                           std::move(callback)));
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

void VirtualAuthenticator::OnCredentialCreated(
    const device::VirtualFidoDevice::Credential& credential) {
  for (Observer& observer : observers_) {
    observer.OnCredentialCreated(this, credential);
  }
}

void VirtualAuthenticator::OnCredentialDeleted(
    base::span<const uint8_t> credential_id) {
  for (Observer& observer : observers_) {
    observer.OnCredentialDeleted(this, credential_id);
  }
}

void VirtualAuthenticator::OnCredentialUpdated(
    const device::VirtualFidoDevice::Credential& credential) {
  for (Observer& observer : observers_) {
    observer.OnCredentialUpdated(this, credential);
  }
}

void VirtualAuthenticator::OnAssertion(
    const device::VirtualFidoDevice::Credential& credential) {
  for (Observer& observer : observers_) {
    observer.OnAssertion(this, credential);
  }
}

void VirtualAuthenticator::OnLargeBlobUncompressed(
    GetLargeBlobCallback callback,
    base::expected<mojo_base::BigBuffer, std::string> result) {
  std::optional<mojo_base::BigBuffer> value;
  if (result.has_value())
    value = std::move(*result);

  std::move(callback).Run(device::fido_parsing_utils::MaterializeOrNull(value));
}

void VirtualAuthenticator::OnLargeBlobCompressed(
    base::span<const uint8_t> key_handle,
    uint64_t original_size,
    SetLargeBlobCallback callback,
    base::expected<mojo_base::BigBuffer, std::string> result) {
  auto registration = state_->registrations.find(key_handle);
  if (registration == state_->registrations.end()) {
    std::move(callback).Run(false);
    return;
  }
  if (result.has_value()) {
    state_->InjectLargeBlob(
        &registration->second,
        device::LargeBlob(device::fido_parsing_utils::Materialize(*result),
                          original_size));
  }
  std::move(callback).Run(result.has_value());
}

}  // namespace content
