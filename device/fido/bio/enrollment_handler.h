// Copyright 2019 The Chromium Authors
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
#include "base/memory/raw_ptr.h"
#include "device/fido/bio/enroller.h"
#include "device/fido/bio/enrollment.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_request_handler_base.h"

namespace device {

// BioEnrollmentHandler exercises the CTAP2.1 authenticatorBioEnrollment
// sub-protocol for enrolling biometric templates on external authenticators
// supporting internal UV.
class COMPONENT_EXPORT(DEVICE_FIDO) BioEnrollmentHandler
    : public FidoRequestHandlerBase,
      public BioEnroller::Delegate {
 public:
  enum class Error {
    kAuthenticatorRemoved,
    kAuthenticatorResponseInvalid,
    kSoftPINBlock,
    kHardPINBlock,
    kNoPINSet,
    kAuthenticatorMissingBioEnrollment,
    kForcePINChange,
  };

  struct COMPONENT_EXPORT(DEVICE_FIDO) SensorInfo {
    SensorInfo();
    SensorInfo(const SensorInfo&) = delete;
    SensorInfo(SensorInfo&&);
    SensorInfo& operator=(const SensorInfo&) = delete;
    SensorInfo& operator=(SensorInfo&&);

    std::optional<uint8_t> max_samples_for_enroll;
    uint32_t max_template_friendly_name;
  };

  using TemplateId = std::vector<uint8_t>;

  // ReadyCallback is invoked once the handler has completed initialization for
  // the authenticator, i.e. obtained a PIN/UV token and read |SensorInfo|.
  // Methods on the handler may be invoked at that point.
  using ReadyCallback = base::OnceCallback<void(SensorInfo)>;

  // ErrorCallback is invoked if the handler has encountered an error. No
  // further methods may be called on the handler be made at that point.
  using ErrorCallback = base::OnceCallback<void(Error)>;

  // GetPINCallback is invoked to obtain a PIN for the authenticator.
  using GetPINCallback =
      base::RepeatingCallback<void(uint32_t min_pin_length,
                                   int64_t retries,
                                   base::OnceCallback<void(std::string)>)>;

  // StatusCallback provides the CTAP response code for an operation.
  using StatusCallback = base::OnceCallback<void(CtapDeviceResponseCode)>;

  // EnumerationCallback is invoked upon the completion of EnumerateTemplates.
  using EnumerationCallback = base::OnceCallback<void(
      CtapDeviceResponseCode,
      std::optional<std::map<TemplateId, std::string>>)>;

  // SampleCallback is invoked repeatedly during an ongoing EnrollTemplate()
  // operation to indicate the status of a single sample collection (i.e. user
  // touched the sensor). It receives a status value and the number of remaining
  // samples to be collected.
  using SampleCallback =
      base::RepeatingCallback<void(BioEnrollmentSampleStatus, uint8_t)>;

  // EnrollmentCallback is invoked upon completion of EnrollTemplate().
  using EnrollmentCallback =
      base::OnceCallback<void(CtapDeviceResponseCode, TemplateId)>;

  BioEnrollmentHandler(
      const base::flat_set<FidoTransportProtocol>& supported_transports,
      ReadyCallback ready_callback,
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
  // has been cancelled, |enrollment_callback| is invoked with the result.
  void EnrollTemplate(SampleCallback sample_callback,
                      EnrollmentCallback enrollment_callback);

  // Cancels an ongoing enrollment, if any, and invokes the
  // |completion_callback| passed to EnrollTemplate() with
  // |CtapDeviceResponseCode::kCtap2ErrKeepAliveCancel|.
  void CancelEnrollment();

  // Requests a map of current enrollments from the authenticator. On success,
  // the callback is invoked with a map from template IDs to human-readable
  // names. On failure, the callback is invoked with std::nullopt.
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
    kGettingSensorInfo,
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
      std::optional<std::vector<uint8_t>> template_id) override;
  void OnEnrollmentError(CtapDeviceResponseCode status) override;

  void OnTouch(FidoAuthenticator* authenticator);
  void OnRetriesResponse(CtapDeviceResponseCode,
                         std::optional<pin::RetriesResponse>);
  void OnHavePIN(std::string pin);
  void OnHavePINToken(CtapDeviceResponseCode,
                      std::optional<pin::TokenResponse>);
  void OnGetSensorInfo(CtapDeviceResponseCode,
                       std::optional<BioEnrollmentResponse>);
  void OnEnumerateTemplates(EnumerationCallback,
                            CtapDeviceResponseCode,
                            std::optional<BioEnrollmentResponse>);
  void OnRenameTemplate(StatusCallback,
                        CtapDeviceResponseCode,
                        std::optional<BioEnrollmentResponse>);
  void OnDeleteTemplate(StatusCallback,
                        CtapDeviceResponseCode,
                        std::optional<BioEnrollmentResponse>);

  void RunErrorCallback(Error error);

  SEQUENCE_CHECKER(sequence_checker_);

  State state_ = State::kWaitingForTouch;

  raw_ptr<FidoAuthenticator> authenticator_ = nullptr;
  std::unique_ptr<BioEnroller> bio_enroller_;
  ReadyCallback ready_callback_;
  ErrorCallback error_callback_;
  GetPINCallback get_pin_callback_;
  EnrollmentCallback enrollment_callback_;
  SampleCallback sample_callback_;
  std::optional<pin::TokenResponse> pin_token_response_;
  base::WeakPtrFactory<BioEnrollmentHandler> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_BIO_ENROLLMENT_HANDLER_H_
