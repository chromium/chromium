// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_authenticator.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/notreached.h"
#include "device/fido/cable/fido_tunnel_device.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"

namespace device {

void FidoAuthenticator::ExcludeAppIdCredentialsBeforeMakeCredential(
    CtapMakeCredentialRequest request,
    MakeCredentialOptions options,
    base::OnceCallback<void(CtapDeviceResponseCode, std::optional<bool>)>
        callback) {
  std::move(callback).Run(CtapDeviceResponseCode::kSuccess, std::nullopt);
}

void FidoAuthenticator::GetPlatformCredentialInfoForRequest(
    const CtapGetAssertionRequest& request,
    const CtapGetAssertionOptions& options,
    GetPlatformCredentialInfoForRequestCallback callback) {
  NOTREACHED();
}

void FidoAuthenticator::GetTouch(base::OnceCallback<void()> callback) {}

void FidoAuthenticator::GetPinRetries(
    FidoAuthenticator::GetRetriesCallback callback) {
  NOTREACHED();
}

void FidoAuthenticator::GetPINToken(
    std::string pin,
    std::vector<pin::Permissions> permissions,
    std::optional<std::string> rp_id,
    FidoAuthenticator::GetTokenCallback callback) {
  NOTREACHED();
}

void FidoAuthenticator::GetUvRetries(
    FidoAuthenticator::GetRetriesCallback callback) {
  NOTREACHED();
}

bool FidoAuthenticator::CanGetUvToken() {
  return false;
}

void FidoAuthenticator::GetUvToken(
    std::vector<pin::Permissions> permissions,
    std::optional<std::string> rp_id,
    FidoAuthenticator::GetTokenCallback callback) {
  NOTREACHED();
}

uint32_t FidoAuthenticator::CurrentMinPINLength() {
  NOTREACHED();
}

uint32_t FidoAuthenticator::NewMinPINLength() {
  NOTREACHED();
}

bool FidoAuthenticator::ForcePINChange() {
  NOTREACHED();
}

void FidoAuthenticator::SetPIN(const std::string& pin,
                               FidoAuthenticator::SetPINCallback callback) {
  NOTREACHED();
}

void FidoAuthenticator::ChangePIN(const std::string& old_pin,
                                  const std::string& new_pin,
                                  SetPINCallback callback) {
  NOTREACHED();
}

FidoAuthenticator::PINUVDisposition
FidoAuthenticator::PINUVDispositionForMakeCredential(
    const CtapMakeCredentialRequest& request,
    const FidoRequestHandlerBase::Observer* observer) {
  return PINUVDisposition::kUVNotSupportedNorRequired;
}

FidoAuthenticator::PINUVDisposition
FidoAuthenticator::PINUVDispositionForGetAssertion(
    const CtapGetAssertionRequest& request,
    const FidoRequestHandlerBase::Observer* observer) {
  return PINUVDisposition::kUVNotSupportedNorRequired;
}

void FidoAuthenticator::GetCredentialsMetadata(
    const pin::TokenResponse& pin_token,
    GetCredentialsMetadataCallback callback) {
  NOTREACHED();
}

void FidoAuthenticator::EnumerateCredentials(
    const pin::TokenResponse& pin_token,
    EnumerateCredentialsCallback callback) {
  NOTREACHED();
}

void FidoAuthenticator::DeleteCredential(
    const pin::TokenResponse& pin_token,
    const PublicKeyCredentialDescriptor& credential_id,
    DeleteCredentialCallback callback) {
  NOTREACHED();
}

bool FidoAuthenticator::SupportsUpdateUserInformation() const {
  return false;
}

void FidoAuthenticator::UpdateUserInformation(
    const pin::TokenResponse& pin_token,
    const PublicKeyCredentialDescriptor& credential_id,
    const PublicKeyCredentialUserEntity& updated_user,
    UpdateUserInformationCallback callback) {
  NOTREACHED();
}

void FidoAuthenticator::GetModality(BioEnrollmentCallback) {
  NOTREACHED();
}

void FidoAuthenticator::GetSensorInfo(BioEnrollmentCallback) {
  NOTREACHED();
}

void FidoAuthenticator::BioEnrollFingerprint(
    const pin::TokenResponse&,
    std::optional<std::vector<uint8_t>> template_id,
    BioEnrollmentCallback) {
  NOTREACHED();
}

void FidoAuthenticator::BioEnrollCancel(BioEnrollmentCallback) {
  NOTREACHED();
}

void FidoAuthenticator::BioEnrollEnumerate(const pin::TokenResponse&,
                                           BioEnrollmentCallback) {
  NOTREACHED();
}

void FidoAuthenticator::BioEnrollRename(const pin::TokenResponse&,
                                        std::vector<uint8_t>,
                                        std::string,
                                        BioEnrollmentCallback) {
  NOTREACHED();
}

void FidoAuthenticator::BioEnrollDelete(const pin::TokenResponse&,
                                        std::vector<uint8_t>,
                                        BioEnrollmentCallback) {
  NOTREACHED();
}

void FidoAuthenticator::GarbageCollectLargeBlob(
    const pin::TokenResponse& pin_uv_auth_token,
    base::OnceCallback<void(CtapDeviceResponseCode)> callback) {
  NOTREACHED();
}

std::optional<base::span<const int32_t>> FidoAuthenticator::GetAlgorithms() {
  return std::nullopt;
}

bool FidoAuthenticator::DiscoverableCredentialStorageFull() const {
  return false;
}

void FidoAuthenticator::Reset(ResetCallback callback) {
  std::move(callback).Run(CtapDeviceResponseCode::kCtap1ErrInvalidCommand,
                          std::nullopt);
}

AuthenticatorType FidoAuthenticator::GetType() const {
  return AuthenticatorType::kOther;
}

cablev2::FidoTunnelDevice* FidoAuthenticator::GetTunnelDevice() {
  return nullptr;
}

std::string FidoAuthenticator::GetDisplayName() const {
  return GetId();
}

ProtocolVersion FidoAuthenticator::SupportedProtocol() const {
  return ProtocolVersion::kUnknown;
}

}  // namespace device
