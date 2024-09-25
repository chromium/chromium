// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/win/fake_webauthn_api.h"

#include <stdint.h>
#include <winerror.h>

#include <memory>
#include <optional>
#include <string>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/notreached.h"
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "crypto/sha2.h"
#include "device/fido/attestation_statement.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/public_key.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/virtual_fido_device.h"
#include "third_party/microsoft_webauthn/webauthn.h"

namespace device {

namespace {

constexpr std::array<uint8_t, kAaguidLength> kTestWindowsAaguid = {
    {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
     0x0d, 0x0e, 0x0f, 0x10}};

std::unique_ptr<VirtualFidoDevice::PrivateKey> MakePrivateKey(
    PCWEBAUTHN_COSE_CREDENTIAL_PARAMETERS cose_credential_parameters,
    bool is_platform_credential) {
  for (size_t i = 0; i < cose_credential_parameters->cCredentialParameters;
       ++i) {
    WEBAUTHN_COSE_CREDENTIAL_PARAMETER parameter =
        cose_credential_parameters->pCredentialParameters[i];
    if (is_platform_credential) {
      // Windows only supports RS256 for platform credentials.
      if (parameter.lAlg ==
          static_cast<LONG>(CoseAlgorithmIdentifier::kRs256)) {
        return VirtualFidoDevice::PrivateKey::FreshP256Key();
      }
      continue;
    }

    switch (parameter.lAlg) {
      case static_cast<LONG>(CoseAlgorithmIdentifier::kEs256):
        return VirtualFidoDevice::PrivateKey::FreshP256Key();
      case static_cast<LONG>(CoseAlgorithmIdentifier::kRs256):
        return VirtualFidoDevice::PrivateKey::FreshRSAKey();
      case static_cast<LONG>(CoseAlgorithmIdentifier::kEdDSA):
        return VirtualFidoDevice::PrivateKey::FreshEd25519Key();
    }
  }
  return nullptr;
}

}  // namespace

struct FakeWinWebAuthnApi::CredentialInfoList {
  WEBAUTHN_CREDENTIAL_DETAILS_LIST credential_details_list;
  // This field is not vector<raw_ptr<...>> due to interaction with third_party
  // api.
  RAW_PTR_EXCLUSION std::vector<WEBAUTHN_CREDENTIAL_DETAILS*> win_credentials;
  std::vector<std::unique_ptr<CredentialInfo>> credentials;
};

struct FakeWinWebAuthnApi::CredentialInfo {
  // This structure contains pointers to itself and thus
  // must not be moved in memory.
  CredentialInfo() = default;
  CredentialInfo(const CredentialInfo&) = delete;
  CredentialInfo(CredentialInfo&&) = delete;
  CredentialInfo& operator=(CredentialInfo&&) = delete;
  CredentialInfo& operator=(const CredentialInfo&) = delete;

  WEBAUTHN_CREDENTIAL_DETAILS details;
  std::vector<uint8_t> credential_id;

  WEBAUTHN_RP_ENTITY_INFORMATION rp;
  std::u16string rp_id;
  std::u16string rp_name;

  WEBAUTHN_USER_ENTITY_INFORMATION user;
  std::vector<uint8_t> user_id;
  std::u16string user_name;
  std::u16string user_display_name;
};

struct FakeWinWebAuthnApi::WebAuthnAttestation {
  WebAuthnAttestation() = default;
  // This structure contains pointers to itself and thus
  // must not be moved in memory.
  WebAuthnAttestation(const WebAuthnAttestation&) = delete;
  WebAuthnAttestation(WebAuthnAttestation&&) = delete;
  WebAuthnAttestation& operator=(WebAuthnAttestation&&) = delete;
  WebAuthnAttestation& operator=(const WebAuthnAttestation&&) = delete;

  std::vector<uint8_t> authenticator_data;
  std::vector<uint8_t> attestation;
  std::vector<uint8_t> attestation_object;
  std::vector<uint8_t> credential_id;

  WEBAUTHN_CREDENTIAL_ATTESTATION win_attestation;
};

struct FakeWinWebAuthnApi::WebAuthnAssertionEx {
  // This structure contains pointers to itself and thus
  // must not be moved in memory.
  WebAuthnAssertionEx() = default;
  WebAuthnAssertionEx(const WebAuthnAssertionEx&) = delete;
  WebAuthnAssertionEx(WebAuthnAssertionEx&&) = delete;
  WebAuthnAssertionEx& operator=(WebAuthnAssertionEx&&) = delete;
  WebAuthnAssertionEx& operator=(const WebAuthnAssertionEx&) = delete;

  std::vector<uint8_t> credential_id;
  std::optional<std::vector<uint8_t>> user_id;
  std::vector<uint8_t> authenticator_data;
  std::vector<uint8_t> signature;
  std::optional<std::vector<uint8_t>> large_blob;
  WEBAUTHN_ASSERTION assertion;
};

FakeWinWebAuthnApi::FakeWinWebAuthnApi() = default;
FakeWinWebAuthnApi::~FakeWinWebAuthnApi() {
  // Ensure callers free unmanaged pointers returned by the real Windows API.
  DCHECK(returned_attestations_.empty());
  DCHECK(returned_assertions_.empty());
  DCHECK(returned_credential_lists_.empty());
}

bool FakeWinWebAuthnApi::InjectNonDiscoverableCredential(
    base::span<const uint8_t> credential_id,
    const std::string& rp_id) {
  bool was_inserted;
  std::tie(std::ignore, was_inserted) = registrations_.insert(
      {fido_parsing_utils::Materialize(credential_id),
       RegistrationData(VirtualFidoDevice::PrivateKey::FreshP256Key(),
                        fido_parsing_utils::CreateSHA256Hash(rp_id),
                        /*counter=*/0)});
  return was_inserted;
}

bool FakeWinWebAuthnApi::InjectDiscoverableCredential(
    base::span<const uint8_t> credential_id,
    device::PublicKeyCredentialRpEntity rp,
    device::PublicKeyCredentialUserEntity user) {
  RegistrationData registration(VirtualFidoDevice::PrivateKey::FreshP256Key(),
                                fido_parsing_utils::CreateSHA256Hash(rp.id),
                                /*counter=*/0);
  registration.is_resident = true;
  registration.user = std::move(user);
  registration.rp = std::move(rp);

  bool was_inserted;
  std::tie(std::ignore, was_inserted) =
      registrations_.insert({fido_parsing_utils::Materialize(credential_id),
                             std::move(registration)});
  return was_inserted;
}

bool FakeWinWebAuthnApi::IsAvailable() const {
  return is_available_;
}

bool FakeWinWebAuthnApi::SupportsSilentDiscovery() const {
  return supports_silent_discovery_;
}

HRESULT FakeWinWebAuthnApi::IsUserVerifyingPlatformAuthenticatorAvailable(
    BOOL* result) {
  DCHECK(is_available_);
  *result = is_uvpaa_;
  return S_OK;
}

HRESULT FakeWinWebAuthnApi::AuthenticatorMakeCredential(
    HWND h_wnd,
    PCWEBAUTHN_RP_ENTITY_INFORMATION rp,
    PCWEBAUTHN_USER_ENTITY_INFORMATION user,
    PCWEBAUTHN_COSE_CREDENTIAL_PARAMETERS cose_credential_parameters,
    PCWEBAUTHN_CLIENT_DATA client_data,
    PCWEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS options,
    PWEBAUTHN_CREDENTIAL_ATTESTATION* credential_attestation_ptr) {
  DCHECK(is_available_);
  last_make_credential_options_ =
      std::make_unique<WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS>(
          *options);
  if (result_override_ != S_OK) {
    return result_override_;
  }
  // Validate the input parameters.
  DCHECK_GT(client_data->cbClientDataJSON, 0u);
  DCHECK(client_data->pbClientDataJSON);
  DCHECK(rp->pwszId);
  DCHECK_GT(wcslen(rp->pwszId), 0u);
  DCHECK(rp->pwszName);
  DCHECK_GT(user->cbId, 0u);
  DCHECK(user->pbId);
  DCHECK(user->pwszName);
  DCHECK_GT(wcslen(user->pwszName), 0u);
  DCHECK(user->pwszDisplayName);
  DCHECK(options->pExcludeCredentialList);

  int attachment = options->dwAuthenticatorAttachment;
  if (attachment == WEBAUTHN_AUTHENTICATOR_ATTACHMENT_ANY) {
    attachment = preferred_attachment_;
  }

  std::unique_ptr<VirtualFidoDevice::PrivateKey> private_key =
      MakePrivateKey(cose_credential_parameters,
                     attachment == WEBAUTHN_AUTHENTICATOR_ATTACHMENT_PLATFORM);
  if (!private_key) {
    return NTE_NOT_SUPPORTED;
  }

  for (size_t i = 0; i < options->pExcludeCredentialList->cCredentials; ++i) {
    PWEBAUTHN_CREDENTIAL_EX exclude_credential =
        options->pExcludeCredentialList->ppCredentials[i];
    std::vector<uint8_t> credential_id = fido_parsing_utils::Materialize(
        base::make_span(exclude_credential->pbId, exclude_credential->cbId));
    if (registrations_.contains(credential_id)) {
      return NTE_EXISTS;
    }
  }

  std::unique_ptr<PublicKey> public_key = private_key->GetPublicKey();
  std::vector<uint8_t> credential_id = fido_parsing_utils::Materialize(
      crypto::SHA256Hash(public_key->cose_key_bytes));
  std::string rp_id = base::WideToUTF8(rp->pwszId);
  std::array<uint8_t, crypto::kSHA256Length> rp_id_hash =
      fido_parsing_utils::CreateSHA256Hash(rp_id);
  std::vector<uint8_t> user_id =
      fido_parsing_utils::Materialize(base::make_span(user->pbId, user->cbId));

  RegistrationData registration(std::move(private_key), std::move(rp_id_hash),
                                /*counter=*/1);
  bool resident_key =
      options->bRequireResidentKey || options->bPreferResidentKey;
  if (resident_key) {
    registration.rp =
        PublicKeyCredentialRpEntity(rp_id, base::WideToUTF8(rp->pwszName));
    registration.user =
        PublicKeyCredentialUserEntity(user_id, base::WideToUTF8(user->pwszName),
                                      base::WideToUTF8(user->pwszDisplayName));
  }

  std::array<uint8_t, 2> credential_id_length = {0, crypto::kSHA256Length};
  AttestedCredentialData credential_data(
      kTestWindowsAaguid, credential_id_length, credential_id,
      registration.private_key->GetPublicKey());
  auto attestation = std::make_unique<WebAuthnAttestation>();
  attestation->authenticator_data =
      AuthenticatorData(registration.application_parameter,
                        /*user_present=*/true,
                        options->dwUserVerificationRequirement !=
                            WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED,
                        /*backup_eligible=*/false, /*backup_state=*/false,
                        registration.counter, std::move(credential_data),
                        /*extensions=*/std::nullopt)
          .SerializeToByteArray();
  attestation->credential_id = credential_id;
  // For now, only support none attestation.
  attestation->attestation =
      *cbor::Writer::Write(NoneAttestationStatement().AsCBOR());

  attestation->win_attestation.dwVersion =
      WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_4;
  attestation->win_attestation.pwszFormatType = WEBAUTHN_ATTESTATION_TYPE_NONE;
  attestation->win_attestation.cbAuthenticatorData =
      attestation->authenticator_data.size();
  attestation->win_attestation.pbAuthenticatorData =
      attestation->authenticator_data.data();
  attestation->win_attestation.cbAttestation = attestation->attestation.size();
  attestation->win_attestation.pbAttestation = attestation->attestation.data();
  attestation->win_attestation.bResidentKey = resident_key;
  attestation->win_attestation.bLargeBlobSupported =
      options->dwLargeBlobSupport != WEBAUTHN_LARGE_BLOB_SUPPORT_NONE &&
      version_ >= WEBAUTHN_API_VERSION_3 && large_blob_supported_;
  attestation->win_attestation.dwUsedTransport =
      attachment == WEBAUTHN_AUTHENTICATOR_ATTACHMENT_PLATFORM
          ? WEBAUTHN_CTAP_TRANSPORT_INTERNAL
          : transport_;

  *credential_attestation_ptr = &attestation->win_attestation;
  returned_attestations_.push_back(std::move(attestation));
  bool result =
      registrations_.insert({std::move(credential_id), std::move(registration)})
          .second;
  DCHECK(result);
  return S_OK;
}

HRESULT FakeWinWebAuthnApi::AuthenticatorGetAssertion(
    HWND h_wnd,
    LPCWSTR rp_id,
    PCWEBAUTHN_CLIENT_DATA client_data,
    PCWEBAUTHN_AUTHENTICATOR_GET_ASSERTION_OPTIONS options,
    PWEBAUTHN_ASSERTION* assertion_ptr) {
  // TODO(martinkr): support AppID extension
  DCHECK(is_available_);

  if (result_override_ != S_OK) {
    return result_override_;
  }

  const auto rp_id_hash =
      fido_parsing_utils::CreateSHA256Hash(base::WideToUTF8(rp_id));

  RegistrationData* registration = nullptr;
  base::span<const uint8_t> credential_id;
  PCWEBAUTHN_CREDENTIAL_LIST allow_credentials = options->pAllowCredentialList;

  // Find a matching resident credential if allow list is empty. Windows
  // provides its own account selector, so only one credential gets returned.
  // Pretend the user selected the first match.
  if (allow_credentials->cCredentials == 0) {
    for (auto& registration_pair : registrations_) {
      if (!registration_pair.second.is_resident ||
          registration_pair.second.application_parameter != rp_id_hash) {
        continue;
      }
      credential_id = registration_pair.first;
      registration = &registration_pair.second;
      break;
    }
  }

  for (size_t i = 0; i < allow_credentials->cCredentials; i++) {
    PWEBAUTHN_CREDENTIAL_EX credential = allow_credentials->ppCredentials[i];
    base::span<const uint8_t> allow_credential_id(credential->pbId,
                                                  credential->cbId);
    auto it = registrations_.find(allow_credential_id);
    if (it == registrations_.end() ||
        it->second.application_parameter != rp_id_hash) {
      continue;
    }
    credential_id = it->first;
    registration = &it->second;
    break;
  }

  if (!registration) {
    return NTE_NOT_FOUND;
  }
  DCHECK(!credential_id.empty());

  auto result = std::make_unique<WebAuthnAssertionEx>();
  result->credential_id = fido_parsing_utils::Materialize(credential_id);
  result->authenticator_data =
      AuthenticatorData(
          registration->application_parameter,
          /*user_present=*/true,
          /*user_verified=*/options->dwUserVerificationRequirement !=
              WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED,
          /*backup_eligible=*/false, /*backup_state=*/false,
          registration->counter++,
          /*attested_credential_data=*/std::nullopt,
          /*extensions=*/std::nullopt)
          .SerializeToByteArray();

  // Create the assertion signature.
  std::vector<uint8_t> sign_data;
  fido_parsing_utils::Append(&sign_data, result->authenticator_data);
  fido_parsing_utils::Append(
      &sign_data, crypto::SHA256Hash({client_data->pbClientDataJSON,
                                      client_data->cbClientDataJSON}));
  result->signature =
      registration->private_key->Sign({sign_data.data(), sign_data.size()});

  // Fill in the WEBAUTHN_ASSERTION struct returned to the caller.
  result->assertion = {};
  switch (version_) {
    case WEBAUTHN_API_VERSION_1:
    case WEBAUTHN_API_VERSION_2:
      result->assertion.dwVersion = WEBAUTHN_ASSERTION_VERSION_1;
      break;
    case WEBAUTHN_API_VERSION_3:
      result->assertion.dwVersion = WEBAUTHN_ASSERTION_VERSION_2;
      break;
    case WEBAUTHN_API_VERSION_4:
      result->assertion.dwVersion = WEBAUTHN_ASSERTION_VERSION_3;
      break;
    default:
      NOTREACHED() << "Unknown webauthn version " << version_;
  }
  result->assertion.cbAuthenticatorData = result->authenticator_data.size();
  result->assertion.pbAuthenticatorData = reinterpret_cast<PBYTE>(
      const_cast<uint8_t*>(result->authenticator_data.data()));

  result->assertion.cbSignature = result->signature.size();
  result->assertion.pbSignature = result->signature.data();
  result->assertion.Credential = {};
  result->assertion.Credential.dwVersion = 1;
  result->assertion.Credential.cbId = result->credential_id.size();
  result->assertion.Credential.pbId = result->credential_id.data();
  result->assertion.Credential.pwszCredentialType =
      WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY;
  if (allow_credentials->cCredentials == 0) {
    result->user_id = registration->user->id;
    result->assertion.pbUserId = result->user_id->data();
    result->assertion.cbUserId = result->user_id->size();
  } else {
    result->assertion.pbUserId = nullptr;
    result->assertion.cbUserId = 0;
  }

  // Perform the large blob operation.
  result->assertion.pbCredLargeBlob = nullptr;
  result->assertion.cbCredLargeBlob = 0;
  if (options->dwCredLargeBlobOperation !=
      WEBAUTHN_CRED_LARGE_BLOB_OPERATION_NONE) {
    result->assertion.dwCredLargeBlobStatus = large_blob_result_;
  }
  if (large_blob_result_ == WEBAUTHN_CRED_LARGE_BLOB_STATUS_SUCCESS &&
      version_ >= WEBAUTHN_API_VERSION_3) {
    switch (options->dwCredLargeBlobOperation) {
      case WEBAUTHN_CRED_LARGE_BLOB_OPERATION_NONE:
        break;
      case WEBAUTHN_CRED_LARGE_BLOB_OPERATION_GET: {
        auto large_blob_it = large_blobs_.find(result->credential_id);
        if (large_blob_it != large_blobs_.end()) {
          result->large_blob = large_blob_it->second;
          result->assertion.pbCredLargeBlob = result->large_blob->data();
          result->assertion.cbCredLargeBlob = result->large_blob->size();
        } else {
          result->assertion.dwCredLargeBlobStatus =
              WEBAUTHN_CRED_LARGE_BLOB_STATUS_NOT_FOUND;
        }
        break;
      }
      case WEBAUTHN_CRED_LARGE_BLOB_OPERATION_SET: {
        std::vector<uint8_t> large_blob(
            options->pbCredLargeBlob,
            options->pbCredLargeBlob + options->cbCredLargeBlob);
        large_blobs_.emplace(result->credential_id, std::move(large_blob));
        break;
      }
      default:
        NOTREACHED() << "Unknown operation "
                     << options->dwCredLargeBlobOperation;
    }
  }

  // The real API hands out results in naked pointers and asks callers
  // to call FreeAssertion() when they're done. We maintain ownership
  // of the pointees in |returned_assertions_|.
  *assertion_ptr = &result->assertion;
  returned_assertions_.push_back(std::move(result));

  return S_OK;
}

HRESULT FakeWinWebAuthnApi::CancelCurrentOperation(GUID* cancellation_id) {
  DCHECK(is_available_);
  NOTREACHED() << "not implemented";
}

HRESULT FakeWinWebAuthnApi::GetPlatformCredentialList(
    PCWEBAUTHN_GET_CREDENTIALS_OPTIONS options,
    PWEBAUTHN_CREDENTIAL_DETAILS_LIST* credentials) {
  DCHECK(is_available_ && supports_silent_discovery_);
  last_get_credentials_options_ =
      std::make_unique<WEBAUTHN_GET_CREDENTIALS_OPTIONS>(*options);
  if (result_override_ != S_OK) {
    return result_override_;
  }
  auto credential_list = std::make_unique<CredentialInfoList>();
  for (const auto& registration : registrations_) {
    if (!registration.second.is_resident) {
      continue;
    }
    if (options->pwszRpId) {
      std::string rp_id = base::WideToUTF8(options->pwszRpId);
      if (registration.second.rp->id != rp_id) {
        continue;
      }
    }

    auto credential = std::make_unique<CredentialInfo>();
    credential->credential_id = registration.first;
    credential->rp_id = base::UTF8ToUTF16(registration.second.rp->id);
    credential->rp_name =
        base::UTF8ToUTF16(registration.second.rp->name.value_or(""));
    credential->user_id = registration.second.user->id;
    credential->user_name =
        base::UTF8ToUTF16(registration.second.user->name.value_or(""));
    credential->user_display_name =
        base::UTF8ToUTF16(registration.second.user->display_name.value_or(""));
    credential->rp = {
        .dwVersion = WEBAUTHN_RP_ENTITY_INFORMATION_CURRENT_VERSION,
        .pwszId = base::as_wcstr(credential->rp_id),
        .pwszName = base::as_wcstr(credential->rp_name),
    };
    credential->user = {
        .dwVersion = WEBAUTHN_USER_ENTITY_INFORMATION_CURRENT_VERSION,
        .cbId = static_cast<DWORD>(credential->user_id.size()),
        .pbId = credential->user_id.data(),
        .pwszName = base::as_wcstr(credential->user_name),
        .pwszDisplayName = base::as_wcstr(credential->user_display_name),
    };
    credential->details = {
        .dwVersion = WEBAUTHN_CREDENTIAL_DETAILS_VERSION_1,
        .cbCredentialID = static_cast<DWORD>(credential->credential_id.size()),
        .pbCredentialID = credential->credential_id.data(),
        .pRpInformation = &credential->rp,
        .pUserInformation = &credential->user,
        .bRemovable = true,
    };
    credential_list->win_credentials.push_back(&credential->details);
    credential_list->credentials.push_back(std::move(credential));
  }

  if (credential_list->credentials.empty()) {
    return NTE_NOT_FOUND;
  }

  credential_list->credential_details_list = {
      .cCredentialDetails =
          static_cast<DWORD>(credential_list->win_credentials.size()),
      .ppCredentialDetails = credential_list->win_credentials.data(),
  };
  returned_credential_lists_.push_back(std::move(credential_list));
  *credentials = &returned_credential_lists_.back()->credential_details_list;
  return S_OK;
}

HRESULT FakeWinWebAuthnApi::DeletePlatformCredential(
    base::span<const uint8_t> credential_id) {
  // TODO: not yet implemented.
  CHECK(false);
  return S_OK;
}

PCWSTR FakeWinWebAuthnApi::GetErrorName(HRESULT hr) {
  DCHECK(is_available_);
  // See the comment for WebAuthNGetErrorName() in <webauthn.h>.
  switch (hr) {
    case S_OK:
      return L"Success";
    case NTE_EXISTS:
      return L"InvalidStateError";
    case HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED):
    case NTE_NOT_SUPPORTED:
    case NTE_TOKEN_KEYSET_STORAGE_FULL:
      return L"ConstraintError";
    case NTE_INVALID_PARAMETER:
      return L"NotSupportedError";
    case NTE_DEVICE_NOT_FOUND:
    case NTE_NOT_FOUND:
    case HRESULT_FROM_WIN32(ERROR_CANCELLED):
    case NTE_USER_CANCELLED:
    case HRESULT_FROM_WIN32(ERROR_TIMEOUT):
      return L"NotAllowedError";
    default:
      return L"UnknownError";
  }
}

void FakeWinWebAuthnApi::FreeCredentialAttestation(
    PWEBAUTHN_CREDENTIAL_ATTESTATION credential_attestation) {
  for (auto it = returned_attestations_.begin();
       it != returned_attestations_.end(); ++it) {
    if (credential_attestation != &(*it)->win_attestation) {
      continue;
    }
    returned_attestations_.erase(it);
    return;
  }
  NOTREACHED();
}

void FakeWinWebAuthnApi::FreeAssertion(PWEBAUTHN_ASSERTION assertion) {
  for (auto it = returned_assertions_.begin(); it != returned_assertions_.end();
       ++it) {
    if (assertion != &(*it)->assertion) {
      continue;
    }
    returned_assertions_.erase(it);
    return;
  }
  NOTREACHED();
}

void FakeWinWebAuthnApi::FreePlatformCredentialList(
    PWEBAUTHN_CREDENTIAL_DETAILS_LIST credentials) {
  for (auto it = returned_credential_lists_.begin();
       it != returned_credential_lists_.end(); ++it) {
    if (credentials != &(*it)->credential_details_list) {
      continue;
    }
    returned_credential_lists_.erase(it);
    return;
  }
  NOTREACHED();
}

int FakeWinWebAuthnApi::Version() {
  return version_;
}

// static
WEBAUTHN_CREDENTIAL_ATTESTATION FakeWinWebAuthnApi::FakeAttestation() {
  WEBAUTHN_CREDENTIAL_ATTESTATION attestation = {};
  attestation.dwVersion = WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_1;
  attestation.cbAuthenticatorData =
      sizeof(test_data::kCtap2MakeCredentialAuthData);
  attestation.pbAuthenticatorData = reinterpret_cast<PBYTE>(
      const_cast<uint8_t*>(device::test_data::kCtap2MakeCredentialAuthData));
  attestation.cbAttestation =
      sizeof(test_data::kPackedAttestationStatementCBOR);
  attestation.pbAttestation = reinterpret_cast<PBYTE>(
      const_cast<uint8_t*>(device::test_data::kPackedAttestationStatementCBOR));
  attestation.cbAttestationObject = 0;
  attestation.cbCredentialId = 0;
  attestation.pwszFormatType = L"packed";
  attestation.dwAttestationDecodeType = 0;
  return attestation;
}

}  // namespace device
