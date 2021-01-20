// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_BIO_ENROLLMENT_HANDLER_H_
#define DEVICE_FIDO_BIO_ENROLLMENT_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "device/fido/bio/enroller.h"
#include "device/fido/bio/enrollment.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_request_handler_base.h"

namespace device {

enum class BioEnrollmentStatus {
  kSuccess,
  kAuthenticatorResponseInvalid,
  kSoftPINBlock,
  kHardPINBlock,
  kNoPINSet,
  kAuthenticatorMissingBioEnrollment,
  kForcePINChange,
};

// BioEnrollmentHandler exercises the CTAP2.1 authenticatorBioEnrollment
// sub-protocol for enrolling biometric templates on external authenticators
// supporting internal UV.
class COMPONENT_EXPORT(DEVICE_FIDO) BioEnrollmentHandler
    : public FidoRequestHandlerBase,
      public BioEnroller::Delegate {
 public:
  using TemplateId = std::vector<uint8_t>;

  using ErrorCallback = base::OnceCallback<void(BioEnrollmentStatus)>;
  using GetPINCallback =
      base::RepeatingCallback<void(uint32_t min_pin_length,
                                   int64_t retries,
                                   base::OnceCallback<void(std::string)>)>;
  using StatusCallback = base::OnceCallback<void(CtapDeviceResponseCode)>;
  using EnumerationCallback = base::OnceCallback<void(
      CtapDeviceResponseCode,
      base::Optional<std::map<TemplateId, std::string>>)>;
  using SampleCallback =
      base::RepeatingCallback<void(BioEnrollmentSampleStatus, uint8_t)>;
  using CompletionCallback =
      base::OnceCallback<void(CtapDeviceResponseCode, TemplateId)>;

  BioEnrollmentHandler(
      const base::flat_set<FidoTransportProtocol>& supported_transports,
      base::OnceClosure ready_callback,
      ErrorCallback error_callback,
      GetPINCallback get_pin_callback,
      FidoDiscoveryFactory* factory);
  ~BioEnrollmentHandler() override;
  BioEnrollmentHandler(const BioEnrollmentHandler&) = delete;
  BioEnrollmentHandler(BioEnrollmentHandler&&) = delete;

  // Enrolls a new fingerprint template. The user must provide the required
  // number of samples by touching the authenticator's sensor repeatedly. For
  // each sample, |sample_callback| is invoked with a status and the remaining
  // number of samples. Once all samples have been collected or the operation
  // has been cancelled, |completion_callback| is invoked with the operation
  // status.
  void EnrollTemplate(SampleCallback sample_callback,
                      CompletionCallback completion_callback);

  // Cancels an ongoing enrollment, if any, and invokes the
  // |completion_callback| passed to EnrollTemplate() with
  // |CtapDeviceResponseCode::kCtap2ErrKeepAliveCancel|.
  void CancelEnrollment();

  // Requests a map of current enrollments from the authenticator. On success,
  // the callback is invoked with a map from template IDs to human-readable
  // names. On failure, the callback is invoked with base::nullopt.
  void EnumerateTemplates(EnumerationCallback);

  // Renames the enrollment identified by |template_id| to |name|.
  void RenameTemplate(std::vector<uint8_t> template_id,
                      std::string name,
                      StatusCallback);

  // Deletes the enrollment identified by |template_id|.
  void DeleteTemplate(std::vector<uint8_t> template_id, StatusCallback);

 private:
  enum class State {
    kWaitingForTouch,
    kGettingRetries,
    kWaitingForPIN,
    kGettingPINToken,
    kReady,
    kEnrolling,
    kCancellingEnrollment,
    kEnumerating,
    kRenaming,
    kDeleting,
    kFinished,
  };

  // FidoRequestHandlerBase:
  void DispatchRequest(FidoAuthenticator*) override;
  void AuthenticatorRemoved(FidoDiscoveryBase*, FidoAuthenticator*) override;

  // BioEnroller::Delegate:
  void OnSampleCollected(BioEnrollmentSampleStatus status,
                         int samples_remaining) override;
  void OnEnrollmentDone(
      base::Optional<std::vector<uint8_t>> template_id) override;
  void OnEnrollmentError(CtapDeviceResponseCode status) override;

  void OnTouch(FidoAuthenticator* authenticator);
  void OnRetriesResponse(CtapDeviceResponseCode,
                         base::Optional<pin::RetriesResponse>);
  void OnHavePIN(std::string pin);
  void OnHavePINToken(CtapDeviceResponseCode,
                      base::Optional<pin::TokenResponse>);
  void OnEnumerateTemplates(EnumerationCallback,
                            CtapDeviceResponseCode,
                            base::Optional<BioEnrollmentResponse>);
  void OnRenameTemplate(StatusCallback,
                        CtapDeviceResponseCode,
                        base::Optional<BioEnrollmentResponse>);
  void OnDeleteTemplate(StatusCallback,
                        CtapDeviceResponseCode,
                        base::Optional<BioEnrollmentResponse>);

  void Finish(BioEnrollmentStatus status);

  SEQUENCE_CHECKER(sequence_checker_);

  State state_ = State::kWaitingForTouch;

  FidoAuthenticator* authenticator_ = nullptr;
  std::unique_ptr<BioEnroller> bio_enroller_;
  base::OnceClosure ready_callback_;
  ErrorCallback error_callback_;
  GetPINCallback get_pin_callback_;
  CompletionCallback enrollment_callback_;
  SampleCallback sample_callback_;
  base::Optional<pin::TokenResponse> pin_token_response_;
  base::WeakPtrFactory<BioEnrollmentHandler> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_BIO_ENROLLMENT_HANDLER_H_
