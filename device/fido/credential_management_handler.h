// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CREDENTIAL_MANAGEMENT_HANDLER_H_
#define DEVICE_FIDO_CREDENTIAL_MANAGEMENT_HANDLER_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "device/fido/credential_management.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/pin.h"

namespace device {

class FidoAuthenticator;
class FidoDiscoveryFactory;

enum class CredentialManagementStatus {
  kSuccess,
  kAuthenticatorResponseInvalid,
  kSoftPINBlock,
  kHardPINBlock,
  kAuthenticatorMissingCredentialManagement,
  kNoCredentials,
  kNoPINSet,
  kForcePINChange,
};

// CredentialManagementHandler implements the authenticatorCredentialManagement
// protocol.
//
// Public methods on instances of this class may be called only after
// ReadyCallback has run, but not after FinishedCallback has run.
class COMPONENT_EXPORT(DEVICE_FIDO) CredentialManagementHandler
    : public FidoRequestHandlerBase {
 public:
  // Details of the authenticator tapped by the user that are interesting to the
  // UI when starting a request or requesting a PIN.
  struct AuthenticatorProperties {
    // The minimum accepted PIN length for the authenticator.
    uint32_t min_pin_length;
    // The number of PIN retries before the authenticator locks down.
    int64_t pin_retries;
    // True if the authenticator supports the UpdateUserInformation command (and
    // therefore, calling |UpdateUserInformation| is valid). False otherwise.
    bool supports_update_user_information;
  };

  using DeleteCredentialCallback =
      base::OnceCallback<void(CtapDeviceResponseCode)>;
  using UpdateUserInformationCallback =
      base::OnceCallback<void(CtapDeviceResponseCode)>;
  using FinishedCallback = base::OnceCallback<void(CredentialManagementStatus)>;
  using GetCredentialsCallback = base::OnceCallback<void(
      CtapDeviceResponseCode,
      std::optional<std::vector<AggregatedEnumerateCredentialsResponse>>,
      std::optional<size_t>)>;
  using GetPINCallback =
      base::RepeatingCallback<void(AuthenticatorProperties,
                                   base::OnceCallback<void(std::string)>)>;
  using ReadyCallback = base::OnceClosure;

  CredentialManagementHandler(
      FidoDiscoveryFactory* fido_discovery_factory,
      const base::flat_set<FidoTransportProtocol>& supported_transports,
      ReadyCallback ready_callback,
      GetPINCallback get_pin_callback,
      FinishedCallback finished_callback);

  CredentialManagementHandler(const CredentialManagementHandler&) = delete;
  CredentialManagementHandler& operator=(const CredentialManagementHandler&) =
      delete;

  ~CredentialManagementHandler() override;

  // GetCredentials invokes a series of commands to fetch all credentials stored
  // on the device. The supplied callback receives the status returned by the
  // device and, if successful, the resident credentials stored and remaining
  // capacity left on the chosen authenticator.
  //
  // The returned AggregatedEnumerateCredentialsResponses will be sorted in
  // ascending order by their RP ID. The |credentials| vector of each response
  // will be sorted in ascending order by user name.
  void GetCredentials(GetCredentialsCallback callback);

  // DeleteCredentials deletes a list of credentials. Each entry in
  // |credential_ids| must be a CBOR-serialized credential_id.
  // If any individual deletion fails, |callback| is invoked with the
  // respective error, and deletion of the remaining credentials will be
  // aborted (but others may have been deleted successfully already).
  void DeleteCredentials(
      std::vector<PublicKeyCredentialDescriptor> credential_ids,
      DeleteCredentialCallback callback);

  // UpdateUserInformation attempts to update the credential with the given
  // |credential_id|.
  void UpdateUserInformation(const PublicKeyCredentialDescriptor& credential_id,
                             const PublicKeyCredentialUserEntity& updated_user,
                             UpdateUserInformationCallback callback);

 private:
  enum class State {
    kWaitingForTouch,
    kGettingRetries,
    kWaitingForPIN,
    kGettingPINToken,
    kReady,
    kGettingMetadata,
    kGettingRP,
    kGettingCredentials,
    kFinished,
  };

  // FidoRequestHandlerBase:
  void DispatchRequest(FidoAuthenticator* authenticator) override;
  void AuthenticatorRemoved(FidoDiscoveryBase* discovery,
                            FidoAuthenticator* authenticator) override;

  void OnTouch(FidoAuthenticator* authenticator);
  void OnRetriesResponse(CtapDeviceResponseCode status,
                         std::optional<pin::RetriesResponse> response);
  void OnHavePIN(std::string pin);
  void OnHavePINToken(CtapDeviceResponseCode status,
                      std::optional<pin::TokenResponse> response);
  void OnInitFinished(CtapDeviceResponseCode status);
  void OnCredentialsMetadata(
      CtapDeviceResponseCode status,
      std::optional<CredentialsMetadataResponse> response);
  void OnEnumerateCredentials(
      CredentialsMetadataResponse metadata_response,
      CtapDeviceResponseCode status,
      std::optional<std::vector<AggregatedEnumerateCredentialsResponse>>
          responses);
  void OnDeleteCredentials(
      std::vector<PublicKeyCredentialDescriptor> remaining_credential_ids,
      CredentialManagementHandler::DeleteCredentialCallback callback,
      CtapDeviceResponseCode status,
      std::optional<DeleteCredentialResponse> response);

  SEQUENCE_CHECKER(sequence_checker_);

  State state_ = State::kWaitingForTouch;
  raw_ptr<FidoAuthenticator> authenticator_ = nullptr;
  std::optional<pin::TokenResponse> pin_token_;

  ReadyCallback ready_callback_;
  GetPINCallback get_pin_callback_;
  GetCredentialsCallback get_credentials_callback_;
  FinishedCallback finished_callback_;

  base::WeakPtrFactory<CredentialManagementHandler> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_CREDENTIAL_MANAGEMENT_HANDLER_H_
