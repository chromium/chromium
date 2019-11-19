// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/authenticator.h"

#include <windows.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/win/type_conversions.h"
#include "device/fido/win/webauthn_api.h"
#include "third_party/microsoft_webauthn/webauthn.h"

namespace device {

// static
bool WinWebAuthnApiAuthenticator::IsUserVerifyingPlatformAuthenticatorAvailable(
    WinWebAuthnApi* api) {
  BOOL result;
  return api && api->IsAvailable() &&
         api->IsUserVerifyingPlatformAuthenticatorAvailable(&result) == S_OK &&
         result == TRUE;
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
    MakeCredentialCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_pending_) {
    NOTREACHED();
    return;
  }
  is_pending_ = true;

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce(&AuthenticatorMakeCredentialBlocking, win_api_,
                     current_window_, cancellation_id_, std::move(request)),
      base::BindOnce(&WinWebAuthnApiAuthenticator::MakeCredentialDone,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WinWebAuthnApiAuthenticator::MakeCredentialDone(
    MakeCredentialCallback callback,
    std::pair<CtapDeviceResponseCode,
              base::Optional<AuthenticatorMakeCredentialResponse>> result) {
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
    std::move(callback).Run(result.first, base::nullopt);
    return;
  }
  if (!result.second) {
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrInvalidCBOR,
                            base::nullopt);
    return;
  }
  std::move(callback).Run(result.first, std::move(result.second));
}

void WinWebAuthnApiAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                               GetAssertionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_pending_);
  if (is_pending_)
    return;

  is_pending_ = true;

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce(&AuthenticatorGetAssertionBlocking, win_api_,
                     current_window_, cancellation_id_, std::move(request)),
      base::BindOnce(&WinWebAuthnApiAuthenticator::GetAssertionDone,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WinWebAuthnApiAuthenticator::GetAssertionDone(
    GetAssertionCallback callback,
    std::pair<CtapDeviceResponseCode,
              base::Optional<AuthenticatorGetAssertionResponse>> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_pending_);
  is_pending_ = false;
  if (waiting_for_cancellation_) {
    waiting_for_cancellation_ = false;
    return;
  }
  if (result.first != CtapDeviceResponseCode::kSuccess) {
    std::move(callback).Run(result.first, base::nullopt);
    return;
  }
  if (!result.second) {
    std::move(callback).Run(CtapDeviceResponseCode::kCtap2ErrInvalidCBOR,
                            base::nullopt);
    return;
  }
  std::move(callback).Run(result.first, std::move(result.second));
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

std::string WinWebAuthnApiAuthenticator::GetId() const {
  return "WinWebAuthnApiAuthenticator";
}

base::string16 WinWebAuthnApiAuthenticator::GetDisplayName() const {
  return base::UTF8ToUTF16(GetId());
}

bool WinWebAuthnApiAuthenticator::IsInPairingMode() const {
  return false;
}

bool WinWebAuthnApiAuthenticator::IsPaired() const {
  return false;
}

bool WinWebAuthnApiAuthenticator::RequiresBlePairingPin() const {
  return false;
}

base::Optional<FidoTransportProtocol>
WinWebAuthnApiAuthenticator::AuthenticatorTransport() const {
  // The Windows API could potentially use any external or
  // platform authenticator.
  return base::nullopt;
}

bool WinWebAuthnApiAuthenticator::IsWinNativeApiAuthenticator() const {
  return true;
}

const base::Optional<AuthenticatorSupportedOptions>&
WinWebAuthnApiAuthenticator::Options() const {
  // The request can potentially be fulfilled by any device that Windows
  // communicates with, so returning AuthenticatorSupportedOptions really
  // doesn't make much sense.
  static const base::Optional<AuthenticatorSupportedOptions> no_options =
      base::nullopt;
  return no_options;
}

base::WeakPtr<FidoAuthenticator> WinWebAuthnApiAuthenticator::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool WinWebAuthnApiAuthenticator::SupportsCredProtectExtension() const {
  return win_api_->Version() >= WEBAUTHN_API_VERSION_2;
}

bool WinWebAuthnApiAuthenticator::ShowsPrivacyNotice() const {
  return win_api_->Version() >= WEBAUTHN_API_VERSION_2;
}

}  // namespace device
