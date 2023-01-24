// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/authenticator.h"

#include <windows.h>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/win/type_conversions.h"
#include "device/fido/win/webauthn_api.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/microsoft_webauthn/webauthn.h"

namespace device {

namespace {

struct PlatformCredentialListDeleter {
  explicit PlatformCredentialListDeleter(WinWebAuthnApi* win_api)
      : win_api_(win_api) {}
  inline void operator()(PWEBAUTHN_CREDENTIAL_DETAILS_LIST ptr) const {
    win_api_->FreePlatformCredentialList(ptr);
  }
  raw_ptr<WinWebAuthnApi> win_api_;
};

}  // namespace

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
            return api && api->IsAvailable() &&
                   api->IsUserVerifyingPlatformAuthenticatorAvailable(
                       &result) == S_OK &&
                   result == TRUE;
          },
          api),
      std::move(callback));
}

// static
void WinWebAuthnApiAuthenticator::IsConditionalMediationAvailable(
    WinWebAuthnApi* api,
    base::OnceCallback<void(bool is_available)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          [](WinWebAuthnApi* api) {
            return api && api->IsAvailable() && api->SupportsSilentDiscovery();
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
      base::BindOnce(
          [](WinWebAuthnApi* api)
              -> std::vector<device::DiscoverableCredentialMetadata> {
            if (!api || !api->IsAvailable() ||
                !api->SupportsSilentDiscovery()) {
              return {};
            }

            WEBAUTHN_GET_CREDENTIALS_OPTIONS options{
                .dwVersion = WEBAUTHN_GET_CREDENTIALS_OPTIONS_VERSION_1,
                .pwszRpId = nullptr,
                .bBrowserInPrivateMode = false};

            PWEBAUTHN_CREDENTIAL_DETAILS_LIST credentials = nullptr;
            HRESULT hresult =
                api->GetPlatformCredentialList(&options, &credentials);
            std::unique_ptr<WEBAUTHN_CREDENTIAL_DETAILS_LIST,
                            PlatformCredentialListDeleter>
                credentials_deleter(credentials,
                                    PlatformCredentialListDeleter(api));

            if (hresult != S_OK) {
              return {};
            }
            return WinCredentialDetailsListToCredentialMetadata(*credentials);
          },
          api),
      std::move(callback));
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

WinWebAuthnApiAuthenticator::WinWebAuthnApiAuthenticator(
    HWND current_window,
    WinWebAuthnApi* win_api)
    : current_window_(current_window), win_api_(win_api) {
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
    return;
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
    std::pair<CtapDeviceResponseCode,
              absl::optional<AuthenticatorMakeCredentialResponse>> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_pending_);
  is_pending_ = false;
  if (waiting_for_cancellation_) {
    // Don't bother invoking the reply callback if the caller has already
    // cancelled the operation.
    waiting_for_cancellation_ = false;
    return;
  }
  if (result.first != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(result.first, absl::nullopt);
    return;
  }
  if (!result.second) {
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrInvalidCBOR,
                            absl::nullopt);
    return;
  }
  std::move(callback).Run(result.first, std::move(result.second));
}

void WinWebAuthnApiAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                               CtapGetAssertionOptions options,
                                               GetAssertionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_pending_);
  if (is_pending_)
    return;

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
    std::pair<CtapDeviceResponseCode,
              absl::optional<AuthenticatorGetAssertionResponse>> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_pending_);
  is_pending_ = false;
  if (waiting_for_cancellation_) {
    waiting_for_cancellation_ = false;
    return;
  }
  if (result.first != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(result.first, {});
    return;
  }
  if (!result.second) {
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrInvalidCBOR, {});
    return;
  }
  std::vector<AuthenticatorGetAssertionResponse> responses;
  responses.emplace_back(std::move(*result.second));
  std::move(callback).Run(result.first, std::move(responses));
}

void WinWebAuthnApiAuthenticator::GetCredentialInformationForRequest(
    const CtapGetAssertionRequest& request,
    base::OnceCallback<void(std::vector<DiscoverableCredentialMetadata>, bool)>
        callback) {
  // Since the Windows authenticator forwards requests to other devices such as
  // security keys, we cannot know if there are no available credentials for a
  // given request. Therefore, this function always sets has_credentials to
  // true.
  if (!request.allow_list.empty()) {
    std::move(callback).Run(/*credentials=*/{}, /*has_credentials=*/true);
    return;
  }
  if (!win_api_->SupportsSilentDiscovery()) {
    // The Windows platform authenticator is the only authenticator available to
    // us and we can't know if there are credentials in advance. Assume there
    // are credentials available.
    FIDO_LOG(DEBUG) << "Windows API version does not support silent discovery";
    std::move(callback).Run(/*credentials=*/{}, /*has_credentials=*/true);
    return;
  }
  FIDO_LOG(DEBUG) << "Silently discovering credentials for " << request.rp_id;
  std::u16string rp_id = base::UTF8ToUTF16(request.rp_id);
  WEBAUTHN_GET_CREDENTIALS_OPTIONS options{
      .dwVersion = WEBAUTHN_GET_CREDENTIALS_OPTIONS_VERSION_1,
      .pwszRpId = base::as_wcstr(rp_id),
      // TODO(nsatragno): plumb browser private mode status in.
      .bBrowserInPrivateMode = false};
  PWEBAUTHN_CREDENTIAL_DETAILS_LIST credentials = nullptr;
  HRESULT hresult = win_api_->GetPlatformCredentialList(&options, &credentials);
  std::unique_ptr<WEBAUTHN_CREDENTIAL_DETAILS_LIST,
                  PlatformCredentialListDeleter>
      credentials_deleter(credentials, PlatformCredentialListDeleter(win_api_));

  switch (hresult) {
    case S_OK: {
      std::vector<DiscoverableCredentialMetadata> result =
          WinCredentialDetailsListToCredentialMetadata(*credentials);
      FIDO_LOG(DEBUG) << "Found " << result.size() << " credentials";
      std::move(callback).Run(std::move(result), /*has_credentials=*/true);
      return;
    }
    case NTE_NOT_FOUND:
      FIDO_LOG(DEBUG) << "No credentials found";
      std::move(callback).Run(/*credentials=*/{}, /*has_credentials=*/true);
      return;
    default:
      FIDO_LOG(ERROR) << "Windows API returned unknown result: " << hresult;
      std::move(callback).Run(/*credentials=*/{}, /*has_credentials=*/true);
      return;
  }
}

void WinWebAuthnApiAuthenticator::GetTouch(base::OnceClosure callback) {
  NOTREACHED();
}

void WinWebAuthnApiAuthenticator::Cancel() {
  if (!is_pending_ || waiting_for_cancellation_)
    return;

  waiting_for_cancellation_ = true;
  // This returns immediately.
  win_api_->CancelCurrentOperation(&cancellation_id_);
}

FidoAuthenticator::Type WinWebAuthnApiAuthenticator::GetType() const {
  return Type::kWinNative;
}

std::string WinWebAuthnApiAuthenticator::GetId() const {
  return "WinWebAuthnApiAuthenticator";
}

absl::optional<FidoTransportProtocol>
WinWebAuthnApiAuthenticator::AuthenticatorTransport() const {
  // The Windows API could potentially use any external or
  // platform authenticator.
  return absl::nullopt;
}

bool WinWebAuthnApiAuthenticator::SupportsCredProtectExtension() const {
  return win_api_->Version() >= WEBAUTHN_API_VERSION_2;
}

bool WinWebAuthnApiAuthenticator::SupportsHMACSecretExtension() const {
  return true;
}

bool WinWebAuthnApiAuthenticator::SupportsEnterpriseAttestation() const {
  return win_api_->Version() >= WEBAUTHN_API_VERSION_3;
}

bool WinWebAuthnApiAuthenticator::SupportsCredBlobOfSize(
    size_t num_bytes) const {
  return win_api_->Version() >= WEBAUTHN_API_VERSION_3;
}

bool WinWebAuthnApiAuthenticator::SupportsLargeBlobs() const {
  return win_api_->SupportsLargeBlobs();
}

const absl::optional<AuthenticatorSupportedOptions>&
WinWebAuthnApiAuthenticator::Options() const {
  // The request can potentially be fulfilled by any device that Windows
  // communicates with, so returning AuthenticatorSupportedOptions really
  // doesn't make much sense.
  static const absl::optional<AuthenticatorSupportedOptions> no_options =
      absl::nullopt;
  return no_options;
}

base::WeakPtr<FidoAuthenticator> WinWebAuthnApiAuthenticator::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool WinWebAuthnApiAuthenticator::ShowsPrivacyNotice() const {
  return win_api_->Version() >= WEBAUTHN_API_VERSION_2;
}

}  // namespace device
