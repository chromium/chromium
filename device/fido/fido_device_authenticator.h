// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_DEVICE_AUTHENTICATOR_H_
#define DEVICE_FIDO_FIDO_DEVICE_AUTHENTICATOR_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/device_operation.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/large_blob.h"
#include "device/fido/pin.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace device {

struct CtapGetAssertionRequest;
struct CtapGetAssertionOptions;
struct CtapMakeCredentialRequest;
struct EnumerateRPsResponse;
class FidoDevice;
class FidoTask;
class GenericDeviceOperation;

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
      base::OnceCallback<void(CtapDeviceResponseCode, std::optional<bool>)>)
      override;
  void MakeCredential(CtapMakeCredentialRequest request,
                      MakeCredentialOptions options,
                      MakeCredentialCallback callback) override;
  void GetAssertion(CtapGetAssertionRequest request,
                    CtapGetAssertionOptions options,
                    GetAssertionCallback callback) override;
  void GetTouch(base::OnceClosure callback) override;
  void GetPinRetries(GetRetriesCallback callback) override;
  void GetPINToken(std::string pin,
                   std::vector<pin::Permissions> permissions,
                   std::optional<std::string> rp_id,
                   GetTokenCallback callback) override;
  void GetUvRetries(GetRetriesCallback callback) override;
  bool CanGetUvToken() override;
  void GetUvToken(std::vector<pin::Permissions> permissions,
                  std::optional<std::string> rp_id,
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
                            std::optional<std::vector<uint8_t>> template_id,
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
  void GarbageCollectLargeBlob(
      const pin::TokenResponse& pin_uv_auth_token,
      base::OnceCallback<void(CtapDeviceResponseCode)> callback) override;

  std::optional<base::span<const int32_t>> GetAlgorithms() override;
  bool DiscoverableCredentialStorageFull() const override;

  void Reset(ResetCallback callback) override;
  void Cancel() override;
  AuthenticatorType GetType() const override;
  cablev2::FidoTunnelDevice* GetTunnelDevice() override;
  std::string GetId() const override;
  std::string GetDisplayName() const override;
  ProtocolVersion SupportedProtocol() const override;
  const AuthenticatorSupportedOptions& Options() const override;
  std::optional<FidoTransportProtocol> AuthenticatorTransport() const override;
  base::WeakPtr<FidoAuthenticator> GetWeakPtr() override;

  FidoDevice* device() { return device_.get(); }
  void SetTaskForTesting(std::unique_ptr<FidoTask> task);

 private:
  using GetEphemeralKeyCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              std::optional<pin::KeyAgreementResponse>)>;
  using LargeBlobReadCallback = base::OnceCallback<void(
      CtapDeviceResponseCode,
      std::optional<std::vector<std::pair<LargeBlobKey, LargeBlob>>> callback)>;
  using CtapGetAssertionCallback =
      base::OnceCallback<void(CtapDeviceResponseCode,
                              std::vector<AuthenticatorGetAssertionResponse>)>;
  using CtapMakeCredentialCallback = base::OnceCallback<void(
      CtapDeviceResponseCode,
      std::optional<AuthenticatorMakeCredentialResponse>)>;
  void InitializeAuthenticatorDone(base::OnceClosure callback);
  void GetEphemeralKey(GetEphemeralKeyCallback callback);
  void DoGetAssertion(CtapGetAssertionRequest request,
                      CtapGetAssertionOptions options,
                      CtapGetAssertionCallback callback);
  void OnHaveCompressedLargeBlobForGetAssertion(
      CtapGetAssertionRequest request,
      CtapGetAssertionOptions options,
      CtapGetAssertionCallback callback,
      size_t original_size,
      base::expected<mojo_base::BigBuffer, std::string> result);
  void MaybeGetEphemeralKeyForGetAssertion(CtapGetAssertionRequest request,
                                           CtapGetAssertionOptions options,
                                           CtapGetAssertionCallback callback);
  void OnHaveAssertion(
      CtapGetAssertionRequest request,
      CtapGetAssertionOptions options,
      CtapGetAssertionCallback callback,
      CtapDeviceResponseCode status,
      std::vector<AuthenticatorGetAssertionResponse> responses);
  void PerformGetAssertionLargeBlobOperation(
      CtapGetAssertionRequest request,
      CtapGetAssertionOptions options,
      std::vector<AuthenticatorGetAssertionResponse> responses,
      CtapGetAssertionCallback callback);
  void OnHaveEphemeralKeyForGetAssertion(
      CtapGetAssertionRequest request,
      CtapGetAssertionOptions options,
      CtapGetAssertionCallback callback,
      CtapDeviceResponseCode status,
      std::optional<pin::KeyAgreementResponse> key);
  void OnHaveEphemeralKeyForGetPINToken(
      std::string pin,
      std::vector<pin::Permissions> permissions,
      std::optional<std::string> rp_id,
      GetTokenCallback callback,
      CtapDeviceResponseCode status,
      std::optional<pin::KeyAgreementResponse> key);
  void OnHaveEphemeralKeyForSetPIN(
      std::string pin,
      SetPINCallback callback,
      CtapDeviceResponseCode status,
      std::optional<pin::KeyAgreementResponse> key);
  void OnHaveEphemeralKeyForChangePIN(
      std::string old_pin,
      std::string new_pin,
      SetPINCallback callback,
      CtapDeviceResponseCode status,
      std::optional<pin::KeyAgreementResponse> key);
  void OnHaveEphemeralKeyForUvToken(
      std::optional<std::string> rp_id,
      std::vector<pin::Permissions> permissions,
      GetTokenCallback callback,
      CtapDeviceResponseCode status,
      std::optional<pin::KeyAgreementResponse> key);
  void OnGetAssertionResponse(
      GetAssertionCallback callback,
      CtapDeviceResponseCode status,
      std::vector<AuthenticatorGetAssertionResponse> responses);
  void MakeCredentialInternal(CtapMakeCredentialRequest request,
                              MakeCredentialOptions request_options,
                              CtapMakeCredentialCallback callback);
  void OnMakeCredentialResponse(
      MakeCredentialCallback callback,
      CtapDeviceResponseCode status,
      std::optional<AuthenticatorMakeCredentialResponse> response);

  // Attempts to read large blobs from the credential encrypted with
  // |large_blob_keys|. Returns a map of keys to their blobs.
  void ReadLargeBlob(const std::vector<LargeBlobKey>& large_blob_keys,
                     LargeBlobReadCallback callback);

  void FetchLargeBlobArray(
      LargeBlobArrayReader large_blob_array_reader,
      base::OnceCallback<void(CtapDeviceResponseCode,
                              std::optional<LargeBlobArrayReader>)> callback);
  void WriteLargeBlobArray(
      std::optional<pin::TokenResponse> pin_uv_auth_token,
      LargeBlobArrayWriter large_blob_array_writer,
      base::OnceCallback<void(CtapDeviceResponseCode)> callback);
  void OnReadLargeBlobFragment(
      const size_t bytes_requested,
      LargeBlobArrayReader large_blob_array_reader,
      base::OnceCallback<void(CtapDeviceResponseCode,
                              std::optional<LargeBlobArrayReader>)> callback,
      CtapDeviceResponseCode status,
      std::optional<LargeBlobsResponse> response);
  void OnWriteLargeBlobFragment(
      LargeBlobArrayWriter large_blob_array_writer,
      std::optional<pin::TokenResponse> pin_uv_auth_token,
      base::OnceCallback<void(CtapDeviceResponseCode)> callback,
      CtapDeviceResponseCode status,
      std::optional<LargeBlobsResponse> response);
  void OnWroteLargeBlobForGetAssertion(
      std::vector<AuthenticatorGetAssertionResponse> responses,
      CtapGetAssertionCallback callback,
      CtapDeviceResponseCode status);
  void OnReadLargeBlobForGetAssertion(
      std::vector<AuthenticatorGetAssertionResponse> responses,
      CtapGetAssertionCallback callback,
      CtapDeviceResponseCode status,
      std::optional<std::vector<std::pair<LargeBlobKey, LargeBlob>>> blobs);
  void OnBlobUncompressed(
      std::vector<AuthenticatorGetAssertionResponse> responses,
      std::vector<std::pair<LargeBlobKey, LargeBlob>> blobs,
      LargeBlobKey uncompressed_key,
      CtapGetAssertionCallback callback,
      base::expected<mojo_base::BigBuffer, std::string> result);
  void OnLargeBlobExtensionUncompressed(
      std::vector<AuthenticatorGetAssertionResponse> responses,
      CtapGetAssertionCallback callback,
      base::expected<mojo_base::BigBuffer, std::string> result);
  void OnCredentialsEnumeratedForGarbageCollect(
      const pin::TokenResponse& pin_uv_auth_token,
      base::OnceCallback<void(CtapDeviceResponseCode)> callback,
      CtapDeviceResponseCode status,
      std::optional<std::vector<AggregatedEnumerateCredentialsResponse>>
          credentials);
  void OnHaveLargeBlobArrayForWrite(
      const LargeBlobKey& large_blob_key,
      std::optional<pin::TokenResponse> pin_uv_auth_token,
      base::OnceCallback<void(CtapDeviceResponseCode)> callback,
      CtapDeviceResponseCode status,
      std::optional<LargeBlobArrayReader> large_blob_array_reader);
  void OnHaveLargeBlobArrayForRead(
      const std::vector<LargeBlobKey>& large_blob_keys,
      LargeBlobReadCallback callback,
      CtapDeviceResponseCode status,
      std::optional<LargeBlobArrayReader> large_blob_array_reader);
  void OnHaveLargeBlobArrayForGarbageCollect(
      std::vector<AggregatedEnumerateCredentialsResponse> credentials,
      const pin::TokenResponse& pin_uv_auth_token,
      base::OnceCallback<void(CtapDeviceResponseCode)> callback,
      CtapDeviceResponseCode status,
      std::optional<LargeBlobArrayReader> large_blob_array_reader);

  template <typename... Args>
  void TaskClearProxy(base::OnceCallback<void(Args...)> callback, Args... args);
  template <typename... Args>
  void OperationClearProxy(base::OnceCallback<void(Args...)> callback,
                           Args... args);
  template <typename Task, typename Response, typename... RequestArgs>
  void RunTask(
      RequestArgs&&... request_args,
      base::OnceCallback<void(CtapDeviceResponseCode, Response)> callback);
  template <typename Request, typename Response>
  void RunOperation(Request request,
                    base::OnceCallback<void(CtapDeviceResponseCode,
                                            std::optional<Response>)> callback,
                    base::OnceCallback<std::optional<Response>(
                        const std::optional<cbor::Value>&)> parser,
                    bool (*string_fixup_predicate)(
                        const std::vector<const cbor::Value*>&) = nullptr);

  struct EnumerateCredentialsState;
  void OnEnumerateRPsDone(EnumerateCredentialsState state,
                          CtapDeviceResponseCode status,
                          std::optional<EnumerateRPsResponse> response);
  void OnEnumerateCredentialsDone(
      EnumerateCredentialsState state,
      CtapDeviceResponseCode status,
      std::optional<EnumerateCredentialsResponse> response);

  size_t max_large_blob_fragment_length();

  const std::unique_ptr<FidoDevice> device_;
  AuthenticatorSupportedOptions options_;
  std::unique_ptr<FidoTask> task_;
  std::unique_ptr<GenericDeviceOperation> operation_;
  bool initialized_ = false;

  // The highest advertised PINUVAuthProtocol version that the authenticator
  // supports. This is guaranteed to be non-null after authenticator
  // initialization if |options_| indicates that PIN is supported.
  std::optional<PINUVAuthProtocol> chosen_pin_uv_auth_protocol_;

  data_decoder::DataDecoder data_decoder_;
  // large_blob_ contains a compressed largeBlob and indicates that an
  // largeBlobKey-based write will occur in a `GetAssertion` operation.
  std::optional<LargeBlob> large_blob_;
  // large_blob_read_ indicates that a largeBlobKey-based read will occur
  // in a `GetAssertion` operation.
  bool large_blob_read_;

  base::WeakPtrFactory<FidoDeviceAuthenticator> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_DEVICE_AUTHENTICATOR_H_
