// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_AUTHENTICATOR_H_
#define DEVICE_FIDO_FIDO_AUTHENTICATOR_H_

#include <cstdint>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/bio/enrollment.h"
#include "device/fido/credential_management.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/large_blob.h"

namespace device {

struct CtapGetAssertionRequest;
struct CtapGetAssertionOptions;
struct CtapMakeCredentialRequest;
struct MakeCredentialOptions;

namespace cablev2 {
class FidoTunnelDevice;
}

namespace pin {
struct RetriesResponse;
struct EmptyResponse;
class TokenResponse;
}  // namespace pin

enum class GetAssertionStatus {
  kSuccess,
  kAuthenticatorResponseInvalid,
  kUserConsentButCredentialNotRecognized,
  kUserConsentDenied,
  kAuthenticatorRemovedDuringPINEntry,
  kSoftPINBlock,
  kHardPINBlock,
  kAuthenticatorMissingResidentKeys,
  // TODO(agl): kAuthenticatorMissingUserVerification can
  // also be returned when the authenticator supports UV, but
  // there's no UI support for collecting a PIN. This could
  // be clearer.
  kAuthenticatorMissingUserVerification,
  kWinNotAllowedError,
  kHybridTransportError,
  kICloudKeychainNoCredentials,
  kEnclaveError,
  kEnclaveCancel,
};

enum class MakeCredentialStatus {
  kSuccess,
  kAuthenticatorResponseInvalid,
  kUserConsentButCredentialExcluded,
  kUserConsentDenied,
  kAuthenticatorRemovedDuringPINEntry,
  kSoftPINBlock,
  kHardPINBlock,
  kAuthenticatorMissingResidentKeys,
  // TODO(agl): kAuthenticatorMissingUserVerification can
  // also be returned when the authenticator supports UV, but
  // there's no UI support for collecting a PIN. This could
  // be clearer.
  kAuthenticatorMissingUserVerification,
  kAuthenticatorMissingLargeBlob,
  kNoCommonAlgorithms,
  kStorageFull,
  kWinInvalidStateError,
  kWinNotAllowedError,
  kHybridTransportError,
  kEnclaveError,
  kEnclaveCancel,
};

// FidoAuthenticator is an authenticator from the WebAuthn Authenticator model
// (https://www.w3.org/TR/webauthn/#sctn-authenticator-model). It may be a
// physical device, or a built-in (platform) authenticator.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoAuthenticator {
 public:
  using MakeCredentialCallback = base::OnceCallback<void(
      MakeCredentialStatus,
      std::optional<AuthenticatorMakeCredentialResponse>)>;
  using GetAssertionCallback =
      base::OnceCallback<void(GetAssertionStatus,
                              std::vector<AuthenticatorGetAssertionResponse>)>;
  using GetPlatformCredentialInfoForRequestCallback = base::OnceCallback<void(
      std::vector<DiscoverableCredentialMetadata> credentials,
      FidoRequestHandlerBase::RecognizedCredential has_credentials)>;

  using GetRetriesCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              std::optional<pin::RetriesResponse>)>;
  using GetTokenCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              std::optional<pin::TokenResponse>)>;
  using SetPINCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              std::optional<pin::EmptyResponse>)>;
  using ResetCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              std::optional<pin::EmptyResponse>)>;
  using GetCredentialsMetadataCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              std::optional<CredentialsMetadataResponse>)>;
  using EnumerateCredentialsCallback = base::OnceCallback<void(
      CtapDeviceResponseCode,
      std::optional<std::vector<AggregatedEnumerateCredentialsResponse>>)>;
  using DeleteCredentialCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              std::optional<DeleteCredentialResponse>)>;
  using UpdateUserInformationCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              std::optional<UpdateUserInformationResponse>)>;
  using BioEnrollmentCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              std::optional<BioEnrollmentResponse>)>;

  FidoAuthenticator() = default;

  FidoAuthenticator(const FidoAuthenticator&) = delete;
  FidoAuthenticator& operator=(const FidoAuthenticator&) = delete;

  virtual ~FidoAuthenticator() = default;

  // Sends GetInfo request to connected authenticator. Once response to GetInfo
  // call is received, |callback| is invoked. Below MakeCredential() and
  // GetAssertion() must only called after |callback| is invoked.
  virtual void InitializeAuthenticator(base::OnceClosure callback) = 0;

  // ExcludeAppIdCredentialsBeforeMakeCredential allows a device to probe for
  // credential IDs from a request that used the appidExclude extension. This
  // assumes that |MakeCredential| will be called afterwards with the same
  // request. I.e. this function may do nothing if it believes that it can
  // better handle the exclusion during |MakeCredential|.
  //
  // The optional bool is an unused response value as all the information is
  // contained in the response code, which will be |kCtap2ErrCredentialExcluded|
  // if an excluded credential is found. (An optional<void> is an error.)
  virtual void ExcludeAppIdCredentialsBeforeMakeCredential(
      CtapMakeCredentialRequest request,
      MakeCredentialOptions options,
      base::OnceCallback<void(CtapDeviceResponseCode, std::optional<bool>)>);

  // Makes a FIDO credential given |request| and |options|.
  // https://drafts.fidoalliance.org/fido-2/stable-links-to-latest/fido-client-to-authenticator-protocol.html#authenticatorMakeCredential
  //
  // This can take an arbitrary amount of time since the authenticator may
  // prompt the user to satisfy user presence. |callback| will be executed with
  // either a kSuccess status code and a valid response, or any other (error)
  // code and an empty response.
  virtual void MakeCredential(CtapMakeCredentialRequest request,
                              MakeCredentialOptions options,
                              MakeCredentialCallback callback) = 0;

  // Requests a FIDO assertion given |request| and |options|.
  // https://drafts.fidoalliance.org/fido-2/stable-links-to-latest/fido-client-to-authenticator-protocol.html#authenticatorGetAssertion
  //
  // This can take an arbitrary amount of time since the authenticator may
  // prompt the user to satisfy user presence. |callback| will be executed with
  // either a kSuccess status code and at least one valid response, or any other
  // (error) code and an empty response.
  virtual void GetAssertion(CtapGetAssertionRequest request,
                            CtapGetAssertionOptions options,
                            GetAssertionCallback callback) = 0;

  // GetPlatformCredentialInfoForRequest returns whether there are platform
  // credentials applicable for |request|, and if supported, a list of the
  // corresponding resident credential metadata for empty allow list requests.
  // This is only valid to call for internal authenticators, or for the Windows
  // native authenticator (in which case the result will reflect its platform
  // authenticator).
  virtual void GetPlatformCredentialInfoForRequest(
      const CtapGetAssertionRequest& request,
      const CtapGetAssertionOptions& options,
      GetPlatformCredentialInfoForRequestCallback callback);
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
                           std::vector<pin::Permissions> permissions,
                           std::optional<std::string> rp_id,
                           GetTokenCallback callback);
  // Returns |true| if the authenticator supports GetUvToken.
  virtual bool CanGetUvToken();
  // GetUvToken uses internal user verification to request a PinUvAuthToken from
  // an authenticator. It is only valid to call this method if CanGetUvToken()
  // returns true.
  // |rp_id| must be set if the PinUvAuthToken will be used for MakeCredential
  // or GetAssertion.
  virtual void GetUvToken(std::vector<pin::Permissions> permissions,
                          std::optional<std::string> rp_id,
                          GetTokenCallback callback);
  // Returns the minimum PIN length for this authenticator's currently set PIN.
  virtual uint32_t CurrentMinPINLength();
  // Returns the minimum PIN length required to set a new PIN for this
  // authenticator.
  virtual uint32_t NewMinPINLength();
  // Returns |true| if the PIN must be changed before attempting to obtain a PIN
  // token.
  virtual bool ForcePINChange();
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

  // PINUVDisposition enumerates the possible options for obtaining user
  // verification when making a CTAP2 request.
  enum class PINUVDisposition {
    // The authenticator doesn't support user verification, which is ok because
    // the request doesn't require it.
    kUVNotSupportedNorRequired,
    // No UV (neither clientPIN nor internal) is needed to make this
    // credential.
    kNoUVRequired,
    // A PIN/UV Auth Token should be used to make this credential. The token
    // needs to be obtained via clientPIN or internal UV, depending on which
    // modality the device supports. The modality may need to be set up first.
    kGetToken,
    // The request should be sent with the `uv` bit set to true, in order to
    // perform internal user verification without a PIN/UV Auth Token.
    kNoTokenInternalUV,
    // Same as kNoTokenInternalUV, but a PIN can be used as a fallback. (A PIN
    // may have to be set first.)
    kNoTokenInternalUVPINFallback,
    // The request cannot be satisfied by this authenticator.
    kUnsatisfiable,
  };

  // PINUVDisposition returns whether and how user verification
  // should be obtained in order to serve the given request on this
  // authenticator.
  virtual PINUVDisposition PINUVDispositionForMakeCredential(
      const CtapMakeCredentialRequest& request,
      const FidoRequestHandlerBase::Observer* observer);

  // WillNeedPINToGetAssertion returns whether a PIN prompt will be needed to
  // serve the given request on this authenticator.
  virtual PINUVDisposition PINUVDispositionForGetAssertion(
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

  virtual bool SupportsUpdateUserInformation() const;
  virtual void UpdateUserInformation(
      const pin::TokenResponse& pin_token,
      const PublicKeyCredentialDescriptor& credential_id,
      const PublicKeyCredentialUserEntity& updated_user,
      UpdateUserInformationCallback callback);

  // Biometric enrollment commands.
  virtual void GetModality(BioEnrollmentCallback callback);
  virtual void GetSensorInfo(BioEnrollmentCallback callback);
  virtual void BioEnrollFingerprint(
      const pin::TokenResponse&,
      std::optional<std::vector<uint8_t>> template_id,
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

  // Removes all stored large blobs that conform to the large blob CBOR
  // structure without a corresponding discoverable credential.
  virtual void GarbageCollectLargeBlob(
      const pin::TokenResponse& pin_uv_auth_token,
      base::OnceCallback<void(CtapDeviceResponseCode)> callback);

  // GetAlgorithms returns the list of supported COSEAlgorithmIdentifiers, or
  // |nullopt| if this is unknown and thus all requests should be tried in case
  // they work.
  virtual std::optional<base::span<const int32_t>> GetAlgorithms();

  // DiscoverableCredentialStorageFull returns true if creation of a
  // discoverable credential is likely to fail because authenticator storage is
  // exhausted. Even if this method returns false, credential creation may still
  // fail with `CTAP2_ERR_KEY_STORE_FULL` on some authenticators.
  virtual bool DiscoverableCredentialStorageFull() const;

  // Reset triggers a reset operation on the authenticator. This erases all
  // stored resident keys and any configured PIN.
  virtual void Reset(ResetCallback callback);
  virtual void Cancel() = 0;

  // GetType returns the type of the authenticator.
  virtual AuthenticatorType GetType() const;

  // Returns this object, as a tunnel device, or null if this object isn't of
  // the correct type.
  virtual cablev2::FidoTunnelDevice* GetTunnelDevice();

  // GetId returns a unique string representing this device. This string should
  // be distinct from all other devices concurrently discovered.
  virtual std::string GetId() const = 0;
  // GetDisplayName returns a string identifying a device to a human, which
  // might not be unique. For example, |GetDisplayName| could return the VID:PID
  // of a HID device, but |GetId| could not because two devices can share the
  // same VID:PID. It defaults to returning the value of |GetId|.
  virtual std::string GetDisplayName() const;
  virtual ProtocolVersion SupportedProtocol() const;
  virtual const AuthenticatorSupportedOptions& Options() const = 0;
  virtual std::optional<FidoTransportProtocol> AuthenticatorTransport()
      const = 0;
  virtual base::WeakPtr<FidoAuthenticator> GetWeakPtr() = 0;
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_AUTHENTICATOR_H_
