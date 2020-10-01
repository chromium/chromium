// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_AUTHENTICATOR_H_
#define DEVICE_FIDO_FIDO_AUTHENTICATOR_H_

#include <cstdint>
#include <string>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/bio/enrollment.h"
#include "device/fido/credential_management.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/large_blob.h"

namespace device {

struct CtapGetAssertionRequest;
struct CtapGetAssertionOptions;
struct CtapMakeCredentialRequest;

namespace pin {
struct RetriesResponse;
struct EmptyResponse;
class TokenResponse;
}  // namespace pin

// FidoAuthenticator is an authenticator from the WebAuthn Authenticator model
// (https://www.w3.org/TR/webauthn/#sctn-authenticator-model). It may be a
// physical device, or a built-in (platform) authenticator.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoAuthenticator {
 public:
  using MakeCredentialCallback = base::OnceCallback<void(
      CtapDeviceResponseCode,
      base::Optional<AuthenticatorMakeCredentialResponse>)>;
  using GetAssertionCallback = base::OnceCallback<void(
      CtapDeviceResponseCode,
      base::Optional<AuthenticatorGetAssertionResponse>)>;
  using GetRetriesCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              base::Optional<pin::RetriesResponse>)>;
  using GetTokenCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              base::Optional<pin::TokenResponse>)>;
  using SetPINCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              base::Optional<pin::EmptyResponse>)>;
  using ResetCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              base::Optional<pin::EmptyResponse>)>;
  using GetCredentialsMetadataCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              base::Optional<CredentialsMetadataResponse>)>;
  using EnumerateCredentialsCallback = base::OnceCallback<void(
      CtapDeviceResponseCode,
      base::Optional<std::vector<AggregatedEnumerateCredentialsResponse>>)>;
  using DeleteCredentialCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              base::Optional<DeleteCredentialResponse>)>;
  using BioEnrollmentCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              base::Optional<BioEnrollmentResponse>)>;
  using LargeBlobReadCallback = base::OnceCallback<void(
      CtapDeviceResponseCode,
      base::Optional<std::vector<std::pair<LargeBlobKey, std::vector<uint8_t>>>>
          callback)>;

  FidoAuthenticator() = default;
  virtual ~FidoAuthenticator() = default;

  // Sends GetInfo request to connected authenticator. Once response to GetInfo
  // call is received, |callback| is invoked. Below MakeCredential() and
  // GetAssertion() must only called after |callback| is invoked.
  virtual void InitializeAuthenticator(base::OnceClosure callback) = 0;
  virtual void MakeCredential(CtapMakeCredentialRequest request,
                              MakeCredentialCallback callback) = 0;
  virtual void GetAssertion(CtapGetAssertionRequest request,
                            CtapGetAssertionOptions options,
                            GetAssertionCallback callback) = 0;
  // GetNextAssertion fetches the next assertion from a device that indicated in
  // the response to |GetAssertion| that multiple results were available.
  virtual void GetNextAssertion(GetAssertionCallback callback);
  // GetTouch causes an (external) authenticator to flash and wait for a touch.
  virtual void GetTouch(base::OnceCallback<void()> callback);
  // GetPinRetries gets the number of PIN attempts remaining before an
  // authenticator locks. It is only valid to call this method if |Options|
  // indicates that the authenticator supports PINs.
  virtual void GetPinRetries(GetRetriesCallback callback);
  // GetUvRetries gets the number of internal user verification attempts before
  // internal user verification locks. It is only valid to call this method if
  // |Options| indicates that the authenticator supports user verification.
  virtual void GetUvRetries(GetRetriesCallback callback);
  // GetPINToken uses the given PIN to request a PinUvAuthToken from an
  // authenticator. It is only valid to call this method if |Options| indicates
  // that the authenticator supports PINs.
  // |permissions| are flags indicating which commands the token may be used
  // for.
  // |rp_id| binds the token to operations related to a given RP ID. |rp_id|
  // must be set if |permissions| includes MakeCredential or GetAssertion.
  virtual void GetPINToken(std::string pin,
                           const std::vector<pin::Permissions>& permissions,
                           base::Optional<std::string> rp_id,
                           GetTokenCallback callback);
  // Returns |true| if the authenticator supports GetUvToken.
  virtual bool CanGetUvToken();
  // GetUvToken uses internal user verification to request a PinUvAuthToken from
  // an authenticator. It is only valid to call this method if CanGetUvToken()
  // returns true.
  // |rp_id| must be set if the PinUvAuthToken will be used for MakeCredential
  // or GetAssertion.
  virtual void GetUvToken(base::Optional<std::string> rp_id,
                          GetTokenCallback callback);
  // SetPIN sets a new PIN on a device that does not currently have one. The
  // length of |pin| must respect |pin::kMinLength| and |pin::kMaxLength|. It is
  // only valid to call this method if |Options| indicates that the
  // authenticator supports PINs.
  virtual void SetPIN(const std::string& pin, SetPINCallback callback);
  // ChangePIN alters the PIN on a device that already has a PIN set. The
  // length of |pin| must respect |pin::kMinLength| and |pin::kMaxLength|. It is
  // only valid to call this method if |Options| indicates that the
  // authenticator supports PINs.
  virtual void ChangePIN(const std::string& old_pin,
                         const std::string& new_pin,
                         SetPINCallback callback);

  // MakeCredentialPINDisposition enumerates the possible interactions between
  // a user-verification level, the PIN configuration of an authenticator, and
  // whether the embedder is capable of collecting PINs from the user.
  enum class MakeCredentialPINDisposition {
    // kNoPIN means that a PIN will not be needed to make this credential.
    kNoPIN,
    // kUsePIN means that a PIN must be gathered and used to make this
    // credential.
    kUsePIN,
    // kUsePINForFallback means that a PIN may be used for fallback if internal
    // user verification fails.
    kUsePINForFallback,
    // kSetPIN means that the operation should set and then use a PIN to
    // make this credential.
    kSetPIN,
    // kUnsatisfiable means that the request cannot be satisfied by this
    // authenticator.
    kUnsatisfiable,
  };
  // WillNeedPINToMakeCredential returns what type of PIN intervention will be
  // needed to serve the given request on this authenticator.
  virtual MakeCredentialPINDisposition WillNeedPINToMakeCredential(
      const CtapMakeCredentialRequest& request,
      const FidoRequestHandlerBase::Observer* observer);

  // GetAssertionPINDisposition enumerates the possible interactions between
  // a user-verification level and the PIN support of an authenticator when
  // getting an assertion.
  enum class GetAssertionPINDisposition {
    // kNoPIN means that a PIN will not be needed for this assertion.
    kNoPIN,
    // kUsePIN means that a PIN must be gathered and used for this assertion.
    kUsePIN,
    // kUsePINForFallback means that a PIN may be used for fallback if internal
    // user verification fails.
    kUsePINForFallback,
    // kUnsatisfiable means that the request cannot be satisfied by this
    // authenticator.
    kUnsatisfiable,
  };
  // WillNeedPINToGetAssertion returns whether a PIN prompt will be needed to
  // serve the given request on this authenticator.
  virtual GetAssertionPINDisposition WillNeedPINToGetAssertion(
      const CtapGetAssertionRequest& request,
      const FidoRequestHandlerBase::Observer* observer);

  virtual void GetCredentialsMetadata(const pin::TokenResponse& pin_token,
                                      GetCredentialsMetadataCallback callback);
  virtual void EnumerateCredentials(const pin::TokenResponse& pin_token,
                                    EnumerateCredentialsCallback callback);
  virtual void DeleteCredential(
      const pin::TokenResponse& pin_token,
      const PublicKeyCredentialDescriptor& credential_id,
      DeleteCredentialCallback callback);

  // Biometric enrollment commands.
  virtual void GetModality(BioEnrollmentCallback callback);
  virtual void GetSensorInfo(BioEnrollmentCallback callback);
  virtual void BioEnrollFingerprint(
      const pin::TokenResponse&,
      base::Optional<std::vector<uint8_t>> template_id,
      BioEnrollmentCallback);
  virtual void BioEnrollCancel(BioEnrollmentCallback);
  virtual void BioEnrollEnumerate(const pin::TokenResponse&,
                                  BioEnrollmentCallback);
  virtual void BioEnrollRename(const pin::TokenResponse&,
                               std::vector<uint8_t> template_id,
                               std::string name,
                               BioEnrollmentCallback);
  virtual void BioEnrollDelete(const pin::TokenResponse&,
                               std::vector<uint8_t> template_id,
                               BioEnrollmentCallback);

  // Large blob commands.
  // Attempts to write a |large_blob| into the credential. If there is an
  // existing credential for the |large_blob_key|, it will be overwritten.
  virtual void WriteLargeBlob(
      const std::vector<uint8_t>& large_blob,
      const LargeBlobKey& large_blob_key,
      base::Optional<pin::TokenResponse> pin_uv_auth_token,
      base::OnceCallback<void(CtapDeviceResponseCode)> callback);
  // Attempts to read large blobs from the credential encrypted with
  // |large_blob_keys|. Returns a map of keys to their blobs.
  virtual void ReadLargeBlob(
      const std::vector<LargeBlobKey>& large_blob_keys,
      base::Optional<pin::TokenResponse> pin_uv_auth_token,
      LargeBlobReadCallback callback);

  // GetAlgorithms returns the list of supported COSEAlgorithmIdentifiers, or
  // |nullopt| if this is unknown and thus all requests should be tried in case
  // they work.
  virtual base::Optional<base::span<const int32_t>> GetAlgorithms();

  // Reset triggers a reset operation on the authenticator. This erases all
  // stored resident keys and any configured PIN.
  virtual void Reset(ResetCallback callback);
  virtual void Cancel() = 0;
  virtual std::string GetId() const = 0;
  virtual base::string16 GetDisplayName() const = 0;
  virtual ProtocolVersion SupportedProtocol() const;
  virtual bool SupportsCredProtectExtension() const;
  virtual bool SupportsHMACSecretExtension() const;
  virtual bool SupportsEnterpriseAttestation() const;
  virtual const base::Optional<AuthenticatorSupportedOptions>& Options()
      const = 0;
  virtual base::Optional<FidoTransportProtocol> AuthenticatorTransport()
      const = 0;
  virtual bool IsInPairingMode() const = 0;
  virtual bool IsPaired() const = 0;
  virtual bool RequiresBlePairingPin() const = 0;
#if defined(OS_WIN)
  virtual bool IsWinNativeApiAuthenticator() const = 0;
#endif  // defined(OS_WIN)
#if defined(OS_MAC)
  virtual bool IsTouchIdAuthenticator() const = 0;
#endif  // defined(OS_MAC)
#if defined(OS_CHROMEOS)
  virtual bool IsChromeOSAuthenticator() const = 0;
#endif
  virtual base::WeakPtr<FidoAuthenticator> GetWeakPtr() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FidoAuthenticator);
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_AUTHENTICATOR_H_
