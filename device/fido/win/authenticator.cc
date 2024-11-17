// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/authenticator.h"

#include <windows.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/win/type_conversions.h"
#include "device/fido/win/webauthn_api.h"
#include "third_party/microsoft_webauthn/webauthn.h"

namespace device {

namespace {

WinWebAuthnApiAuthenticator::TestObserver* g_observer = nullptr;

AuthenticatorSupportedOptions WinWebAuthnApiOptions(int api_version) {
  AuthenticatorSupportedOptions options;
  options.is_platform_device =
      AuthenticatorSupportedOptions::PlatformDevice::kBoth;
  options.supports_resident_key = true;
  options.user_verification_availability = AuthenticatorSupportedOptions::
      UserVerificationAvailability::kSupportedAndConfigured;
  options.supports_user_presence = true;
  options.supports_cred_protect = api_version >= WEBAUTHN_API_VERSION_2;
  options.enterprise_attestation = api_version >= WEBAUTHN_API_VERSION_3;
  if (api_version >= WEBAUTHN_API_VERSION_3) {
    options.large_blob_type = LargeBlobSupportType::kBespoke;
  }
  options.supports_min_pin_length_extension =
      api_version >= WEBAUTHN_API_VERSION_3;
  if (api_version >= WEBAUTHN_API_VERSION_3) {
    // The 256-byte maximum length here is an arbitrary sanity limit. Blobs are
    // generally 32 bytes so this is intended to be larger than any reasonable
    // request.
    options.max_cred_blob_length = 256;
  }
  options.supports_hmac_secret = true;
  return options;
}

bool MayHaveWindowsHelloCredentials(
    std::vector<PublicKeyCredentialDescriptor> allow_list) {
  return allow_list.empty() ||
         base::ranges::any_of(allow_list, [](const auto& credential) {
           return credential.transports.empty() ||
                  base::Contains(credential.transports,
                                 FidoTransportProtocol::kInternal);
         });
}

// Filters credentials from |found_creds| that are not present in
// |allow_list_creds|.
void FilterFoundCredentials(
    std::vector<DiscoverableCredentialMetadata>* found_creds,
    const std::vector<PublicKeyCredentialDescriptor>& allow_list_creds) {
  auto remove_it = base::ranges::remove_if(
      *found_creds, [&allow_list_creds](const auto& found_cred) {
        return base::ranges::none_of(
            allow_list_creds, [&found_cred](const auto& allow_list_cred) {
              return allow_list_cred.id == found_cred.cred_id;
            });
      });
  found_creds->erase(remove_it, found_creds->end());
}

void SignalUnknownCredentialBlocking(WinWebAuthnApi* api,
                                     const std::vector<uint8_t>& credential_id,
                                     const std::string& relying_party_id) {
  if (!base::FeatureList::IsEnabled(device::kWebAuthnHelloSignal) || !api ||
      !api->IsAvailable() || !api->SupportsSilentDiscovery()) {
    FIDO_LOG(DEBUG) << "SignalUnknownCredential unavailable for Windows Hello";
    return;
  }
  std::vector<DiscoverableCredentialMetadata> credentials =
      device::AuthenticatorEnumerateCredentialsBlocking(
          api, base::UTF8ToUTF16(relying_party_id), /*is_incognito=*/false)
          .second;
  const auto credential_it = std::ranges::find_if(
      credentials, [credential_id](const auto& credential) {
        return credential.cred_id == credential_id;
      });
  if (credential_it == credentials.end()) {
    FIDO_LOG(DEBUG) << "Unknown credential not found on Windows Hello";
    return;
  }
  FIDO_LOG(DEBUG) << "Removing unknown Windows Hello credential";
  api->DeletePlatformCredential(credential_id);
}

void SignalAllAcceptedCredentialsBlocking(
    WinWebAuthnApi* api,
    const std::string& relying_party_id,
    const std::vector<uint8_t>& user_id,
    const std::vector<std::vector<uint8_t>>& all_accepted_credential_ids) {
  if (!base::FeatureList::IsEnabled(device::kWebAuthnHelloSignal) || !api ||
      !api->IsAvailable() || !api->SupportsSilentDiscovery()) {
    FIDO_LOG(DEBUG)
        << "SignalAllAcceptedCredentials unavailable for Windows Hello";
    return;
  }
  FIDO_LOG(DEBUG)
      << "Removing Windows Hello credentials not on accepted credentials list";
  std::vector<DiscoverableCredentialMetadata> credentials =
      device::AuthenticatorEnumerateCredentialsBlocking(
          api, base::UTF8ToUTF16(relying_party_id), /*is_incognito=*/false)
          .second;
  for (const DiscoverableCredentialMetadata& credential : credentials) {
    if (credential.user.id != user_id ||
        std::ranges::find(all_accepted_credential_ids, credential.cred_id) !=
            all_accepted_credential_ids.end()) {
      continue;
    }
    FIDO_LOG(DEBUG) << "Removing credential "
                    << base::HexEncode(credential.cred_id);
    api->DeletePlatformCredential(credential.cred_id);
  }
}

}  // namespace

// static
void WinWebAuthnApiAuthenticator::SetGlobalObserverForTesting(
    TestObserver* observer) {
  CHECK(!observer || !g_observer);
  g_observer = observer;
}

// static
void WinWebAuthnApiAuthenticator::IsUserVerifyingPlatformAuthenticatorAvailable(
    WinWebAuthnApi* api,
    base::OnceCallback<void(bool is_available)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          [](WinWebAuthnApi* api) {
            BOOL result;
            if (!api || !api->IsAvailable()) {
              return false;
            }
            return api->IsUserVerifyingPlatformAuthenticatorAvailable(
                       &result) == S_OK &&
                   result == TRUE;
          },
          api),
      std::move(callback));
}

// static
void WinWebAuthnApiAuthenticator::EnumeratePlatformCredentials(
    WinWebAuthnApi* api,
    base::OnceCallback<
        void(std::vector<device::DiscoverableCredentialMetadata>)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(AuthenticatorEnumerateCredentialsBlocking, api,
                     /*rp_id=*/std::u16string_view(),
                     /*is_incognito=*/false),
      base::BindOnce(
          [](base::OnceCallback<void(
                 std::vector<device::DiscoverableCredentialMetadata>)> callback,
             std::pair<bool, std::vector<DiscoverableCredentialMetadata>>
                 result) { std::move(callback).Run(std::move(result.second)); },
          std::move(callback)));
}

// static
void WinWebAuthnApiAuthenticator::DeletePlatformCredential(
    WinWebAuthnApi* api,
    base::span<const uint8_t> credential_id,
    base::OnceCallback<void(bool)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          [](WinWebAuthnApi* api, std::vector<uint8_t> credential_id) -> bool {
            return api && api->IsAvailable() &&
                   api->SupportsSilentDiscovery() &&
                   api->DeletePlatformCredential(credential_id) == S_OK;
          },
          api,
          std::vector<uint8_t>(credential_id.begin(), credential_id.end())),
      std::move(callback));
}

// static
void WinWebAuthnApiAuthenticator::SignalUnknownCredential(
    WinWebAuthnApi* api,
    const std::vector<uint8_t>& credential_id,
    const std::string& relying_party_id) {
  base::OnceClosure reply = base::DoNothing();
  if (g_observer) {
    reply = base::BindOnce(&TestObserver::OnSignalUnknownCredential,
                           // Unretained is fine here since this is test only.
                           base::Unretained(g_observer));
  }
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(SignalUnknownCredentialBlocking, api, credential_id,
                     relying_party_id),
      std::move(reply));
}

// static
void WinWebAuthnApiAuthenticator::SignalAllAcceptedCredentials(
    WinWebAuthnApi* api,
    const std::string& relying_party_id,
    const std::vector<uint8_t>& user_id,
    const std::vector<std::vector<uint8_t>>& all_accepted_credential_ids) {
  base::OnceClosure reply = base::DoNothing();
  if (g_observer) {
    reply = base::BindOnce(&TestObserver::OnSignalAllAcceptedCredentials,
                           // Unretained is fine here since this is test only.
                           base::Unretained(g_observer));
  }
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(SignalAllAcceptedCredentialsBlocking, api,
                     relying_party_id, user_id, all_accepted_credential_ids),
      std::move(reply));
}

WinWebAuthnApiAuthenticator::WinWebAuthnApiAuthenticator(
    HWND current_window,
    WinWebAuthnApi* win_api)
    : options_(WinWebAuthnApiOptions(win_api->Version())),
      current_window_(current_window),
      win_api_(win_api) {
  CHECK(win_api_->IsAvailable());
  CoCreateGuid(&cancellation_id_);
}

WinWebAuthnApiAuthenticator::~WinWebAuthnApiAuthenticator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Cancel();
}

void WinWebAuthnApiAuthenticator::InitializeAuthenticator(
    base::OnceClosure callback) {
  std::move(callback).Run();
}

void WinWebAuthnApiAuthenticator::MakeCredential(
    CtapMakeCredentialRequest request,
    MakeCredentialOptions options,
    MakeCredentialCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_pending_) {
    NOTREACHED();
  }
  is_pending_ = true;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce(&AuthenticatorMakeCredentialBlocking, win_api_,
                     current_window_, cancellation_id_, std::move(request),
                     std::move(options)),
      base::BindOnce(&WinWebAuthnApiAuthenticator::MakeCredentialDone,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WinWebAuthnApiAuthenticator::MakeCredentialDone(
    MakeCredentialCallback callback,
    std::pair<MakeCredentialStatus,
              std::optional<AuthenticatorMakeCredentialResponse>> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_pending_);
  is_pending_ = false;
  if (waiting_for_cancellation_) {
    // Don't bother invoking the reply callback if the caller has already
    // cancelled the operation.
    waiting_for_cancellation_ = false;
    return;
  }
  if (result.first != MakeCredentialStatus::kSuccess) {
    std::move(callback).Run(result.first, std::nullopt);
    return;
  }
  CHECK(result.second);
  std::move(callback).Run(result.first, std::move(result.second));
}

void WinWebAuthnApiAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                               CtapGetAssertionOptions options,
                                               GetAssertionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_pending_);
  if (is_pending_) {
    return;
  }

  is_pending_ = true;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce(&AuthenticatorGetAssertionBlocking, win_api_,
                     current_window_, cancellation_id_, std::move(request),
                     std::move(options)),
      base::BindOnce(&WinWebAuthnApiAuthenticator::GetAssertionDone,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WinWebAuthnApiAuthenticator::GetAssertionDone(
    GetAssertionCallback callback,
    std::pair<GetAssertionStatus,
              std::optional<AuthenticatorGetAssertionResponse>> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_pending_);
  is_pending_ = false;
  if (waiting_for_cancellation_) {
    waiting_for_cancellation_ = false;
    return;
  }
  if (result.first != GetAssertionStatus::kSuccess) {
    std::move(callback).Run(result.first, {});
    return;
  }
  CHECK(result.second);
  std::vector<AuthenticatorGetAssertionResponse> responses;
  responses.emplace_back(std::move(*result.second));
  std::move(callback).Run(result.first, std::move(responses));
}

void WinWebAuthnApiAuthenticator::GetPlatformCredentialInfoForRequest(
    const CtapGetAssertionRequest& request,
    const CtapGetAssertionOptions& request_options,
    GetPlatformCredentialInfoForRequestCallback callback) {
  // Handle the special case where a request has an allow list, all the
  // credential descriptors have a transport, and none of have the "internal"
  // transport. These credentials cannot possibly be Windows Hello.
  if (!MayHaveWindowsHelloCredentials(request.allow_list)) {
    std::move(callback).Run(
        /*credentials=*/{},
        FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential);
    return;
  }
  FIDO_LOG(DEBUG) << "Silently discovering credentials for " << request.rp_id;
  auto [success, credentials] = AuthenticatorEnumerateCredentialsBlocking(
      win_api_, base::UTF8ToUTF16(request.rp_id),
      request_options.is_off_the_record_context);
  if (!success) {
    std::move(callback).Run(
        /*credentials=*/{},
        FidoRequestHandlerBase::RecognizedCredential::kUnknown);
    return;
  }
  if (!request.allow_list.empty()) {
    FilterFoundCredentials(&credentials, request.allow_list);
  }
  auto recognized = credentials.empty()
                        ? FidoRequestHandlerBase::RecognizedCredential::
                              kNoRecognizedCredential
                        : FidoRequestHandlerBase::RecognizedCredential::
                              kHasRecognizedCredential;
  std::move(callback).Run(std::move(credentials), recognized);
}

void WinWebAuthnApiAuthenticator::GetTouch(base::OnceClosure callback) {
  NOTREACHED();
}

void WinWebAuthnApiAuthenticator::Cancel() {
  if (!is_pending_ || waiting_for_cancellation_) {
    return;
  }

  waiting_for_cancellation_ = true;
  // This returns immediately.
  win_api_->CancelCurrentOperation(&cancellation_id_);
}

AuthenticatorType WinWebAuthnApiAuthenticator::GetType() const {
  return AuthenticatorType::kWinNative;
}

std::string WinWebAuthnApiAuthenticator::GetId() const {
  return "WinWebAuthnApiAuthenticator";
}

std::optional<FidoTransportProtocol>
WinWebAuthnApiAuthenticator::AuthenticatorTransport() const {
  // The Windows API could potentially use any external or
  // platform authenticator.
  return std::nullopt;
}

const AuthenticatorSupportedOptions& WinWebAuthnApiAuthenticator::Options()
    const {
  return options_;
}

base::WeakPtr<FidoAuthenticator> WinWebAuthnApiAuthenticator::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool WinWebAuthnApiAuthenticator::ShowsResidentCredentialNotice() const {
  return win_api_->Version() >= WEBAUTHN_API_VERSION_2;
}

}  // namespace device
