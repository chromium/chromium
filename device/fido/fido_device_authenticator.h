// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_DEVICE_AUTHENTICATOR_H_
#define DEVICE_FIDO_FIDO_DEVICE_AUTHENTICATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "device/fido/ctap2_device_operation.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/large_blob.h"
#include "device/fido/pin.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

struct CtapGetAssertionRequest;
struct CtapGetAssertionOptions;
struct CtapMakeCredentialRequest;
struct EnumerateRPsResponse;
class FidoDevice;
class FidoTask;

// Adaptor class from a |FidoDevice| to the |FidoAuthenticator| interface.
// Responsible for translating WebAuthn-level requests into serializations that
// can be passed to the device for transport.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoDeviceAuthenticator
    : public FidoAuthenticator {
 public:
  explicit FidoDeviceAuthenticator(std::unique_ptr<FidoDevice> device);

  FidoDeviceAuthenticator(const FidoDeviceAuthenticator&) = delete;
  FidoDeviceAuthenticator& operator=(const FidoDeviceAuthenticator&) = delete;

  ~FidoDeviceAuthenticator() override;

  // FidoAuthenticator:
  void InitializeAuthenticator(base::OnceClosure callback) override;
  void ExcludeAppIdCredentialsBeforeMakeCredential(
      CtapMakeCredentialRequest request,
      MakeCredentialOptions options,
      base::OnceCallback<void(CtapDeviceResponseCode, absl::optional<bool>)>)
      override;
  void MakeCredential(CtapMakeCredentialRequest request,
                      MakeCredentialOptions options,
                      MakeCredentialCallback callback) override;
  void GetAssertion(CtapGetAssertionRequest request,
                    CtapGetAssertionOptions options,
                    GetAssertionCallback callback) override;
  void GetNextAssertion(GetAssertionCallback callback) override;
  void GetTouch(base::OnceClosure callback) override;
  void GetPinRetries(GetRetriesCallback callback) override;
  void GetPINToken(std::string pin,
                   std::vector<pin::Permissions> permissions,
                   absl::optional<std::string> rp_id,
                   GetTokenCallback callback) override;
  void GetUvRetries(GetRetriesCallback callback) override;
  bool CanGetUvToken() override;
  void GetUvToken(std::vector<pin::Permissions> permissions,
                  absl::optional<std::string> rp_id,
                  GetTokenCallback callback) override;
  uint32_t CurrentMinPINLength() override;
  uint32_t NewMinPINLength() override;
  bool ForcePINChange() override;
  void SetPIN(const std::string& pin, SetPINCallback callback) override;
  void ChangePIN(const std::string& old_pin,
                 const std::string& new_pin,
                 SetPINCallback callback) override;
  PINUVDisposition PINUVDispositionForMakeCredential(
      const CtapMakeCredentialRequest& request,
      const FidoRequestHandlerBase::Observer* observer) override;

  // WillNeedPINToGetAssertion returns whether a PIN prompt will be needed to
  // serve the given request on this authenticator.
  PINUVDisposition PINUVDispositionForGetAssertion(
      const CtapGetAssertionRequest& request,
      const FidoRequestHandlerBase::Observer* observer) override;

  void GetCredentialsMetadata(const pin::TokenResponse& pin_token,
                              GetCredentialsMetadataCallback callback) override;
  void EnumerateCredentials(const pin::TokenResponse& pin_token,
                            EnumerateCredentialsCallback callback) override;
  void DeleteCredential(const pin::TokenResponse& pin_token,
                        const PublicKeyCredentialDescriptor& credential_id,
                        DeleteCredentialCallback callback) override;
  bool SupportsUpdateUserInformation() const override;
  void UpdateUserInformation(const pin::TokenResponse& pin_token,
                             const PublicKeyCredentialDescriptor& credential_id,
                             const PublicKeyCredentialUserEntity& updated_user,
                             UpdateUserInformationCallback callback) override;

  void GetModality(BioEnrollmentCallback callback) override;
  void GetSensorInfo(BioEnrollmentCallback callback) override;
  void BioEnrollFingerprint(const pin::TokenResponse&,
                            absl::optional<std::vector<uint8_t>> template_id,
                            BioEnrollmentCallback) override;
  void BioEnrollCancel(BioEnrollmentCallback) override;
  void BioEnrollEnumerate(const pin::TokenResponse&,
                          BioEnrollmentCallback) override;
  void BioEnrollRename(const pin::TokenResponse&,
                       std::vector<uint8_t> template_id,
                       std::string name,
                       BioEnrollmentCallback) override;
  void BioEnrollDelete(const pin::TokenResponse&,
                       std::vector<uint8_t> template_id,
                       BioEnrollmentCallback) override;
  void WriteLargeBlob(
      LargeBlob large_blob,
      const LargeBlobKey& large_blob_key,
      absl::optional<pin::TokenResponse> pin_uv_auth_token,
      base::OnceCallback<void(CtapDeviceResponseCode)> callback) override;
  void ReadLargeBlob(const std::vector<LargeBlobKey>& large_blob_keys,
                     absl::optional<pin::TokenResponse> pin_uv_auth_token,
                     LargeBlobReadCallback callback) override;

  absl::optional<base::span<const int32_t>> GetAlgorithms() override;
  bool DiscoverableCredentialStorageFull() const override;

  void Reset(ResetCallback callback) override;
  void Cancel() override;
  std::string GetId() const override;
  std::string GetDisplayName() const override;
  ProtocolVersion SupportedProtocol() const override;
  bool SupportsHMACSecretExtension() const override;
  bool SupportsEnterpriseAttestation() const override;
  bool SupportsCredBlobOfSize(size_t num_bytes) const override;
  bool SupportsDevicePublicKey() const override;
  const absl::optional<AuthenticatorSupportedOptions>& Options() const override;
  absl::optional<FidoTransportProtocol> AuthenticatorTransport() const override;
  bool IsInPairingMode() const override;
  bool IsPaired() const override;
  bool RequiresBlePairingPin() const override;
  base::WeakPtr<FidoAuthenticator> GetWeakPtr() override;

  FidoDevice* device() { return device_.get(); }
  void SetTaskForTesting(std::unique_ptr<FidoTask> task);

 protected:
  void OnCtapMakeCredentialResponseReceived(
      MakeCredentialCallback callback,
      absl::optional<std::vector<uint8_t>> response_data);
  void OnCtapGetAssertionResponseReceived(
      GetAssertionCallback callback,
      absl::optional<std::vector<uint8_t>> response_data);

 private:
  using GetEphemeralKeyCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              absl::optional<pin::KeyAgreementResponse>)>;
  void InitializeAuthenticatorDone(base::OnceClosure callback);
  void GetEphemeralKey(GetEphemeralKeyCallback callback);
  void DoGetAssertion(CtapGetAssertionRequest request,
                      CtapGetAssertionOptions options,
                      GetAssertionCallback callback);
  void OnHaveEphemeralKeyForGetAssertion(
      CtapGetAssertionRequest request,
      CtapGetAssertionOptions options,
      GetAssertionCallback callback,
      CtapDeviceResponseCode status,
      absl::optional<pin::KeyAgreementResponse> key);
  void OnHaveEphemeralKeyForGetPINToken(
      std::string pin,
      std::vector<pin::Permissions> permissions,
      absl::optional<std::string> rp_id,
      GetTokenCallback callback,
      CtapDeviceResponseCode status,
      absl::optional<pin::KeyAgreementResponse> key);
  void OnHaveEphemeralKeyForSetPIN(
      std::string pin,
      SetPINCallback callback,
      CtapDeviceResponseCode status,
      absl::optional<pin::KeyAgreementResponse> key);
  void OnHaveEphemeralKeyForChangePIN(
      std::string old_pin,
      std::string new_pin,
      SetPINCallback callback,
      CtapDeviceResponseCode status,
      absl::optional<pin::KeyAgreementResponse> key);
  void OnHaveEphemeralKeyForUvToken(
      absl::optional<std::string> rp_id,
      std::vector<pin::Permissions> permissions,
      GetTokenCallback callback,
      CtapDeviceResponseCode status,
      absl::optional<pin::KeyAgreementResponse> key);

  void FetchLargeBlobArray(
      absl::optional<pin::TokenResponse> pin_uv_auth_token,
      LargeBlobArrayReader large_blob_array_reader,
      base::OnceCallback<void(CtapDeviceResponseCode,
                              absl::optional<LargeBlobArrayReader>)> callback);
  void WriteLargeBlobArray(
      absl::optional<pin::TokenResponse> pin_uv_auth_token,
      LargeBlobArrayWriter large_blob_array_writer,
      base::OnceCallback<void(CtapDeviceResponseCode)> callback);
  void OnReadLargeBlobFragment(
      const size_t bytes_requested,
      LargeBlobArrayReader large_blob_array_reader,
      absl::optional<pin::TokenResponse> pin_uv_auth_token,
      base::OnceCallback<void(CtapDeviceResponseCode,
                              absl::optional<LargeBlobArrayReader>)> callback,
      CtapDeviceResponseCode status,
      absl::optional<LargeBlobsResponse> response);
  void OnWriteLargeBlobFragment(
      LargeBlobArrayWriter large_blob_array_writer,
      absl::optional<pin::TokenResponse> pin_uv_auth_token,
      base::OnceCallback<void(CtapDeviceResponseCode)> callback,
      CtapDeviceResponseCode status,
      absl::optional<LargeBlobsResponse> response);
  void OnHaveLargeBlobArrayForWrite(
      LargeBlob large_blob,
      const LargeBlobKey& large_blob_key,
      absl::optional<pin::TokenResponse> pin_uv_auth_token,
      base::OnceCallback<void(CtapDeviceResponseCode)> callback,
      CtapDeviceResponseCode status,
      absl::optional<LargeBlobArrayReader> large_blob_array_reader);
  void OnHaveLargeBlobArrayForRead(
      const std::vector<LargeBlobKey>& large_blob_keys,
      LargeBlobReadCallback callback,
      CtapDeviceResponseCode status,
      absl::optional<LargeBlobArrayReader> large_blob_array_reader);

  template <typename... Args>
  void TaskClearProxy(base::OnceCallback<void(Args...)> callback, Args... args);
  template <typename... Args>
  void OperationClearProxy(base::OnceCallback<void(Args...)> callback,
                           Args... args);
  template <typename Task, typename Response, typename... RequestArgs>
  void RunTask(RequestArgs&&... request_args,
               base::OnceCallback<void(CtapDeviceResponseCode,
                                       absl::optional<Response>)> callback);
  template <typename Request, typename Response>
  void RunOperation(Request request,
                    base::OnceCallback<void(CtapDeviceResponseCode,
                                            absl::optional<Response>)> callback,
                    base::OnceCallback<absl::optional<Response>(
                        const absl::optional<cbor::Value>&)> parser,
                    bool (*string_fixup_predicate)(
                        const std::vector<const cbor::Value*>&) = nullptr);

  struct EnumerateCredentialsState;
  void OnEnumerateRPsDone(EnumerateCredentialsState state,
                          CtapDeviceResponseCode status,
                          absl::optional<EnumerateRPsResponse> response);
  void OnEnumerateCredentialsDone(
      EnumerateCredentialsState state,
      CtapDeviceResponseCode status,
      absl::optional<EnumerateCredentialsResponse> response);

  size_t max_large_blob_fragment_length();

  const std::unique_ptr<FidoDevice> device_;
  absl::optional<AuthenticatorSupportedOptions> options_;
  std::unique_ptr<FidoTask> task_;
  std::unique_ptr<GenericDeviceOperation> operation_;

  // The highest advertised PINUVAuthProtocol version that the authenticator
  // supports. This is guaranteed to be non-null after authenticator
  // initialization if |options_| indicates that PIN is supported.
  absl::optional<PINUVAuthProtocol> chosen_pin_uv_auth_protocol_;

  base::WeakPtrFactory<FidoDeviceAuthenticator> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_DEVICE_AUTHENTICATOR_H_
